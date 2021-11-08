#pragma once

#include "circular_memory.h"

#include <string>
#include <tuple>
#include <cstdint>


namespace c2rionet
{
//RIO용 버퍼로는 사용할 수 없다!!
class RingBufferVA2
{
public:
	RingBufferVA2() = default;

	~RingBufferVA2()
	{
		CircularMemory::Release(ring_buffer_, secondary_view_);
	}

	char* GetBuffer() { return ring_buffer_; }
		
	std::tuple<int, std::string> Create(const bool is_write_only, const std::uint32_t ring_buffer_size)
	{
		is_write_only_ = is_write_only;

		auto [pRingBuffer, errorCode, error_msg] = CircularMemory::AllocMemory(ring_buffer_size, &secondary_view_);

		if (pRingBuffer == nullptr) {
			return { errorCode, error_msg };
		}

		ring_buffer_ = pRingBuffer;
		ring_buffer_size_ = ring_buffer_size;

		Clear();

		return { errorCode, error_msg };
	}

	// VirtualAlloc2의 링버퍼를 사용하고 있어서 주소로는 2배까지 사용 가능하다. 
	int EnableBufferSize() { return ring_buffer_size_ * 2; }
		
	void Clear()
	{
		write_pos_ = 0;
		read_pos_ = 0;
	}
		
	std::tuple<std::uint32_t, char*> Read()
	{
		char* pData = nullptr;
		std::uint32_t readAbleSize = 0;

		if (read_pos_ < write_pos_) {
			readAbleSize = write_pos_ - read_pos_;
			pData = &ring_buffer_[read_pos_];
		} else {
			readAbleSize = (ring_buffer_size_ - read_pos_) + write_pos_;
			pData = &ring_buffer_[read_pos_];
		}
			
		return { readAbleSize, pData };
	}
		
	void IncremantReadPos(const std::uint32_t pos)
	{
		read_pos_ += pos;

		if (read_pos_ >= ring_buffer_size_)
		{
			read_pos_ -= ring_buffer_size_;
		}
	}


	char* Write(const std::uint32_t size, const char* p_data)
	{
		if (is_write_only_ == false && CheckRemainingSpace(size) == false) { 
			return nullptr; 
		}
			
		int oldPos = write_pos_;
		CopyMemory(&ring_buffer_[write_pos_], p_data, size);
		write_pos_ += size;

		if (write_pos_ >= ring_buffer_size_) {
			write_pos_ -= ring_buffer_size_;
		}

		return &ring_buffer_[oldPos];
	}

	bool Write(const std::uint32_t size)
	{
		if (is_write_only_ == false && CheckRemainingSpace(size) == false) { 
			return false; 
		}

		write_pos_ += size;

		if (write_pos_ >= ring_buffer_size_) {	
			write_pos_ -= ring_buffer_size_; 
		}

		return true;
	}

	void MoveWriteablePos(const std::uint32_t pos) { write_pos_ = pos; }

	std::uint32_t GetWriteablePos() { return write_pos_; }

		
private:
	bool CheckRemainingSpace(const std::uint32_t desired_size)
	{			
		if (read_pos_ <= write_pos_)
		{
			std::int32_t remain = 0;

			if (write_pos_ >= ring_buffer_size_) {
				remain = ((std::int32_t)read_pos_ - (std::int32_t)(write_pos_ - ring_buffer_size_));
			} else {
				remain = (ring_buffer_size_ - write_pos_) + read_pos_;
			}
				
			if (remain < (std::int32_t)desired_size) {
				return false; 
			}
		} else {
			auto remain = read_pos_ - write_pos_;
			if (remain < desired_size) { 
				return false; 
			}
		}

		return true;
	}


	bool is_write_only_ = false;
	std::uint32_t ring_buffer_size_ = 0;
	char* ring_buffer_ = nullptr;
	void* secondary_view_ = nullptr;
	std::uint32_t write_pos_ = 0;
	std::uint32_t read_pos_ = 0;

};

}// namespace c2rionet