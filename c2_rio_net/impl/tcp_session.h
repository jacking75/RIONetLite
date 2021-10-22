#pragma once

#include <vector>

#include "net_sender_immediately.h"
#include "ring_buffer.h"
#include "fast_spin_lock.h"
#include "rio_context.h"

namespace c2rionet
{
	class TcpSession
	{
		const INT64 CONNECTED = 1;
		const INT64 ENABLE_VALUE = 1;

	public:
		TcpSession() = default;
		~TcpSession() = default;
		
		int GetIndex() { return index_; }

		UINT64 GetUniqueId() { return mUnqiueId; }

		bool IsEnableNetwork() { return enable_network_ == ENABLE_VALUE; }
		void SetDisableNetwork() { enable_network_ = 0; }
		
		void Init(int index, int threadId)
		{
			rio_thread_id_ = threadId;
			index_ = index;
			memset(&client_addr_, 0, sizeof(SOCKADDR_IN));
		}

		bool RioInitialize(ConnectionNetConfig net_config)
		{
			max_try_1recv_buffer_size = net_config.max_try_1recv_buffer_size;
			io_sender_.Init(index_, net_config);
			
			if (auto result = recv_buffer_.Create(false, net_config.max_recv_buffer_size_);
				result != 0)
			{
				LogFuncPtr((int)LogLevel::kERROR, "Session::RioInitialize() - fail mRecvBuffer.Create");
				return false;
			}

			recv_rio_buffer_id_ = RIO.RIORegisterBuffer(recv_buffer_.GetBuffer(), recv_buffer_.EnableBufferSize());
			if (recv_rio_buffer_id_ == RIO_INVALID_BUFFERID)
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "Session::RioInitialize() - RIORegisterBuffer Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::kERROR, logmsg);

				return false;
			}

			recv_context_.Init(index_, IOType::kRECV);
			return true;
		}

		void Dispose()
		{
			if (recv_rio_buffer_id_ != NULL) { RIO.RIODeregisterBuffer(recv_rio_buffer_id_); }
			io_sender_.Dispose();
		}

		bool OnConnect(UINT64 unique_id, SOCKET socket, RIO_RQ rio_rq)
		{
			// 세션 객체가 재 사용되더라도 대기 시간이 있으므로 연결이 끊어질 때와 이 함수 호출이 겹쳐지지 않는다.
			SetAddress(socket);
									
			/// turn off nagle
			int opt = 1;
			setsockopt(socket_, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int));
			
			socket_ = socket;
			mUnqiueId = unique_id;
			connected_ = CONNECTED;
			enable_network_ = ENABLE_VALUE;
			rio_rq_ = rio_rq;
			io_sender_.OnConnect(rio_rq_);
			recv_buffer_.Clear();

			return PostRecv() == NetErrorCode::kNONE;
		}
		
		bool IsConnected() const { return connected_ == CONNECTED; }

		int	GetRioThreadId() const { return rio_thread_id_; }

		bool PostSend(const UINT32 data_size, const char* p_data)
		{
			FastSpinlockGuard criticalSection(session_lock_);

			auto result = io_sender_.PostSend(GetUniqueId(), data_size, p_data);
			return result;
		}

		void SendCompletion(RioIoContext* p_send_context)
		{
			FastSpinlockGuard criticalSection(session_lock_);
			io_sender_.Completed(p_send_context);
		}

		NetErrorCode PostRecv()
		{
			FastSpinlockGuard criticalSection(session_lock_);

			if (IsConnected() == false) { return NetErrorCode::kRECV_CLOSED_STATUS; }

			if (IsEnableNetwork() == false) { return NetErrorCode::kRECV_DISABLE_NETWORK_STATUS; }

			recv_context_.session_unique_Id_ = GetUniqueId();
			recv_context_.BufferId = recv_rio_buffer_id_;
			recv_context_.Length = max_try_1recv_buffer_size;
			recv_context_.Offset = recv_buffer_.GetWriteablePos();
						
			/// start async recv
			DWORD flags = 0;
			if (!RIO.RIOReceive(rio_rq_, (PRIO_BUF)&recv_context_, 1, flags, &recv_context_))
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "Session::PostRecv() - RIOReceive Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::kERROR, logmsg);

				return NetErrorCode::kRECV_RIO_FAIL;
			}

			return NetErrorCode::kNONE;
		}
		

		void RecvCompletion(UINT32 size) { recv_buffer_.Write(size); }

		std::tuple<UINT32, char*> ReadRecvBuffer() { return recv_buffer_.Read(); }

		void IncremantReadPosRecvBuffer(const UINT32 size) { recv_buffer_.IncremantReadPos(size); }

		
		bool Disconnect(const UINT64 session_uniqueId_by_context)
		{
			if (IsConnected() == false) { return false; }

			FastSpinlockGuard criticalSection(session_lock_);

			if (GetUniqueId() != session_uniqueId_by_context) { return false; }

			LINGER lingerOption;
			lingerOption.l_onoff = 1;
			lingerOption.l_linger = 0;

			/// no TCP TIME_WAIT
			setsockopt(socket_, SOL_SOCKET, SO_LINGER, (char*)&lingerOption, sizeof(LINGER));
						
			closesocket(socket_);
			socket_ = NULL;
			connected_ = 0;
			SetDisableNetwork();
			mUnqiueId = 0;
					
			return true;
		}

		void DisConnectByAcceptFail()
		{
			closesocket(socket_);
			socket_ = NULL;
			connected_ = 0;
			SetDisableNetwork();
			mUnqiueId = 0;
		}
				

	private:
		void SetAddress(SOCKET socket)
		{
			SOCKADDR_IN clientaddr;
			int addrlen = sizeof(clientaddr);
			getpeername(socket, (SOCKADDR*)&clientaddr, &addrlen);
			memcpy(&client_addr_, &clientaddr, sizeof(SOCKADDR_IN));
		}



		
		FastSpinlock session_lock_;

		int				index_ = -1;
		UINT32			max_try_1recv_buffer_size = 0;
		int				rio_thread_id_ = -1;
		RIO_BUFFERID	recv_rio_buffer_id_ = NULL;

		UINT64			mUnqiueId = 0;
		
		// 64비트에서 INT64는 아토믹하게 읽기,쓰기 가능. ++i 가 아닌 i = 1 등이 아토믹하다는 것이다.
		// https://docs.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access
		INT64			connected_ = 0;
		
		INT64			enable_network_ = 0;
		
		SOCKET			socket_ = INVALID_SOCKET;
		SOCKADDR_IN		client_addr_;
		
		RIO_RQ			rio_rq_ = NULL;
		RioIoContext recv_context_;
				
		NetSenderImmediately io_sender_;

		RingBufferVA2 recv_buffer_;
	};

}