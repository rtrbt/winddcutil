#include <WinSDKVer.h>
#define _WIN32_WINNT 0x0601
#include <SDKDDKVer.h>

#undef UNICODE
#define UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <strsafe.h>
#include <shellapi.h>
#include <atlstr.h>
#include <PhysicalMonitorEnumerationAPI.h>
#include <LowLevelMonitorConfigurationAPI.h>

#include <iostream>
#include <vector>
#include <thread>
#include <string>
#include <unordered_map>
#include <sstream>
#include <algorithm>

std::vector<PHYSICAL_MONITOR> physicalMonitorHandles{};

//called by EnumDisplayMonitors
BOOL CALLBACK GetPhysicalMonitorsCallback(
	_In_ HMONITOR hMonitor,
	_In_ HDC      hdcMonitor,
	_In_ LPRECT   lprcMonitor,
	_In_ LPARAM   dwData)
{
	UNREFERENCED_PARAMETER(hdcMonitor);
	UNREFERENCED_PARAMETER(lprcMonitor);
	UNREFERENCED_PARAMETER(dwData);
	DWORD cPhysicalMonitors;
	BOOL bSuccess = GetNumberOfPhysicalMonitorsFromHMONITOR(
		hMonitor,
		&cPhysicalMonitors
	);

	if (bSuccess)
	{
		auto oldHandleCount = physicalMonitorHandles.size();
		physicalMonitorHandles.resize(physicalMonitorHandles.size() + cPhysicalMonitors);
		bSuccess = GetPhysicalMonitorsFromHMONITOR(
			hMonitor, cPhysicalMonitors, physicalMonitorHandles.data() + oldHandleCount);
	}
	return true;
}

std::string toUTF8String(wchar_t *buffer)
{
	CW2A utf8(buffer, CP_UTF8);
	const char* data = utf8.m_psz;
	return std::string{ data };
}

bool cleanup()
{
	BOOL bSuccess = DestroyPhysicalMonitors(
		physicalMonitorHandles.size(),
		physicalMonitorHandles.data());
	return bSuccess;
}



void printUsage();

int detect(std::vector<std::string> args)
{
	int i = 0;
	for (auto &display : physicalMonitorHandles)
	{
		std::cout << "Display " << i << std::endl;
		std::cout << "\t" << " Name: " << toUTF8String(display.szPhysicalMonitorDescription) << std::endl;
		++i;
	}
	return 0;
}

int capabilities(std::vector<std::string> args)
{
	if (args.size() < 1)
	{
		std::cerr << "Missing display id" << std::endl;
		printUsage();
		return 1;
	}
	size_t displayId;
	try
	{
		displayId = std::stoul(args[0]);
	}
	catch (...)
	{
		std::cerr << "Could not parse capabilities parameter." << std::endl;
		printUsage();
		return 1;
	}

	auto monitorHandle = physicalMonitorHandles[displayId].hPhysicalMonitor;

	DWORD capStringLen;	
	auto result = GetCapabilitiesStringLength(monitorHandle, &capStringLen);
	if (!result)
	{
		std::cerr << "Failed to get capability string length" << std::endl;
		return 1;
	}

	std::unique_ptr<CHAR[]> capString{ new CHAR[capStringLen] };
	result = CapabilitiesRequestAndCapabilitiesReply(monitorHandle, capString.get(), capStringLen);
	if (!result)
	{
		std::cerr << "Failed to get capability string" << std::endl;
		return 1;
	}

	std::cout << std::string(capString.get()) << std::endl;
	return 0;
}

int setVcp(std::vector<std::string> args)
{
	if (args.size() < 3)
	{
		std::cerr << "Missing parameters" << std::endl;
		printUsage();
		return 1;
	}
	size_t displayId;
	uint8_t vcpCode;
	uint32_t vcpValue;
	try
	{
		displayId = std::stoul(args[0]);
		int vcpInt = std::stoi(args[1]);
		if (vcpInt > 255)
			throw std::out_of_range{ "VcpCode has to be less than 255." };
		vcpCode = static_cast<uint8_t>(vcpInt);
		vcpValue = std::stoul(args[2]);
	}
	catch (...)
	{
		std::cerr << "Could not parse setvcp parameters." << std::endl;
		printUsage();
		return 1;
	}
	bool result = SetVCPFeature(physicalMonitorHandles[displayId].hPhysicalMonitor, vcpCode, vcpValue);
	if (!result)
		std::cerr << "Failed to set vcp feature" << std::endl;
	return result ? 0 : 1;
}

std::unordered_map < std::string, std::tuple<std::function<int(std::vector<std::string>)>, std::string, std::string>> commands
{   //command name, command fn, parameter names, description
	{ "detect",{ detect, "", "Detect monitors" } },
	{ "capabilities",{ capabilities, "<display ID>", "Query monitor capabilities string" } },
	{ "setvcp",{ setVcp, "<display ID> <feature-code> <new-value>", "Set VCP feature value" } }
};

void printUsage()
{
	std::cout << "Usage: winddcutil command" << std::endl << std::endl;
	std::cout << "Commands:" << std::endl;

	//find longest parameter description
	auto maxIt = std::max_element(commands.begin(), commands.end(),
		
		[](const  decltype(commands)::value_type &left,
		   const  decltype(commands)::value_type &right)
		{
			return left.first.length() + std::get<1>(left.second).length() < right.first.length() + std::get<1>(right.second).length();
		}
	);
	auto maxLen = maxIt->first.length() + std::get<1>(maxIt->second).length() + 2; //space between command name and parameters and space between parameters and description
	for (auto &it : commands)
	{
		auto &name = it.first;
		auto &params = std::get<1>(it.second);
		auto &description = std::get<2>(it.second);
		std::cout << "\t" << name << " " << params;

		for (size_t i = 0; i < maxLen - params.length() - name.length(); ++i)
			std::cout << " ";
		std::cout << description << std::endl;
	}
}

int runProgram(std::vector<std::string> args)
{
	EnumDisplayMonitors(NULL, NULL, &GetPhysicalMonitorsCallback, 0);
	if (args.size() < 2)
	{
		std::cerr << "Missing command" << std::endl;
		printUsage();
		return 1;
	}

	auto it = commands.find(args[1]);
	if (it == commands.end())
	{
		std::cerr << "Unknown command" << std::endl;
		printUsage();
		return 1;
	}
	args.erase(args.begin()); //remove program name
	args.erase(args.begin()); //remove command name
	auto result = std::get<0>(it->second)(args);
	if (!cleanup())
	{
		std::cerr << "Failed to clean up created handles" << std::endl;
		return 1;
	}
	return result;
}

int wmain(int argc, wchar_t* argv[])
{
	std::vector<std::string> args;
	args.reserve(argc);

	for (int i = 0; i < argc; ++i)
	{
		CW2A utf8(argv[i], CP_UTF8);
		const char* data = utf8.m_psz;
		args.emplace_back(std::string{ data });
	}
		
	return runProgram(args);
}