#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdio.h>
#include <tuple>
#include <cstdint>

namespace c2rionet
{
	class SendRingBuffer
	{
	public:
		SendRingBuffer() = default;
		~SendRingBuffer() { delete ring_buffer_; }

		char* GetBuffer() { return ring_buffer_; }

		std::uint32_t EnableBufferSize() { return ring_buffer_size_; }

		std::uint32_t GetReadPos() { return read_pos_; }

		//ring_buffer_size 사이즈는 가급적 크게 하는 것이 좋다
		void Create(const std::uint32_t ring_buffer_size)
		{
			ring_buffer_size_ = ring_buffer_size;
			ring_buffer_ = new char[ring_buffer_size_];
			ring_buffer_half_size_ = ring_buffer_size_ / 2;
				
			Clear();
		}

		void Clear()
		{
			write_pos_ = 0;
			read_pos_ = 0;
		}

		bool Write(const std::uint32_t size, const char* p_data)
		{
			if ((write_pos_ + size) >= ring_buffer_size_) {
				return false;
			}

			CopyMemory(&ring_buffer_[write_pos_], p_data, size);
			write_pos_ += size;
			return true;
		}

		std::uint32_t SendAbleSize() {	return write_pos_ - read_pos_; }

		void IncremantReadPos(const std::uint32_t pos) { read_pos_ += pos; }

		void MoveWrittenDataForward()
		{
			auto remainData = write_pos_ - read_pos_;

			//대체적으로 버퍼의 50%를 사용하기 전에 send가 다 완료 되므로remainData == 0 조건이 호출될 확률이 아주 높다
			if (remainData == 0) {
				read_pos_ = 0;
				write_pos_ = 0;
			} else {
				if (IsMoveWrittenDataForward(remainData)) {
					CopyMemory(&ring_buffer_[0], &ring_buffer_[read_pos_], remainData);
					read_pos_ = 0;
					write_pos_ = remainData;
				}
			}
		}


	private:
		bool IsMoveWrittenDataForward(const std::uint32_t remain_data)
		{
			if ( (write_pos_ > ring_buffer_half_size_) && (remain_data < ring_buffer_half_size_) ) {
				return true;
			}

			return false;
		}



		std::uint32_t ring_buffer_size_ = 0;
		std::uint32_t ring_buffer_half_size_ = 0;
		char* ring_buffer_ = nullptr;

		std::uint32_t write_pos_ = 0;
		std::uint32_t read_pos_ = 0;
	};
}