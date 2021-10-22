#pragma once

#include <vector>
#include <queue>

#include "ring_buffer.h"
#include "fast_spin_lock.h"
#include "rio_context.h"
#include "define.h"

namespace c2rionet
{
	// 스레드 세이프 하지 않다 !!!
	class NetSenderImmediately
	{
	public:
		NetSenderImmediately() = default;

		~NetSenderImmediately() = default;
		
		char* GetBuffer() { return send_buffer_.GetBuffer(); }

		bool Init(const INT32 sessionIndex, ConnectionNetConfig netConfig)
		{
			session_index_ = sessionIndex;
			max_send_packet_size_ = netConfig.max_send_packet_size_;
			max_send_count_per_1sec_ = netConfig.max_send_count_per_1sec_;
			max_send_ring_buffer_size_ = netConfig.max_send_buffer_size_;
			
			if (auto isResult = send_buffer_.Create(true, max_send_ring_buffer_size_); 
				isResult != 0)
			{
				LogFuncPtr((int)LogLevel::kERROR, "NetSenderImmediately::Init() - fail mSendBuffer.Create");
				return false;
			}

			for (int i = 0; i < max_send_count_per_1sec_; ++i)
			{
				auto context = new RioIoContext;
				context->Init(session_index_, IOType::kSEND);

				send_contexts_.push_back(context);
			}

			send_rio_buffer_id_ = RIO.RIORegisterBuffer(send_buffer_.GetBuffer(), send_buffer_.EnableBufferSize());
			if (send_rio_buffer_id_ == RIO_INVALID_BUFFERID)
			{
				char logmsg[512] = { 0, };
				sprintf_s(logmsg, "NetSenderImmediately::Init() - RIORegisterBuffer Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::kERROR, logmsg);
				return false;
			}

			Clear();

			return true;
		}

		void Dispose()
		{
			RIO.RIODeregisterBuffer(send_rio_buffer_id_);

			for (int i = 0; i < max_send_count_per_1sec_; ++i)
			{
				delete send_contexts_[i];
			}
		}

		void OnConnect(RIO_RQ requestQueue)
		{
			rio_rq_ = requestQueue;
			Clear();
		}
				
		bool PostSend(const UINT64 sessionUniqueId, const UINT32 dataSize, const char* pData)
		{
			auto curPos = send_sequence_ % max_send_count_per_1sec_;
			InterlockedIncrement(&send_sequence_);
			RioIoContext* pSendContext = send_contexts_[curPos];
			
			if (pSendContext->Length != 0) 
			{ 
				LogFuncPtr((int)LogLevel::kERROR, "NetSenderImmediately::PostSend() - empty Context");
				return false; 
			}

			auto writeAblePos = send_buffer_.GetWriteablePos();
			 send_buffer_.Write(dataSize, pData);
			
			pSendContext->session_unique_Id_ = sessionUniqueId;
			pSendContext->BufferId = send_rio_buffer_id_;
			pSendContext->Length = dataSize;
			pSendContext->Offset = writeAblePos;
					
			
			/// start async send
			DWORD flags = 0;
			if (!RIO.RIOSend(rio_rq_, (PRIO_BUF)pSendContext, 1, flags, pSendContext))
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "NetSenderImmediately::PostSend() - RIOSend Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::kERROR, logmsg);

				Completed(pSendContext);
				return false;
			}

			return true;
		}
				
		void Completed(RioIoContext* pSendContext)
		{
			pSendContext->Length = 0;
		}


	private:
		void Clear()
		{
			for (auto pContext : send_contexts_)
			{
				pContext->Length = 0;
			}

			send_buffer_.Clear();
		}


		INT32 session_index_ = 0;
		INT32 max_send_packet_size_ = 0;
		UINT64 max_send_count_per_1sec_ = 0;
		INT32 max_send_ring_buffer_size_ = 0;
			
		UINT64 send_sequence_ = 0;
		std::vector<RioIoContext*> send_contexts_;
		
		RIO_RQ			rio_rq_ = NULL;		
		RIO_BUFFERID	send_rio_buffer_id_ = NULL;
		
		RingBufferVA2 send_buffer_;
	};
}