#pragma once

#include "SendBuffer.h"

#include "..\FastSpinlock.h"
#include "..\RioContext.h"
#include "..\CommonDef.h"


namespace SendPacketCollectSystem
{
	using LogLeveL = C2RIONetLib::LogLeveL;
	using LogFunc = C2RIONetLib::LogFuncPtr;

	struct NetSendConfig
	{
		UINT32 MaxSessionCount = 0;
		UINT32 MaxWorkerThreadCount = 0;
		UINT32 MaxSendPacketSize = 0;
		UINT32 MAXSendCountPer1Sec = 0;  // 정한 것 보다 조금 더 넉넉하게 주는 것을 추천
		UINT32 MaxSendBufferSize = 0;

	};

	// 보낼 데이터를 모아서 주기적으로 한번에 보내기
	class Session
	{
	public:
		Session() = default;
		~Session() = default;

		char* GetBuffer() { return mSendBuffer.GetBuffer(); }

		bool IsConnected() { return mIsConnected; }

		bool Init(const INT32 sessionIndex_, NetSendConfig netConfig_)
		{
			mSessionIndex = sessionIndex_;
			mMaxSendPacketSize = netConfig_.MaxSendPacketSize;
			mMAXSendCountPer1Sec = netConfig_.MAXSendCountPer1Sec;
			mMaxSendRingBufferSize = netConfig_.MaxSendBufferSize;

			if (auto isResult = mSendBuffer.Create(mMaxSendRingBufferSize);
				isResult != 0)
			{
				LogFunc((int)LogLevel::error, "Session::Init() - fail mSendBuffer.Create");
				return false;
			}

			mSendContexts.Init(mSessionIndex, IOType::send);
			
			mSendRioBufferId = RIO.RIORegisterBuffer(mSendBuffer.GetBuffer(), mSendBuffer.EnableBufferSize());
			if (mSendRioBufferId == RIO_INVALID_BUFFERID)
			{
				char logmsg[128] = { 0, };
				sprintf_s(logmsg, "Session::Init() - RIORegisterBuffer Error: %d", GetLastError());
				LogFunc((int)LogLevel::error, logmsg);
				return false;
			}

			Clear();

			return true;
		}

		void Dispose()
		{
			RIO.RIODeregisterBuffer(mSendRioBufferId);			
		}

		void OnConnect(RIO_RQ requestQueue_)
		{
			mIsConnected = true;
			mRequestQueue = requestQueue_;
			Clear();
		}

		void OnDisConnect()
		{
			mIsConnected = false;
		}

		bool PostSend(const UINT32 dataSize_, const char* pData_)
		{
			C2RIONetLib::FastSpinlockGuard criticalSection(mSessionLock);
			if (mSendBuffer.Write(dataSize_, pData_) == false) { return false; }
			return true;
		}

		void SendCompleted(const UINT32 length_)
		{
			C2RIONetLib::FastSpinlockGuard criticalSection(mSessionLock);

			mSendBuffer.IncremantReadPos(length_);
			mSendBuffer.MoveWrittenDataForward();
		}


		bool RealSend()
		{
			C2RIONetLib::FastSpinlockGuard criticalSection(mSessionLock);

			auto sendSize = mSendBuffer.SendAbleSize();

			if (sendSize <= 0) { return true }

			if (sendSize > mMaxSendPacketSize) { sendSize = mMaxSendPacketSize;	}
			
			mSendContexts.SessionUniqueId = sessionUniqueId;
			mSendContexts.BufferId = mSendRioBufferId;
			mSendContexts.Length = sendSize;
			mSendContexts.Offset = mSendBuffer.GetReadPos();
			
			/// start async send
			DWORD flags = 0;
			if (!RIO.RIOSend(mRequestQueue, (PRIO_BUF)pSendContext, 1, flags, pSendContext))
			{
				char logmsg[128] = { 0, };
				sprintf_s(logmsg, "Session::RealSend() - RIOSend Error: %d", GetLastError());
				LogFunc((int)LogLevel::error, logmsg);
				return false;
			}

			return true;
		}
		

	private:
		void Clear()
		{
			mSendBuffer.Clear();
		}

		bool mIsConnected = false;
		INT32 mSessionIndex = 0;
		INT32 mMaxSendPacketSize = 0;
		UINT64 mMAXSendCountPer1Sec = 0;
		INT32 mMaxSendRingBufferSize = 0;

		C2RIONetLib::RioIoContext mSendContexts;

		RIO_RQ			mRequestQueue = NULL;
		RIO_BUFFERID	mSendRioBufferId = NULL;

		C2RIONetLib::FastSpinlock mSessionLock;
		SendBuffer mSendBuffer;
	};
}