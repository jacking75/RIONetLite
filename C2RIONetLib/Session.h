#pragma once

#include <vector>

#include "NetSenderImmediately.h"
#include "RingBufferVA2.h"
#include "FastSpinlock.h"
#include "RioContext.h"

namespace C2RIONetLib
{
	class Session
	{
		const INT64 CONNECTED = 1;

	public:
		Session() = default;
		~Session() = default;
		
		int GetIndex() { return mIndex; }

		UINT64 GetUniqueId() { return mUnqiueId; }

		void Init(int index, int threadId)
		{
			mRioThreadId = threadId;
			mIndex = index;
			memset(&mClientAddr, 0, sizeof(SOCKADDR_IN));
		}

		bool RioInitialize(ConnectionNetConfig netConfig)
		{
			mMaxTry1RecvBufferSize = netConfig.MaxTry1RecvBufferSize;
			mIOSender_.Init(mIndex, netConfig);
			mRecvBuffer_.Create(netConfig.MaxRecvBufferSize);

			mRecvRioBufferId = RIO.RIORegisterBuffer(mRecvBuffer_.GetBuffer(), netConfig.MaxRecvBufferSize);
			if (mRecvRioBufferId == RIO_INVALID_BUFFERID)
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "Session::RioInitialize() - RIORegisterBuffer Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::Error, logmsg);

				return false;
			}

			mRecvContext.Init(mIndex, IOType::IO_RECV);
			return true;
		}

		void Dispose()
		{
			if (mRecvRioBufferId != NULL) { RIO.RIODeregisterBuffer(mRecvRioBufferId); }
			mIOSender_.Dispose();
		}

		//TODO 멤버들 초기화 잊어 먹지 않고 잘 하고 있나?
		bool OnConnect(UINT64 uniqueId, SOCKET socket, RIO_RQ rioRequestQueue)
		{
			SetAddress(socket);

			FastSpinlockGuard criticalSection(mSessionLock);

			mSocket = socket;
			mUnqiueId = uniqueId;

			/// make socket non-blocking
			u_long arg = 1;
			ioctlsocket(mSocket, FIONBIO, &arg);

			/// turn off nagle
			int opt = 1;
			setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int));
			
			mConnected = CONNECTED;
			mRequestQueue = rioRequestQueue;
			mIOSender_.OnConnect(mRequestQueue);
			
			return PostRecv();
		}
		
		bool IsConnected() const { return mConnected == CONNECTED; }

		int	GetRioThreadId() const { return mRioThreadId; }

		bool PostSend(const UINT32 dataSize, const char* pData)
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			auto result = mIOSender_.PostSend(GetUniqueId(), dataSize, pData);
			return result;
		}

		bool PostRecv()
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			if (!IsConnected()) { return false; }

			mRecvContext.SessionUnique = GetUniqueId();
			mRecvContext.BufferId = mRecvRioBufferId;
			mRecvContext.Length = mMaxTry1RecvBufferSize;
			mRecvContext.Offset = mRecvBuffer_.GetWriteableOffSet();
						
			/// start async recv
			DWORD flags = 0;
			if (!RIO.RIOReceive(mRequestQueue, (PRIO_BUF)&mRecvContext, 1, flags, &mRecvContext))
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "Session::PostRecv() - RIOReceive Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::Error, logmsg);

				return false;
			}

			return true;
		}
						
		void SendCompletion(RioIoContext* pSendContext)
		{
			FastSpinlockGuard criticalSection(mSessionLock);
			mIOSender_.Completed(pSendContext);
		}

		bool Disconnect(const UINT64 sessionUniqueIdByContext)
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			if (IsConnected() == false) { return false; }

			if (GetUniqueId() != sessionUniqueIdByContext) { return false; }

			LINGER lingerOption;
			lingerOption.l_onoff = 1;
			lingerOption.l_linger = 0;

			/// no TCP TIME_WAIT
			setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOption, sizeof(LINGER));
						
			closesocket(mSocket);
			mSocket = NULL;
			mConnected = 0;
			mUnqiueId = 0;
					
			return true;
		}

		void DisConnectByAcceptFail()
		{
			closesocket(mSocket);
			mSocket = NULL;
			mConnected = 0;
			mUnqiueId = 0;
		}
				

	private:
		void SetAddress(SOCKET socket)
		{
			SOCKADDR_IN clientaddr;
			int addrlen = sizeof(clientaddr);
			getpeername(socket, (SOCKADDR*)&clientaddr, &addrlen);
			memcpy(&mClientAddr, &clientaddr, sizeof(SOCKADDR_IN));
		}

		
		int				mIndex = -1;
		UINT64			mUnqiueId = 0;
		
		// 64비트에서 INT64는 아토믹하게 읽기,쓰기 가능. ++i 가 아닌 i = 1 등이 아토믹하다는 것이다.
		// https://docs.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access
		INT64			mConnected = 0;
		
		//INT64			mDisConnectedSec = 0;

		FastSpinlock	mSessionLock;

		SOCKET			mSocket = INVALID_SOCKET;
		SOCKADDR_IN		mClientAddr;

		UINT32 mMaxTry1RecvBufferSize = 0;

		
		
		int				mRioThreadId = -1;
		RIO_BUFFERID	mRecvRioBufferId = NULL;		
		RIO_RQ			mRequestQueue = NULL;
		RioIoContext mRecvContext;
				
		NetSenderImmediately mIOSender_;

		RingBufferVA2 mRecvBuffer_;
	};

}