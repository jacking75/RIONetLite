#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <tuple>

namespace C2RIONetLib
{
	class PacketRingBuffer
	{
	public:
		PacketRingBuffer() = default;

		~PacketRingBuffer()
		{
			delete mRingBuffer;
		}

		char* GetBuffer() { return mRingBuffer; }

		bool Create(const bool isWriteOnly_, const unsigned int ringBufferSize_, const unsigned int maxPacketSize_)
		{
			mIsWriteOnly = isWriteOnly_;
			mRingBufferSize = ringBufferSize_;
			mRingBuffer = new char[mRingBufferSize*2];

			if (mRingBuffer == nullptr)
			{
				return false;
			}

			Clear();
			return true;
		}

		void Clear()
		{
			mWritePos = 0;
			mReadPos = 0;
		}


		std::tuple<unsigned int, char*> Read()
		{
			char* pData = nullptr;
			unsigned int readAbleSize = 0;

			if (mReadPos < mWritePos)
			{
				readAbleSize = mWritePos - mReadPos;
				pData = &mRingBuffer[mReadPos];
			}
			else
			{
				readAbleSize = (mRingBufferSize - mReadPos) + mWritePos;
				pData = &mRingBuffer[mReadPos];
			}

			return { readAbleSize, pData };
		}

		void IncremantReadPos(const unsigned int pos_)
		{
			mReadPos += pos_;

			if (mReadPos >= mRingBufferSize)
			{
				mReadPos -= mRingBufferSize;
			}
		}


		char* Write(const unsigned int size_, const char* pData_)
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

		bool Write(const unsigned int size_)
		{
			if (mIsWriteOnly == false && CheckRemainingSpace(size_) == false) { return false; }

			mWritePos += size_;

			if (mWritePos >= mRingBufferSize) { mWritePos -= mRingBufferSize; }

			return true;
		}

		void MoveWriteablePos(const unsigned int pos_) { mWritePos = pos_; }
		unsigned int GetWriteablePos() { return mWritePos; }

		
	private:
		bool CheckRemainingSpace(const unsigned int desiredSize_)
		{
			if (mReadPos <= mWritePos)
			{
				int remain = 0;
				if (mWritePos >= mRingBufferSize)
				{
					remain = ((int)mReadPos - (int)(mWritePos - mRingBufferSize));
				}
				else
				{
					remain = (mRingBufferSize - mWritePos) + mReadPos;
				}

				if (remain < (int)desiredSize_) { return false; }
			}
			else
			{
				auto remain = mReadPos - mWritePos;
				if (remain < desiredSize_) { return false; }
			}

			return true;
		}

		bool mIsWriteOnly = false;
		unsigned int mRingBufferSize = 0;
		char* mRingBuffer = nullptr;
		unsigned int mWritePos = 0;
		unsigned int mReadPos = 0;

	};
}