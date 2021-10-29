#pragma once

#include "send_ring_buffer.h"
#include "fast_spin_lock.h"
#include "rio_context.h"
#include "../define.h"

#include <cstdint>


namespace c2rionet
{
	// 보낼 데이터를 모아서 주기적으로 한번에 보내기
	class SendSession
	{
	public:
		SendSession() = default;
		~SendSession() = default;

		char* GetBuffer() { return send_buffer_.GetBuffer(); }

		bool IsConnected() { return is_connected_; }

		bool Init(const std::int32_t session_index, NetSendConfig net_cfg)
		{
			session_index_ = session_index;
			max_send_packet_size_ = net_cfg.max_send_packet_size_;
			max_send_count_per_1sec_ = net_cfg.max_send_count_per_1sec_;
			max_send_ring_buffer_size_ = net_cfg.max_send_buffer_size_;

			if (auto isResult = send_buffer_.Create(max_send_ring_buffer_size_);
						isResult != 0) {
				LogFuncPtr((int)LogLevel::kERROR, "SendSession::Init() - fail mSendBuffer.Create");
				return false;
			}

			sendContext_.Init(session_index_, IOType::kSEND);
			
			send_rio_buffer_id_ = RIO.RIORegisterBuffer(send_buffer_.GetBuffer(), send_buffer_.EnableBufferSize());
			if (send_rio_buffer_id_ == RIO_INVALID_BUFFERID) {
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

		void OnConnect(RIO_RQ request_queue)
		{
			is_connected_ = true;
			ref_rio_rq_ = request_queue;
			Clear();
		}

		void OnDisConnect()
		{
			is_connected_ = false;
		}

		bool PostSend(const std::uint32_t data_size, const char* p_data)
		{
			FastSpinlockGuard criticalSection(session_lock_);
			
			if (send_buffer_.Write(data_size, p_data) == false) { 
				return false; 
			}
			
			return true;
		}

		void SendCompleted(const std::uint32_t length)
		{
			FastSpinlockGuard criticalSection(session_lock_);

			send_buffer_.IncremantReadPos(length);
			send_buffer_.MoveWrittenDataForward();
		}


		bool RealSend()
		{
			FastSpinlockGuard criticalSection(session_lock_);

			auto sendSize = send_buffer_.SendAbleSize();

			if (sendSize <= 0) { 
				return true; 
			}

			if (sendSize > (uint32_t)max_send_packet_size_) { 
				sendSize = max_send_packet_size_;	
			}
			
			sendContext_.session_index_ = session_index_;
			sendContext_.BufferId = send_rio_buffer_id_;
			sendContext_.Length = sendSize;
			sendContext_.Offset = send_buffer_.GetReadPos();
			
			/// start async send
			DWORD flags = 0;
			if (!RIO.RIOSend(ref_rio_rq_, (PRIO_BUF)&sendContext_, 1, flags, &sendContext_)) {
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
		std::int32_t session_index_ = 0;
		std::int32_t max_send_packet_size_ = 0;
		std::uint64_t max_send_count_per_1sec_ = 0;
		std::int32_t max_send_ring_buffer_size_ = 0;

		RioIoContext sendContext_;

		RIO_RQ			ref_rio_rq_ = NULL;
		RIO_BUFFERID	send_rio_buffer_id_ = NULL;

		FastSpinlock session_lock_;
		SendRingBuffer send_buffer_;
	};
}