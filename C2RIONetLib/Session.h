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

		void Init(int index, int threadId)
		{
			mRioThreadId = threadId;
			mIndex = index;
			memset(&mClientAddr, 0, sizeof(SOCKADDR_IN));
		}

		//TODO 환경 설정값 사용하기, 반환 값 타입을 enum으로
		bool RioInitialize(ConnectionNetConfig netConfig)
		{
			mMaxTry1RecvBufferSize = netConfig.MaxTry1RecvBufferSize;
			mIOSender_.Init(mIndex, netConfig);
			mRecvBuffer_.Create(netConfig.MaxRecvBufferSize);

			mRecvRioBufferId = RIO.RIORegisterBuffer(mRecvBuffer_.GetBuffer(), netConfig.MaxRecvBufferSize);
			if (mRecvRioBufferId == RIO_INVALID_BUFFERID)
			{
				printf_s("[DEBUG] RIORegisterBuffer Error: %d\n", GetLastError());
				return false;
			}

			mRecvContext.Init(mIndex, IOType::IO_RECV);
			return true;
		}

		bool	OnConnect(SOCKET socket, const RIO_CQ& rioCompletionQueue)
		{
			SetAddress(socket);
			
			FastSpinlockGuard criticalSection(mSessionLock);

			mSocket = socket;

			/// make socket non-blocking
			u_long arg = 1;
			ioctlsocket(mSocket, FIONBIO, &arg);

			/// turn off nagle
			int opt = 1;
			setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int));

			/// create socket RQ
			/// SEND and RECV within one CQ (you can do with two CQs, seperately)
			mRequestQueue = RIO.RIOCreateRequestQueue(mSocket,
				MAX_RECV_RQ_SIZE_PER_SOCKET, 1,
				MAX_SEND_RQ_SIZE_PER_SOCKET, 1,
				rioCompletionQueue,
				rioCompletionQueue,
				NULL);
			if (mRequestQueue == RIO_INVALID_RQ)
			{
				//printf_s("[DEBUG] RIOCreateRequestQueue Error: %d\n", GetLastError());
				return false;
			}

			
			mConnected = true;
			mIOSender_.OnConnect(mRequestQueue);
			//printf_s("[DEBUG] Client Connected: IP=%s, PORT=%d\n", inet_ntoa(mClientAddr.sin_addr), ntohs(mClientAddr.sin_port));

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

			//TODO 버퍼 공간 남았는지 확인하기

			//TODO 동적 할당 안하도록 하기
			mRecvContext.BufferId = mRecvRioBufferId;
			mRecvContext.Length = mMaxTry1RecvBufferSize;
			mRecvContext.Offset = mRecvBuffer_.GetWriteableOffSet();

			DWORD recvbytes = 0;
			DWORD flags = 0;

			/// start async recv
			if (!RIO.RIOReceive(mRequestQueue, (PRIO_BUF)&mRecvContext, 1, flags, &mRecvContext))
			{
				//TODO 구현하기
				//printf_s("[DEBUG] RIOReceive error: %d\n", GetLastError());
				//ReleaseContext(recvContext);
				return false;
			}

			return true;
		}

		void	RecvCompletion(DWORD transferred)
		{
			//TODO 구현하기
		}

		//bool	PostSend()
		//{
		//	if (!IsConnected())
		//		return false;

		//	if (0 == mCircularBuffer.GetContiguiousBytes())
		//		return true;

		//	RioIoContext* sendContext = new RioIoContext(mIndex, IO_SEND);

		//	sendContext->BufferId = mRioBufferId;
		//	sendContext->Length = static_cast<ULONG>(mCircularBuffer.GetContiguiousBytes());
		//	sendContext->Offset = mCircularBuffer.GetReadableOffset();

		//	DWORD sendbytes = 0;
		//	DWORD flags = 0;

		//	/// start async send
		//	if (!RIO.RIOSend(mRequestQueue, (PRIO_BUF)sendContext, 1, flags, sendContext))
		//	{
		//		printf_s("[DEBUG] RIOSend error: %d\n", GetLastError());
		//		//ReleaseContext(sendContext);
		//		return false;
		//	}

		//	return true;
		//}

		void SendCompletion(DWORD transferred)
		{
		}

		void Disconnect(DisconnectReason dr)
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			if (!IsConnected()) { return; }

			LINGER lingerOption;
			lingerOption.l_onoff = 1;
			lingerOption.l_linger = 0;

			/// no TCP TIME_WAIT
			setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOption, sizeof(LINGER));

			//printf_s("[DEBUG] Client Disconnected: Reason=%d IP=%s, PORT=%d \n", dr, inet_ntoa(mClientAddr.sin_addr), ntohs(mClientAddr.sin_port));

			closesocket(mSocket);

			mConnected = false;
			mSocket = NULL;

			ReleaseRef();
		}

		void AddRef()
		{
			InterlockedIncrement(&mRefCount);
		}

		//TODO: ReleaseRef 사용 안하기
		void ReleaseRef()
		{
			long ret = InterlockedDecrement(&mRefCount);			
			if (ret == 0)
			{				
				//GSessionManager->ReturnClientSession(this);
			}
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

		
		UINT32 mMaxTry1RecvBufferSize = 0;

		bool			mConnected = false;
		int				mIndex= -1;
		int				mRioThreadId = -1;

		SOCKET			mSocket = INVALID_SOCKET;
		SOCKADDR_IN		mClientAddr;

		RIO_BUFFERID	mRecvRioBufferId = NULL;
		
		RIO_RQ			mRequestQueue;

		FastSpinlock	mSessionLock;

		RioIoContext mRecvContext;
		NetSenderImmediately mIOSender_;

		RingBufferVA2 mRecvBuffer_;

		volatile long	mRefCount = 0;
	};

}