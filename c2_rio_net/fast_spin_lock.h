#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

namespace c2rionet
{
	class CustomSpinLockCriticalSection
	{
		const int SPINLOCK_COUNT = 1000;

	public:
		CustomSpinLockCriticalSection()
		{
			(void)InitializeCriticalSectionAndSpinCount(&mCS, SPINLOCK_COUNT);
		}

		~CustomSpinLockCriticalSection()
		{
			DeleteCriticalSection(&mCS);
		}
		
		void EnterLock()
		{
			EnterCriticalSection(&mCS);
		}

		void LeaveLock()
		{
			LeaveCriticalSection(&mCS);
		}
				

	private:
		CRITICAL_SECTION mCS;
	};


	class FastSpinlock
	{
	public:
		FastSpinlock() = default;
		~FastSpinlock() = default;

		void EnterLock()
		{
			for (int nloops = 0; ; nloops++)
			{
				if (InterlockedExchange(&mLockFlag, 1) == 0) { return; }

				UINT uTimerRes = 1;
				timeBeginPeriod(uTimerRes);
				Sleep((DWORD)min(10, nloops));
				timeEndPeriod(uTimerRes);
			}
		}

		void LeaveLock()
		{
			InterlockedExchange(&mLockFlag, 0);
		}

	private:
		FastSpinlock(const FastSpinlock& rhs);
		FastSpinlock& operator=(const FastSpinlock& rhs);

		volatile long mLockFlag = 0;
	};


	class FastSpinlockGuard
	{
	public:
		FastSpinlockGuard(FastSpinlock& lock) : mLock(lock)
		{
			mLock.EnterLock();
		}

		~FastSpinlockGuard()
		{
			mLock.LeaveLock();
		}

	private:
		FastSpinlock& mLock;
	};

	template<class LockType>  
	class SpinlockGuard
	{
	public:
		SpinlockGuard(LockType& lock) : mLock(lock)
		{
			mLock.EnterLock();
		}

		~SpinlockGuard()
		{
			mLock.LeaveLock();
		}

	private:
		LockType& mLock;
	};
}
