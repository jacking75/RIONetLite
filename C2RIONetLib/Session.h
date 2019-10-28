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
		const INT64 ENABLE_VALUE = 1;

	public:
		Session() = default;
		~Session() = default;
		
		int GetIndex() { return mIndex; }

		UINT64 GetUniqueId() { return mUnqiueId; }

		bool IsEnableNetwork() { return mEnableNetwork == ENABLE_VALUE; }
		void SetDisableNetwork() { mEnableNetwork = 0; }
		
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
				LogFuncPtr((int)LogLevel::error, logmsg);

				return false;
			}

			mRecvContext.Init(mIndex, IOType::recv);
			return true;
		}

		void Dispose()
		{
			if (mRecvRioBufferId != NULL) { RIO.RIODeregisterBuffer(mRecvRioBufferId); }
			mIOSender_.Dispose();
		}

		bool OnConnect(UINT64 uniqueId, SOCKET socket, RIO_RQ rioRequestQueue)
		{
			SetAddress(socket);

			FastSpinlockGuard criticalSection(mSessionLock);

			//TODO 테스트 하면서 논블러링으로 해야 하는지 확인하기
			/// make socket non-blocking
			//u_long arg = 1;
			//ioctlsocket(mSocket, FIONBIO, &arg);

			/// turn off nagle
			int opt = 1;
			setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int));
			
			mSocket = socket;
			mUnqiueId = uniqueId;
			mConnected = CONNECTED;
			mEnableNetwork = ENABLE_VALUE;
			mRequestQueue = rioRequestQueue;
			mIOSender_.OnConnect(mRequestQueue);
			mRecvBuffer_.Clear();

			return PostRecv() == NetErrorCode::none;
		}
		
		bool IsConnected() const { return mConnected == CONNECTED; }

		int	GetRioThreadId() const { return mRioThreadId; }

		bool PostSend(const UINT32 dataSize, const char* pData)
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			auto result = mIOSender_.PostSend(GetUniqueId(), dataSize, pData);
			return result;
		}

		void SendCompletion(RioIoContext* pSendContext)
		{
			FastSpinlockGuard criticalSection(mSessionLock);
			mIOSender_.Completed(pSendContext);
		}

		NetErrorCode PostRecv()
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			if (IsConnected() == false) { return NetErrorCode::recv_closed_status; }

			if (IsEnableNetwork() == false) { return NetErrorCode::recv_disable_network_status; }

			mRecvContext.SessionUnique = GetUniqueId();
			mRecvContext.BufferId = mRecvRioBufferId;
			mRecvContext.Length = mMaxTry1RecvBufferSize;
			mRecvContext.Offset = mRecvBuffer_.GetWriteablePos();
						
			/// start async recv
			DWORD flags = 0;
			if (!RIO.RIOReceive(mRequestQueue, (PRIO_BUF)&mRecvContext, 1, flags, &mRecvContext))
			{
				char logmsg[256] = { 0, };
				sprintf_s(logmsg, "Session::PostRecv() - RIOReceive Error: %d", GetLastError());
				LogFuncPtr((int)LogLevel::error, logmsg);

				return NetErrorCode::recv_rio_fail;
			}

			return NetErrorCode::none;
		}
						
		char* RecvCompletion(UINT32 readRecvSize_)
		{ 
			mRecvBuffer_.Write(readRecvSize_);
			
			// lock을 걸지 않아도 문제 되지 않을 것 같다.
			return mRecvBuffer_.Read(readRecvSize_);
		}

		bool Disconnect(const UINT64 sessionUniqueIdByContext)
		{
			if (IsConnected() == false) { return false; }

			FastSpinlockGuard criticalSection(mSessionLock);

			if (GetUniqueId() != sessionUniqueIdByContext) { return false; }

			LINGER lingerOption;
			lingerOption.l_onoff = 1;
			lingerOption.l_linger = 0;

			/// no TCP TIME_WAIT
			setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOption, sizeof(LINGER));
						
			closesocket(mSocket);
			mSocket = NULL;
			mConnected = 0;
			SetDisableNetwork();
			mUnqiueId = 0;
					
			return true;
		}

		void DisConnectByAcceptFail()
		{
			closesocket(mSocket);
			mSocket = NULL;
			mConnected = 0;
			SetDisableNetwork();
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

		
		FastSpinlock mSessionLock;

		int				mIndex = -1;
		UINT32 mMaxTry1RecvBufferSize = 0;
		int				mRioThreadId = -1;
		RIO_BUFFERID	mRecvRioBufferId = NULL;

		UINT64			mUnqiueId = 0;
		
		// 64비트에서 INT64는 아토믹하게 읽기,쓰기 가능. ++i 가 아닌 i = 1 등이 아토믹하다는 것이다.
		// https://docs.microsoft.com/en-us/windows/win32/sync/interlocked-variable-access
		INT64			mConnected = 0;
		
		INT64			mEnableNetwork = 0;
		
		SOCKET			mSocket = INVALID_SOCKET;
		SOCKADDR_IN		mClientAddr;
		
		RIO_RQ			mRequestQueue = NULL;
		RioIoContext mRecvContext;
				
		NetSenderImmediately mIOSender_;

		RingBufferVA2 mRecvBuffer_;
	};

}