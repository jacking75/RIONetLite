#pragma once

#include <vector>
#include <queue>

#include "RingBufferVA2.h"
#include "FastSpinlock.h"
#include "RioContext.h"
#include "CommonDef.h"

namespace C2RIONetLib
{
	// 스레드 세이프 하지 않다 !!!
	class NetSenderImmediately
	{
	public:
		NetSenderImmediately() = default;

		~NetSenderImmediately() = default;
		
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
				char logmsg[512] = { 0, };
				sprintf_s(logmsg, "NetSenderImmediately::Init() - Error: %d", isResult);
				LogFuncPtr((int)LogLevel::error, logmsg);
				return false;
			}

			for (int i = 0; i < mMAXSendCountPer1Sec_; ++i)
			{
				auto context = new RioIoContext;
				context->Init(mSessionIndex_, IOType::send);

				mSendContexts_.push_back(context);
			}

			mSendRioBufferId_ = RIO.RIORegisterBuffer(mSendBuffer_.GetBuffer(), netConfig.MaxSendBufferSize);
			if (mSendRioBufferId_ == RIO_INVALID_BUFFERID)
			{
				char logmsg[512] = { 0, };
				sprintf_s(logmsg, "NetSenderImmediately::Init() - RIORegisterBuffer Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::error, logmsg);
				return false;
			}

			Clear();

			return true;
		}

		void Dispose()
		{
			RIO.RIODeregisterBuffer(mSendRioBufferId_);

			for (int i = 0; i < mMAXSendCountPer1Sec_; ++i)
			{
				delete mSendContexts_[i];
			}
		}

		void OnConnect(RIO_RQ requestQueue)
		{
			mRequestQueue_ = requestQueue;
			Clear();
		}
				
		bool PostSend(const UINT64 sessionUniqueId, const UINT32 dataSize, const char* pData)
		{
			auto curPos = mSendSequence_ % mMAXSendCountPer1Sec_;
			RioIoContext* pSendContext = mSendContexts_[curPos];
			
			if (pSendContext->Length != 0) 
			{ 
				LogFuncPtr((int)LogLevel::error, "NetSenderImmediately::PostSend() - empty Context");
				return false; 
			}

			auto writeAblePos = mSendBuffer_.GetWriteablePos();
			 mSendBuffer_.Write(dataSize, pData);
			
			pSendContext->SessionUnique = sessionUniqueId;
			pSendContext->BufferId = mSendRioBufferId_;
			pSendContext->Length = dataSize;
			pSendContext->Offset = writeAblePos;
					
			
			/// start async send
			DWORD flags = 0;
			if (!RIO.RIOSend(mRequestQueue_, (PRIO_BUF)pSendContext, 1, flags, pSendContext))
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "NetSenderImmediately::PostSend() - RIOSend Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::error, logmsg);

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
			for (auto pContext : mSendContexts_)
			{
				pContext->Length = 0;
			}

			mSendBuffer_.Clear();
		}


		INT32 mSessionIndex_ = 0;
		INT32 mMaxSendPacketSize_ = 0;
		UINT64 mMAXSendCountPer1Sec_ = 0;
		INT32 mMaxSendRingBufferSize_ = 0;
			
		UINT64 mSendSequence_ = 0;
		std::vector<RioIoContext*> mSendContexts_;
		
		RIO_RQ			mRequestQueue_ = NULL;		
		RIO_BUFFERID	mSendRioBufferId_ = NULL;
		
		RingBufferVA2 mSendBuffer_;
	};
}