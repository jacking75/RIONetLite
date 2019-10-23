#pragma once

#include <vector>
#include <concurrent_queue.h>

#include "RingBufferVA2.h"
#include "FastSpinlock.h"
#include "RioContext.h"
#include "CommonDef.h"

namespace C2RIONetLib
{
	class NetSenderImmediately
	{
	public:
		NetSenderImmediately() = default;

		~NetSenderImmediately()
		{

		}

		char* GetBuffer() { return mSendBuffer_.GetBuffer(); }

		bool Init(const INT32 sessionIndex, ConnectionNetConfig netConfig)
		{
			mSessionIndex_ = sessionIndex;
			mMaxSendPacketSize_ = netConfig.MaxSendPacketSize;
			mMAXSendCountPer1Sec_ = netConfig.MAXSendCountPer1Sec;
			mMaxSendRingBufferSize_ = netConfig.MaxSendBufferSize;
			
			if (auto isResult = mSendBuffer_.Create(mMaxSendRingBufferSize_); 
				isResult != 0)
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "NetSenderImmediately::Initialize() - Error: %d", isResult);
				LogFuncPtr((int)LogLevel::Error, logmsg);
				return false;
			}

			for (int i = 0; i < mMAXSendCountPer1Sec_; ++i)
			{
				auto context = new RioIoContext;
				context->Init(mSessionIndex_, IOType::IO_SEND);

				mSendContexts_.push_back(context);
			}

			mSendRioBufferId = RIO.RIORegisterBuffer(mSendBuffer_.GetBuffer(), netConfig.MaxSendBufferSize);
			if (mSendRioBufferId == RIO_INVALID_BUFFERID)
			{
				printf_s("[DEBUG] RIORegisterBuffer Error: %d\n", GetLastError());
				return false;
			}

			Clear();

			return true;
		}

		void Dispose()
		{
			RIO.RIODeregisterBuffer(mSendRioBufferId);

			for (int i = 0; i < mMAXSendCountPer1Sec_; ++i)
			{
				delete mSendContexts_[i];
			}
		}

		// 스레드 세이프 하지 않다
		void Clear()
		{
			mSendContextQueue_.clear();

			for (auto pContext : mSendContexts_)
			{
				mSendContextQueue_.push(pContext);
			}

			mSendBuffer_.Clear();
		}

		void OnConnect(RIO_RQ requestQueue)
		{
			mRequestQueue = requestQueue;
		}

		// 스레드 세이프 하지 않다
		bool PostSend(const UINT32 dataSize, const char* pData)
		{
			RioIoContext* pSendContext = nullptr;
			if (mSendContextQueue_.try_pop(pSendContext) == false) { return false; }

			auto writeAblePos = mSendBuffer_.GetWriteableOffSet();
			auto pBufferPos = mSendBuffer_.Write(dataSize, pData);
			
			pSendContext->BufferId = mSendRioBufferId;
			pSendContext->Length = dataSize;
			pSendContext->Offset = writeAblePos;

			
			DWORD sendbytes = 0;
			DWORD flags = 0;

			/// start async send
			if (!RIO.RIOSend(mRequestQueue, (PRIO_BUF)pSendContext, 1, flags, pSendContext))
			{
				printf_s("[DEBUG] RIOSend error: %d\n", GetLastError());
				//ReleaseContext(sendContext);
				return false;
			}

			return true;
		}

		// 스레드 세이프 하지 않다
		void Completed(RioIoContext* pSendContext)
		{
		}


	private:
		INT32 mSessionIndex_ = 0;
		INT32 mMaxSendPacketSize_ = 0;
		INT32 mMAXSendCountPer1Sec_ = 0;
		INT32 mMaxSendRingBufferSize_ = 0;
			
		RIO_RQ			mRequestQueue;

		std::vector<RioIoContext*> mSendContexts_;
		concurrency::concurrent_queue<RioIoContext*> mSendContextQueue_;
		
		RIO_BUFFERID	mSendRioBufferId = NULL;
		RingBufferVA2 mSendBuffer_;
	};
}