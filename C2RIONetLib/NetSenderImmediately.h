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
		
		char* GetBuffer() { return mSendBuffer.GetBuffer(); }

		bool Init(const INT32 sessionIndex, ConnectionNetConfig netConfig)
		{
			mSessionIndex = sessionIndex;
			mMaxSendPacketSize = netConfig.MaxSendPacketSize;
			mMAXSendCountPer1Sec = netConfig.MAXSendCountPer1Sec;
			mMaxSendRingBufferSize = netConfig.MaxSendBufferSize;
			
			if (auto isResult = mSendBuffer.Create(mMaxSendRingBufferSize); 
				isResult != 0)
			{
				char logmsg[512] = { 0, };
				sprintf_s(logmsg, "NetSenderImmediately::Init() - Error: %d", isResult);
				LogFuncPtr((int)LogLevel::error, logmsg);
				return false;
			}

			for (int i = 0; i < mMAXSendCountPer1Sec; ++i)
			{
				auto context = new RioIoContext;
				context->Init(mSessionIndex, IOType::send);

				mSendContexts.push_back(context);
			}

			mSendRioBufferId = RIO.RIORegisterBuffer(mSendBuffer.GetBuffer(), netConfig.MaxSendBufferSize);
			if (mSendRioBufferId == RIO_INVALID_BUFFERID)
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
			RIO.RIODeregisterBuffer(mSendRioBufferId);

			for (int i = 0; i < mMAXSendCountPer1Sec; ++i)
			{
				delete mSendContexts[i];
			}
		}

		void OnConnect(RIO_RQ requestQueue)
		{
			mRequestQueue = requestQueue;
			Clear();
		}
				
		bool PostSend(const UINT64 sessionUniqueId, const UINT32 dataSize, const char* pData)
		{
			auto curPos = mSendSequence % mMAXSendCountPer1Sec;
			RioIoContext* pSendContext = mSendContexts[curPos];
			
			if (pSendContext->Length != 0) 
			{ 
				LogFuncPtr((int)LogLevel::error, "NetSenderImmediately::PostSend() - empty Context");
				return false; 
			}

			auto writeAblePos = mSendBuffer.GetWriteablePos();
			 mSendBuffer.Write(dataSize, pData);
			
			pSendContext->SessionUniqueId = sessionUniqueId;
			pSendContext->BufferId = mSendRioBufferId;
			pSendContext->Length = dataSize;
			pSendContext->Offset = writeAblePos;
					
			
			/// start async send
			DWORD flags = 0;
			if (!RIO.RIOSend(mRequestQueue, (PRIO_BUF)pSendContext, 1, flags, pSendContext))
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
			for (auto pContext : mSendContexts)
			{
				pContext->Length = 0;
			}

			mSendBuffer.Clear();
		}


		INT32 mSessionIndex = 0;
		INT32 mMaxSendPacketSize = 0;
		UINT64 mMAXSendCountPer1Sec = 0;
		INT32 mMaxSendRingBufferSize = 0;
			
		UINT64 mSendSequence = 0;
		std::vector<RioIoContext*> mSendContexts;
		
		RIO_RQ			mRequestQueue = NULL;		
		RIO_BUFFERID	mSendRioBufferId = NULL;
		
		RingBufferVA2 mSendBuffer;
	};
}