#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

namespace C2RIONetLib
{
	class CustomSpinLockCriticalSection
	{
		const int SPINLOCK_COUNT = 1000;

	public:
		CustomSpinLockCriticalSection()
		{
			(void)InitializeCriticalSectionAndSpinCount(&m_CS, SPINLOCK_COUNT);
		}

		~CustomSpinLockCriticalSection()
		{
			DeleteCriticalSection(&m_CS);
		}
		
		void EnterLock()
		{
			EnterCriticalSection(&m_CS);
		}

		void LeaveLock()
		{
			LeaveCriticalSection(&m_CS);
		}
				

	private:
		CRITICAL_SECTION m_CS;
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
				if (InterlockedExchange(&mLockFlag, 1) == 0)
					return;

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
//template <class TargetClass>
//class ClassTypeLock
//{
//public:
//	struct LockGuard
//	{
//		LockGuard()
//		{
//			TargetClass::mLock.EnterLock();
//		}
//
//		~LockGuard()
//		{
//			TargetClass::mLock.LeaveLock();
//		}
//
//	};
//
//private:
//	static FastSpinlock mLock;
//
//};
//
//template <class TargetClass>
//FastSpinlock ClassTypeLock<TargetClass>::mLock;