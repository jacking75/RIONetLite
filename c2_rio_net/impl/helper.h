#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>
#include <cstdint>

namespace c2rionet
{
class Helper
{
public:
	Helper() = default;
	~Helper() = default;

	static std::int32_t GetAllocationGranularity()
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		return systemInfo.dwAllocationGranularity; ///< maybe 64K
	}

	static INT32 SystemCoreCount()
	{
		SYSTEM_INFO systemInfo;
		GetSystemInfo(&systemInfo);
		return (std::int32_t)systemInfo.dwNumberOfProcessors;
	}

	static std::int32_t GetBestIOBufferSize(std::uint32_t Buffer)
	{
		auto AllocationGranularity = GetAllocationGranularity();
		return (std::int32_t)((Buffer / AllocationGranularity + 1) * AllocationGranularity);
	}

	static std::int32_t GetNumberOfProcessors()
	{
		SYSTEM_INFO		SysInfo;
		GetSystemInfo(&SysInfo);
		return (std::int32_t)SysInfo.dwNumberOfProcessors;
	}

	static const std::wstring s2ws(const std::string& s)
	{
		auto slength = (int)s.length();
		auto len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, 0, 0);
		std::wstring r(len, L'\0');
		MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, &r[0], len);
		return r;
	}


	static const std::string ws2s(const std::wstring& s)
	{
		auto slength = (int)s.length();
		auto len = WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, 0, 0, 0, 0);
		std::string r(len, '\0');
		WideCharToMultiByte(CP_ACP, 0, s.c_str(), slength, &r[0], len, 0, 0);
		return r;
	}

	static std::int64_t CurrentMilliSecond()
	{
		auto curTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch());
		return curTime.count();
	}

	static std::int64_t CurrentSecond()
	{
		auto curTime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch());
		return curTime.count();
	}
};
	
}// namespace c2rionet