#pragma once

#include <winsock2.h>

#include "CircularBuffer.h"
#include "FastSpinlock.h"
#include "RioContext.h"

namespace C2RIONetLib
{
	class ClientSession
	{
	public:
		ClientSession() = default;

		~ClientSession()
		{
			//TODO: 구현하기
			//RIO.RIODeregisterBuffer(mRioBufferId);
			VirtualFreeEx(GetCurrentProcess(), mRioBufferPointer, 0, MEM_RELEASE);			
		}

		int GetIndex() { return mIndex; }

		void Init(int index, int threadId)
		{
			mRioThreadId = threadId;
			mIndex = index;
			memset(&mClientAddr, 0, sizeof(SOCKADDR_IN));
		}

		bool	OnConnect(SOCKET socket, SOCKADDR_IN* addr)
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			//CRASH_ASSERT(LIoThreadId == MAIN_THREAD_ID);

			mSocket = socket;

			/// make socket non-blocking
			u_long arg = 1;
			ioctlsocket(mSocket, FIONBIO, &arg);

			/// turn off nagle
			int opt = 1;
			setsockopt(mSocket, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int));

			//TODO:구현하기
			/// create socket RQ
			/// SEND and RECV within one CQ (you can do with two CQs, seperately)
			/*mRequestQueue = RIO.RIOCreateRequestQueue(mSocket,
				MAX_RECV_RQ_SIZE_PER_SOCKET, 1,
				MAX_SEND_RQ_SIZE_PER_SOCKET, 1,
				GRioManager->GetCompletionQueue(mRioThreadId),
				GRioManager->GetCompletionQueue(mRioThreadId),
				NULL);*/
			if (mRequestQueue == RIO_INVALID_RQ)
			{
				//printf_s("[DEBUG] RIOCreateRequestQueue Error: %d\n", GetLastError());
				return false;
			}

			memcpy(&mClientAddr, addr, sizeof(SOCKADDR_IN));
			mConnected = true;

			//printf_s("[DEBUG] Client Connected: IP=%s, PORT=%d\n", inet_ntoa(mClientAddr.sin_addr), ntohs(mClientAddr.sin_port));

			return PostRecv();
		}

		bool	IsConnected() const { return mConnected; }
		int		GetRioThreadId() const { return mRioThreadId; }


		bool	PostRecv()
		{
			if (!IsConnected())
				return false;

			if (0 == mCircularBuffer.GetFreeSpaceSize())
				return false;

			//TODO 동적 할당 안하도록 하기
			RioIoContext* recvContext = new RioIoContext(mIndex, IO_RECV);

			recvContext->BufferId = mRioBufferId;
			recvContext->Length = static_cast<ULONG>(mCircularBuffer.GetFreeSpaceSize());
			recvContext->Offset = mCircularBuffer.GetWritableOffset();

			DWORD recvbytes = 0;
			DWORD flags = 0;

			/// start async recv
			if (!RIO.RIOReceive(mRequestQueue, (PRIO_BUF)recvContext, 1, flags, recvContext))
			{
				//printf_s("[DEBUG] RIOReceive error: %d\n", GetLastError());
				//ReleaseContext(recvContext);
				return false;
			}

			return true;
		}

		void	RecvCompletion(DWORD transferred)
		{
			mCircularBuffer.Commit(transferred);
		}

		bool	PostSend()
		{
			if (!IsConnected())
				return false;

			if (0 == mCircularBuffer.GetContiguiousBytes())
				return true;

			RioIoContext* sendContext = new RioIoContext(mIndex, IO_SEND);

			sendContext->BufferId = mRioBufferId;
			sendContext->Length = static_cast<ULONG>(mCircularBuffer.GetContiguiousBytes());
			sendContext->Offset = mCircularBuffer.GetReadableOffset();

			DWORD sendbytes = 0;
			DWORD flags = 0;

			/// start async send
			if (!RIO.RIOSend(mRequestQueue, (PRIO_BUF)sendContext, 1, flags, sendContext))
			{
				printf_s("[DEBUG] RIOSend error: %d\n", GetLastError());
				//ReleaseContext(sendContext);
				return false;
			}

			return true;
		}

		void SendCompletion(DWORD transferred)
		{
			mCircularBuffer.Remove(transferred);
		}

		void Disconnect(DisconnectReason dr)
		{
			FastSpinlockGuard criticalSection(mSessionLock);

			if (!IsConnected())
				return;

			LINGER lingerOption;
			lingerOption.l_onoff = 1;
			lingerOption.l_linger = 0;

			/// no TCP TIME_WAIT
			if (SOCKET_ERROR == setsockopt(mSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOption, sizeof(LINGER)))
			{
				//TODO: 모든 printf_s 로그로 바꾸기
				printf_s("[DEBUG] setsockopt linger option error: %d\n", GetLastError());
			}

			//printf_s("[DEBUG] Client Disconnected: Reason=%d IP=%s, PORT=%d \n", dr, inet_ntoa(mClientAddr.sin_addr), ntohs(mClientAddr.sin_port));

			closesocket(mSocket);

			mConnected = false;
			mSocket = NULL;

			ReleaseRef();
		}

		void	AddRef()
		{
			InterlockedIncrement(&mRefCount);
		}

		void	ReleaseRef()
		{
			long ret = InterlockedDecrement(&mRefCount);			
			if (ret == 0)
			{
				//TODO: 구현하기
				//GSessionManager->ReturnClientSession(this);
			}
		}



	private:
		bool	RioInitialize()
		{
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);
			const unsigned __int64 granularity = systemInfo.dwAllocationGranularity; ///< maybe 64K

			CRASH_ASSERT(SESSION_BUFFER_SIZE % granularity == 0);

			mRioBufferPointer = reinterpret_cast<char*>(VirtualAllocEx(GetCurrentProcess(), 0, SESSION_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
			if (mRioBufferPointer == nullptr)
			{
				printf_s("[DEBUG] VirtualAllocEx Error: %d\n", GetLastError());
				return false;
			}

			mCircularBuffer.Init(mRioBufferPointer, SESSION_BUFFER_SIZE);
			
			mRioBufferId = RIO.RIORegisterBuffer(mRioBufferPointer, SESSION_BUFFER_SIZE);

			if (mRioBufferId == RIO_INVALID_BUFFERID)
			{
				printf_s("[DEBUG] RIORegisterBuffer Error: %d\n", GetLastError());
				return false;
			}

			return true;
		}

		char* mRioBufferPointer = nullptr;
		RIO_BUFFERID	mRioBufferId = NULL;
		CircularBuffer mCircularBuffer;

		RIO_RQ			mRequestQueue;

	private:
		bool			mConnected = false;
		int				mIndex= -1;
		int				mRioThreadId = -1;

		SOCKET			mSocket = INVALID_SOCKET;
		SOCKADDR_IN		mClientAddr;

		FastSpinlock	mSessionLock;

		volatile long	mRefCount = 0;

		friend class SessionManager;
	};

}