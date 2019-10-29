#pragma once

#include "CircularMemory.h"

namespace C2RIONetLib
{
	class RingBufferVA2
	{
	public:
		RingBufferVA2() = default;

		~RingBufferVA2()
		{
			CircularMemory::Release(mRingBuffer, mSecondaryView);
		}

		char* GetBuffer() { return mRingBuffer; }


		int Create(const bool isWriteOnly_, const UINT32 ringBufferSize_)
		{
			mIsWriteOnly = isWriteOnly_;

			auto [pRingBuffer, errorCode] = CircularMemory::AllocMemory(ringBufferSize_, &mSecondaryView);

			if (pRingBuffer == nullptr)
			{
				printf("CreateRingBuffer failed\n");
				return errorCode;
			}

			mRingBuffer = pRingBuffer;
			mRingBufferSize = ringBufferSize_;

			Clear();

			return errorCode;
		}

		void Clear()
		{
			mWritePos = 0;
			mReadPos = 0;
		}
		
		char* Read(bool isPeek_, const UINT32 reqReadSize_)
		{
			char* pResult = nullptr;
			pResult = &mRingBuffer[mReadPos];

			if (isPeek_) { return pResult; }

			mReadPos += reqReadSize_;
			if (mReadPos >= mRingBufferSize) { mReadPos -= mRingBufferSize;	}

			return pResult;
		}

		char* Write(const UINT32 size_, const char* pData_)
		{
			if (mIsWriteOnly == false && CheckRemainingSpace(size_) == false) { return nullptr; }
			
			int oldPos = mWritePos;
			CopyMemory(&mRingBuffer[mWritePos], pData_, size_);
			mWritePos += size_;

			if (mWritePos >= mRingBufferSize)
			{
				mWritePos -= mRingBufferSize;
			}

			return &mRingBuffer[oldPos];
		}

		bool Write(const UINT32 size_)
		{
			if (mIsWriteOnly == false && CheckRemainingSpace(size_) == false) { return false; }

			mWritePos += size_;

			if (mWritePos >= mRingBufferSize) {	mWritePos -= mRingBufferSize; }

			return true;
		}

		void MoveWriteablePos(const UINT32 pos_) { mWritePos = pos_; }
		UINT32 GetWriteablePos() { return mWritePos; }

		void UnitTest_SetWritePos(UINT32 pos) { mWritePos = pos; }
		void UnitTest_SetReadPos(UINT32 pos) { mReadPos = pos; }


	private:
		bool CheckRemainingSpace(const UINT32 desiredSize_)
		{			
			if (mReadPos <= mWritePos)
			{
				INT32 remain = 0;
				if (mWritePos >= mRingBufferSize)
				{
					remain = ((INT32)mReadPos - (INT32)(mWritePos - mRingBufferSize));
				}
				else
				{
					remain = (mRingBufferSize - mWritePos) + mReadPos;
				}
				
				if (remain < (INT32)desiredSize_) { return false; }
			}
			else
			{
				auto remain = mReadPos - mWritePos;
				if (remain < desiredSize_) { return false; }
			}

			return true;
		}

		bool mIsWriteOnly = false;
		UINT32 mRingBufferSize = 0;
		char* mRingBuffer = nullptr;
		void* mSecondaryView = nullptr;
		UINT32 mWritePos = 0;
		UINT32 mReadPos = 0;

	};
}