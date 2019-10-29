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
		
		void Init(int index_, int threadId_)
		{
			mRioThreadId = threadId_;
			mIndex = index_;
			memset(&mClientAddr, 0, sizeof(SOCKADDR_IN));
		}

		bool RioInitialize(ConnectionNetConfig netConfig_)
		{
			mMaxTry1RecvBufferSize = netConfig_.MaxTry1RecvBufferSize;
			mIOSender.Init(mIndex, netConfig_);
			mRecvBuffer.Create(false, netConfig_.MaxRecvBufferSize);

			mRecvRioBufferId = RIO.RIORegisterBuffer(mRecvBuffer.GetBuffer(), netConfig_.MaxRecvBufferSize);
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
			mIOSender.Dispose();
		}

		bool OnConnect(UINT64 uniqueId_, SOCKET socket_, RIO_RQ rioRequestQueue_)
		{
			// 세션 객체가 재 사용되더라도 대기 시간이 있으므로 연결이 끊어질 때와 이 함수 호출이 겹쳐지지 않는다.
			SetAddress(socket_);
									
			/// turn off nagle
			int opt = 1;
			setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int));
			
			mSocket = socket_;
			mUnqiueId = uniqueId_;
			mConnected = CONNECTED;
			mEnableNetwork = ENABLE_VALUE;
			mRequestQueue = rioRequestQueue_;
			mIOSender.OnConnect(mRequestQueue);
			mRecvBuffer.Clear();

			return PostRecv() == NetErrorCode::none;
		}
		
		bool IsConnected() const { return mConnected == CONNECTED; }

		int	GetRioThreadId() const { return mRioThreadId; }

		bool PostSend(const UINT32 dataSize_, const char* pData_)
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			auto result = mIOSender.PostSend(GetUniqueId(), dataSize_, pData_);
			return result;
		}

		void SendCompletion(RioIoContext* pSendContext_)
		{
			FastSpinlockGuard criticalSection(mSessionLock);
			mIOSender.Completed(pSendContext_);
		}

		NetErrorCode PostRecv()
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			if (IsConnected() == false) { return NetErrorCode::recv_closed_status; }

			if (IsEnableNetwork() == false) { return NetErrorCode::recv_disable_network_status; }

			mRecvContext.SessionUniqueId = GetUniqueId();
			mRecvContext.BufferId = mRecvRioBufferId;
			mRecvContext.Length = mMaxTry1RecvBufferSize;
			mRecvContext.Offset = mRecvBuffer.GetWriteablePos();
						
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
		

		void RecvCompletion(UINT32 size_)
		{ 
			mRecvBuffer.Write(size_);			
		}

		std::tuple<UINT32, char*> ReadRecvBuffer() { return mRecvBuffer.Read(); }

		void ReadCompleteRecvBuffer(const UINT32 size_) { mRecvBuffer.IncremantReadPos(size_); }


		bool Disconnect(const UINT64 sessionUniqueIdByContext_)
		{
			if (IsConnected() == false) { return false; }

			FastSpinlockGuard criticalSection(mSessionLock);

			if (GetUniqueId() != sessionUniqueIdByContext_) { return false; }

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
		void SetAddress(SOCKET socket_)
		{
			SOCKADDR_IN clientaddr;
			int addrlen = sizeof(clientaddr);
			getpeername(socket_, (SOCKADDR*)&clientaddr, &addrlen);
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
				
		NetSenderImmediately mIOSender;

		RingBufferVA2 mRecvBuffer;
	};

}