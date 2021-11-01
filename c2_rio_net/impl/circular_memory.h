#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <tuple>
#include <string>
#include <format>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// MS Docs에는 Kernel32.lib 이 필수 lib인데 아래 lib이 필수 lib임 -_-;;
#pragma comment(lib,"mincore.lib")

// 정리: https://docs.google.com/document/d/175VX3vthy_O88EvfYRaOJ-sf9XoBHmMAMzA5Cq5Pui8/edit?usp=sharing
//https://docs.microsoft.com/ko-kr/windows/win32/api/memoryapi/nf-memoryapi-virtualalloc2
//int main()
//{
//	char* ringBuffer;
//	void* secondaryView;
//	unsigned int bufferSize = 0x10000;
//
//	ringBuffer = (char*)CreateRingBuffer(bufferSize, &secondaryView);
//
//	if (ringBuffer == nullptr) {
//		printf("CreateRingBuffer failed\n");
//		return 0;
//	}
//
//	//
//	// Make sure the buffer wraps properly.
//	//
//
//	ringBuffer[0] = 'a';
//
//	if (ringBuffer[bufferSize] == 'a') {
//		printf("The buffer wraps as expected\n");
//	}
//
//	UnmapViewOfFile(ringBuffer);
//	UnmapViewOfFile(secondaryView);
//
//	return 0;
//}

namespace c2rionet
{	
	struct CircularMemory
	{
		static std::tuple<char*, int, std::string> AllocMemory(UINT32 buffer_size, _Outptr_ void** secondary_view)
		{
			int error_code = 0;
			std::string error_msg = "";
			BOOL result;
			HANDLE section = nullptr;
			SYSTEM_INFO sysInfo;
			void* ringBuffer = nullptr;
			void* placeholder1 = nullptr;
			void* placeholder2 = nullptr;
			void* view1 = nullptr;
			void* view2 = nullptr;
			*secondary_view = NULL;

			GetSystemInfo(&sysInfo);

			//bufferSize의 크기는 페이지 크기의 배수 이어야 한다.
			// 아래 주석은 dwAllocationGranularity(메모리 할당 단위.프로세스 주소 공간에서 특정 영역을 예약할 때 사용하는 단위 크기. 65,536 값)의 배수가 아니면 실패로 되어 있다.
			// 이것은 아마 MapViewOfFile 때문인 것 같은데 MapViewOfFile에는 크기가 dwAllocationGranularity의 배수이어야 한다.
			// 그런데 이 코드에서는 MapViewOfFile3 버전을 사용하기 때문에 dwAllocationGranularity 크기가 아닌 페이지 크기의 배수이면 된다.				
			/*if ((bufferSize % sysInfo.dwAllocationGranularity) != 0) {
				return nullptr;
			}*/

			//
			// Reserve a placeholder region where the buffer will be mapped.
			//

			// bufferSize의 크기는 4096의 배수이어야 한다!!!
			if ((buffer_size % sysInfo.dwPageSize) != 0) {
				return { nullptr, 6, "size of bufferSize is not a multiple of page size. must be a multiple of 4096" };
			}

			placeholder1 = (PCHAR)VirtualAlloc2(
				nullptr,
				nullptr,
				2 * buffer_size,
				MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
				PAGE_NOACCESS,
				nullptr, 0
			);

			if (placeholder1 == nullptr) {
				error_code = 1;
				error_msg = std::format("VirtualAlloc2 failed, errorCode: {0}", GetLastError());
				goto Exit;
			}

			//
			// Split the placeholder region into two regions of equal size.
			//
			result = VirtualFree(
				placeholder1,
				buffer_size,
				MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER
			);

			if (result == FALSE) {
				error_code = 2;
				error_msg = std::format("VirtualFreeEx failed, errorCode: {0}", GetLastError());
				goto Exit;
			}

			placeholder2 = (void*)((ULONG_PTR)placeholder1 + buffer_size);

			//
			// Create a pagefile-backed section for the buffer.
			//

			section = CreateFileMapping(
				INVALID_HANDLE_VALUE,
				nullptr,
				PAGE_READWRITE,
				0,
				buffer_size, nullptr
			);

			if (section == nullptr) {
				error_code = 3;
				error_msg = std::format("CreateFileMapping failed, errorCode: {0}", GetLastError());
				goto Exit;
			}

			//
			// Map the section into the first placeholder region.
			//

			view1 = MapViewOfFile3(
				section,
				nullptr,
				placeholder1,
				0,
				buffer_size,
				MEM_REPLACE_PLACEHOLDER,
				PAGE_READWRITE,
				nullptr, 0
			);

			if (view1 == nullptr) {
				error_code = 4;
				error_msg = std::format("MapViewOfFile3 failed, errorCode: {0}", GetLastError());
				goto Exit;
			}

			//
			// Ownership transferred, don’t free this now.
			//

			placeholder1 = nullptr;

			//
			// Map the section into the second placeholder region.
			//

			view2 = MapViewOfFile3(
				section,
				nullptr,
				placeholder2,
				0,
				buffer_size,
				MEM_REPLACE_PLACEHOLDER,
				PAGE_READWRITE,
				nullptr, 0
			);

			if (view2 == nullptr) {
				error_code = 5;
				error_msg = std::format("MapViewOfFile3 failed, errorCode: {0}", GetLastError());
				goto Exit;
			}

			//
			// Success, return both mapped views to the caller.
			//

			ringBuffer = view1;
			*secondary_view = view2;

			placeholder2 = nullptr;
			view1 = nullptr;
			view2 = nullptr;

		Exit:

			if (section != nullptr) {
				CloseHandle(section);
			}

			if (placeholder1 != nullptr) {
				VirtualFree(placeholder1, 0, MEM_RELEASE);
			}

			if (placeholder2 != nullptr) {
				VirtualFree(placeholder2, 0, MEM_RELEASE);
			}

			if (view1 != nullptr) {
				UnmapViewOfFileEx(view1, 0);
			}

			if (view2 != nullptr) {
				UnmapViewOfFileEx(view2, 0);
			}

			return { (char*)ringBuffer, error_code, error_msg };
		}

		static void Release(char* pRingBuffer, void* pSecondaryView)
		{
			UnmapViewOfFile(pRingBuffer);
			UnmapViewOfFile(pSecondaryView);
		}
	};
}