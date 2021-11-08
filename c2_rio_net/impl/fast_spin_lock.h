#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>

namespace c2rionet
{
class CustomSpinLockCriticalSection
{
	const int kSPIN_LOCK_COUNT = 1000;

public:
	CustomSpinLockCriticalSection()
	{
		(void)InitializeCriticalSectionAndSpinCount(&cs_lock_, kSPIN_LOCK_COUNT);
	}

	~CustomSpinLockCriticalSection()
	{
		DeleteCriticalSection(&cs_lock_);
	}
		
	void EnterLock()
	{
		EnterCriticalSection(&cs_lock_);
	}

	void LeaveLock()
	{
		LeaveCriticalSection(&cs_lock_);
	}
				

private:
	CRITICAL_SECTION cs_lock_;
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
			if (InterlockedExchange(&lock_flag_, 1) == 0) { return; }

			UINT uTimerRes = 1;
			timeBeginPeriod(uTimerRes);
			Sleep((DWORD)min(10, nloops));
			timeEndPeriod(uTimerRes);
		}
	}

	void LeaveLock()
	{
		InterlockedExchange(&lock_flag_, 0);
	}

private:
	FastSpinlock(const FastSpinlock& rhs);
	FastSpinlock& operator=(const FastSpinlock& rhs);

	volatile long lock_flag_ = 0;
};


class FastSpinlockGuard
{
public:
	FastSpinlockGuard(FastSpinlock& lock) : lock_(lock)
	{
		lock_.EnterLock();
	}

	~FastSpinlockGuard()
	{
		lock_.LeaveLock();
	}

private:
	FastSpinlock& lock_;
};

template<class LockType>  
class SpinlockGuard
{
public:
	SpinlockGuard(LockType& lock) : lock_(lock)
	{
		lock_.EnterLock();
	}

	~SpinlockGuard()
	{
		lock_.LeaveLock();
	}

private:
	LockType& lock_;
};

}// namespace c2rionet
