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
		~SendRingBuffer() { delete ring_buffer_; }

		char* GetBuffer() { return ring_buffer_; }

		UINT32 EnableBufferSize() { return ring_buffer_size_; }

		UINT32 GetReadPos() { return read_pos_; }

		bool Create(const UINT32 ringBufferSize_)
		{
			ring_buffer_size_ = ringBufferSize_;
			ring_buffer_ = new char[ring_buffer_size_];
			ring_buffer_half_size_ = ring_buffer_size_ / 2;

			if (ring_buffer_ == nullptr) { 
				return false; 
			}

			Clear();
			return true;
		}

		void Clear()
		{
			write_pos_ = 0;
			read_pos_ = 0;
		}

		bool Write(const UINT32 size_, const char* pData_)
		{
			if ((write_pos_ + size_) >= ring_buffer_size_)
			{
				return false;
			}

			CopyMemory(&ring_buffer_[write_pos_], pData_, size_);
			write_pos_ += size_;
			return true;
		}

		UINT32 SendAbleSize() {	return write_pos_ - read_pos_; }

		void IncremantReadPos(const UINT32 pos) { read_pos_ += pos; }

		void MoveWrittenDataForward()
		{
			auto remainData = write_pos_ - read_pos_;

			if (remainData == 0)
			{
				read_pos_ = 0;
				write_pos_ = 0;
			}
			else
			{
				if (IsMoveWrittenDataForward(remainData))
				{
					CopyMemory(&ring_buffer_[0], &ring_buffer_[read_pos_], remainData);
					read_pos_ = 0;
					write_pos_ = remainData;
				}
			}
		}

	private:
		bool IsMoveWrittenDataForward(const UINT32 remainData)
		{
			if ( (write_pos_ > ring_buffer_half_size_) && (remainData < ring_buffer_half_size_) ) {
				return true;
			}

			return false;
		}


		UINT32 ring_buffer_size_ = 0;

		UINT32 ring_buffer_half_size_ = 0;
		char* ring_buffer_ = nullptr;
		UINT32 write_pos_ = 0;
		UINT32 read_pos_ = 0;
	};
}