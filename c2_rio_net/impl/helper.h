#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>


namespace c2rionet
{
	class Helper
	{
	public:
		Helper() = default;
		~Helper() = default;

		static INT32 GetAllocationGranularity()
		{
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);
			return systemInfo.dwAllocationGranularity; ///< maybe 64K
		}

		static INT32 SystemCoreCount()
		{
			SYSTEM_INFO systemInfo;
			GetSystemInfo(&systemInfo);
			return (INT32)systemInfo.dwNumberOfProcessors;
		}

		static INT32 GetBestIOBufferSize(UINT32 Buffer)
		{
			auto AllocationGranularity = GetAllocationGranularity();
			return (INT32)((Buffer / AllocationGranularity + 1) * AllocationGranularity);
		}

		static INT32 GetNumberOfProcessors()
		{
			SYSTEM_INFO		SysInfo;
			GetSystemInfo(&SysInfo);
			return (INT32)SysInfo.dwNumberOfProcessors;
		}

		static const std::wstring s2ws(const std::string& s)
		{
			int slength = (int)s.length();
			int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
			std::wstring r(len, L'\0');
			MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &r[0], len);
			return r;
		}


		static const std::string ws2s(const std::wstring& s)
		{
			int slength = (int)s.length();
			int len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
			std::string r(len, '\0');
			WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, &r[0], len, 0, 0);
			return r;
		}

		static INT64 CurrentMilliSecond()
		{
			auto curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
			return curTime.count();
		}

		static INT64 CurrentSecond()
		{
			auto curTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch());
			return curTime.count();
		}
	};
	
}