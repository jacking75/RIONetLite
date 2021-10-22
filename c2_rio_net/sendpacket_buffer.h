#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <tuple>

namespace c2rionet
{
	class SendRingBuffer
	{
	public:
		SendRingBuffer() = default;
		~SendRingBuffer() { delete mRingBuffer; }

		char* GetBuffer() { return mRingBuffer; }

		UINT32 EnableBufferSize() { return mRingBufferSize; }

		//void MoveWriteablePos(const unsigned int pos_) { mWritePos = pos_; }
		UINT32 GetReadPos() { return mReadPos; }

		bool Create(const UINT32 ringBufferSize_)
		{
			mRingBufferSize = ringBufferSize_;
			mRingBuffer = new char[mRingBufferSize];
			mRingBufferHalfSize = mRingBufferSize / 2;

			if (mRingBuffer == nullptr) { return false; }

			Clear();
			return true;
		}

		void Clear()
		{
			mWritePos = 0;
			mReadPos = 0;
		}

		bool Write(const UINT32 size_, const char* pData_)
		{
			if ((mWritePos + size_) >= mRingBufferSize)
			{
				return false;
			}

			CopyMemory(&mRingBuffer[mWritePos], pData_, size_);
			mWritePos += size_;
			return true;
		}

		UINT32 SendAbleSize() {	return mWritePos - mReadPos; }

		void IncremantReadPos(const UINT32 pos) { mReadPos += pos; }

		void MoveWrittenDataForward()
		{
			auto remainData = mWritePos - mReadPos;

			if (remainData == 0)
			{
				mReadPos = 0;
				mWritePos = 0;
			}
			else
			{
				if (IsMoveWrittenDataForward(remainData))
				{
					CopyMemory(&mRingBuffer[0], &mRingBuffer[mReadPos], remainData);
					mReadPos = 0;
					mWritePos = remainData;
				}
			}
		}

	private:
		bool IsMoveWrittenDataForward(const UINT32 remainData)
		{
			if ( (mWritePos > mRingBufferHalfSize) && (remainData < mRingBufferHalfSize) ) {
				return true;
			}

			return false;
		}


		UINT32 mRingBufferSize = 0;

		UINT32 mRingBufferHalfSize = 0;
		char* mRingBuffer = nullptr;
		UINT32 mWritePos = 0;
		UINT32 mReadPos = 0;
	};
}