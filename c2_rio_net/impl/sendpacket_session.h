#pragma once

#include "sendpacket_buffer.h"

#include "fast_spin_lock.h"
#include "rio_context.h"
#include "common_def.h"


namespace c2rionet
{
	struct NetSendConfig
	{
		UINT32 max_session_count_ = 0;
		UINT32 max_worker_thread_count_ = 0;
		UINT32 max_send_packet_size_ = 0;
		UINT32 max_send_count_per_1sec_ = 0;  // 정한 것 보다 조금 더 넉넉하게 주는 것을 추천
		UINT32 max_send_buffer_size_ = 0;

	};

	// 보낼 데이터를 모아서 주기적으로 한번에 보내기
	class SendSession
	{
	public:
		SendSession() = default;
		~SendSession() = default;

		char* GetBuffer() { return send_buffer_.GetBuffer(); }

		bool IsConnected() { return is_connected_; }

		bool Init(const INT32 sessionIndex_, NetSendConfig netConfig_)
		{
			session_index_ = sessionIndex_;
			max_send_packet_size_ = netConfig_.max_send_packet_size_;
			max_send_count_per_1sec_ = netConfig_.max_send_count_per_1sec_;
			max_send_ring_buffer_size_ = netConfig_.max_send_buffer_size_;

			if (auto isResult = send_buffer_.Create(max_send_ring_buffer_size_);
				isResult != 0)
			{
				LogFuncPtr((int)LogLevel::kERROR, "Session::Init() - fail mSendBuffer.Create");
				return false;
			}

			sendContext_.Init(session_index_, IOType::kSEND);
			
			send_rio_buffer_id_ = RIO.RIORegisterBuffer(send_buffer_.GetBuffer(), send_buffer_.EnableBufferSize());
			if (send_rio_buffer_id_ == RIO_INVALID_BUFFERID)
			{
				char logmsg[128] = { 0, };
				sprintf_s(logmsg, "Session::Init() - RIORegisterBuffer Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::kERROR, logmsg);
				return false;
			}

			Clear();

			return true;
		}

		void Dispose()
		{
			RIO.RIODeregisterBuffer(send_rio_buffer_id_);			
		}

		void OnConnect(RIO_RQ requestQueue_)
		{
			is_connected_ = true;
			rio_rq_ = requestQueue_;
			Clear();
		}

		void OnDisConnect()
		{
			is_connected_ = false;
		}

		bool PostSend(const UINT32 dataSize_, const char* pData_)
		{
			FastSpinlockGuard criticalSection(session_lock_);
			if (send_buffer_.Write(dataSize_, pData_) == false) { return false; }
			return true;
		}

		void SendCompleted(const UINT32 length_)
		{
			FastSpinlockGuard criticalSection(session_lock_);

			send_buffer_.IncremantReadPos(length_);
			send_buffer_.MoveWrittenDataForward();
		}


		bool RealSend()
		{
			FastSpinlockGuard criticalSection(session_lock_);

			auto sendSize = send_buffer_.SendAbleSize();

			if (sendSize <= 0) { 
				return true; 
			}

			if (sendSize > max_send_packet_size_) { sendSize = max_send_packet_size_;	}
			
			sendContext_.session_index_ = session_index_;
			sendContext_.BufferId = send_rio_buffer_id_;
			sendContext_.Length = sendSize;
			sendContext_.Offset = send_buffer_.GetReadPos();
			
			/// start async send
			DWORD flags = 0;
			if (!RIO.RIOSend(rio_rq_, (PRIO_BUF)&sendContext_, 1, flags, &sendContext_))
			{
				char logmsg[128] = { 0, };
				sprintf_s(logmsg, "Session::RealSend() - RIOSend Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::kERROR, logmsg);
				return false;
			}

			return true;
		}
		

	private:
		void Clear()
		{
			send_buffer_.Clear();
		}

		bool is_connected_ = false;
		INT32 session_index_ = 0;
		INT32 max_send_packet_size_ = 0;
		UINT64 max_send_count_per_1sec_ = 0;
		INT32 max_send_ring_buffer_size_ = 0;

		RioIoContext sendContext_;

		RIO_RQ			rio_rq_ = NULL;
		RIO_BUFFERID	send_rio_buffer_id_ = NULL;

		FastSpinlock session_lock_;
		SendRingBuffer send_buffer_;
	};
}