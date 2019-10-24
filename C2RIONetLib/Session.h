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
	public:
		Session() {}// = default;

		~Session()
		{
			RIO.RIODeregisterBuffer(mRecvRioBufferId);
			mIOSender_.Dispose();
		}

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
			
			mConnected = true;
			mRequestQueue = rioRequestQueue;
			mIOSender_.OnConnect(mRequestQueue);
			
			return PostRecv();
		}
		
		bool IsConnected() const { return mConnected; }

		int	GetRioThreadId() const { return mRioThreadId; }

		bool PostSend(const UINT32 dataSize, const char* pData)
		{
			return mIOSender_.PostSend(dataSize, pData);
		}

		bool PostRecv()
		{
			if (!IsConnected()) { return false; }

			//TODO PostRecv() 버퍼 공간 남았는지 확인하기

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
			mIOSender_.Completed(pSendContext);
		}

		bool Disconnect()
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			if (!IsConnected()) { return false; }

			LINGER lingerOption;
			lingerOption.l_onoff = 1;
			lingerOption.l_linger = 0;

			/// no TCP TIME_WAIT
			setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOption, sizeof(LINGER));
						
			closesocket(mSocket);

			mConnected = false;
			mSocket = NULL;			
			return true;
		}
				

	private:
		void Destory()
		{
		}

		void SetAddress(SOCKET socket)
		{
			SOCKADDR_IN clientaddr;
			int addrlen = sizeof(clientaddr);
			getpeername(socket, (SOCKADDR*)&clientaddr, &addrlen);
			memcpy(&mClientAddr, &clientaddr, sizeof(SOCKADDR_IN));
		}

		
		int				mIndex = -1;
		UINT64			mUnqiueId = 0;
		
		bool			mConnected = false;
		FastSpinlock	mSessionLock;

		SOCKET			mSocket = INVALID_SOCKET;
		SOCKADDR_IN		mClientAddr;

		UINT32 mMaxTry1RecvBufferSize = 0;

		
		
		int				mRioThreadId = -1;
		RIO_BUFFERID	mRecvRioBufferId = NULL;		
		RIO_RQ			mRequestQueue;
		RioIoContext mRecvContext;
				
		NetSenderImmediately mIOSender_;

		RingBufferVA2 mRecvBuffer_;
	};

}