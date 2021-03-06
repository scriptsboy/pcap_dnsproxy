// This code is part of Pcap_DNSProxy
// Pcap_DNSProxy, A local DNS server base on WinPcap and LibPcap.
// Copyright (C) 2012-2014 Chengr28
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.


#include "Pcap_DNSProxy.h"

//#define QUERY_SERVICE_CONFIG_BUFFER_MAXSIZE 8192U //8192 bytes/8KB

extern std::wstring Path, ErrorLogPath;
extern Configuration Parameter;
extern DNSCurveConfiguration DNSCurveParameter;

//The Main function of program
int main(int argc, char *argv[])
{
#ifdef _DEBUG
//Handle the system signal.
	SetConsoleCtrlHandler((PHANDLER_ROUTINE)CtrlHandler, TRUE);
#endif

	if (argc > 0)
	{
		std::shared_ptr<wchar_t> wPath(new wchar_t[MAX_PATH]());
	//Path initialization and Winsock initialization.
		MultiByteToWideChar(CP_ACP, NULL, argv[0], MBSTOWCS_NULLTERMINATE, wPath.get(), MAX_PATH);
		if (FileInit(wPath.get()) == EXIT_FAILURE)
			return EXIT_FAILURE;
		wPath.reset();

	//Windows Firewall Test in first start.
		if (argc > 1 && strlen(argv[1U]) == strlen("--FirstStart") && memcmp(argv[1], ("--FirstStart"), strlen("--FirstStart")) == 0)
		{
			if (FirewallTest(AF_INET6) == EXIT_FAILURE && FirewallTest(AF_INET) == EXIT_FAILURE)
			{
				PrintError(WINSOCK_ERROR, L"Windows Firewall Test error", NULL, nullptr, NULL);

				WSACleanup();
				return EXIT_FAILURE;
			}
			else {
				return EXIT_SUCCESS;
			}
		}
	}
	else {
		return EXIT_FAILURE;
	}

//Read configuration file and WinPcap initialization.
	if (ReadParameter() == EXIT_FAILURE)
	{
		WSACleanup();
		return EXIT_FAILURE;
	}

//Get Localhost DNS PTR Records.
	std::thread IPv6LocalAddressThread(LocalAddressToPTR, AF_INET6);
	std::thread IPv4LocalAddressThread(LocalAddressToPTR, AF_INET);
	IPv6LocalAddressThread.detach();
	IPv4LocalAddressThread.detach();
	
//Read IPFilter, start DNS Cache monitor(Timer type) and read Hosts.
	if (Parameter.FileRefreshTime > 0)
	{
		if (Parameter.CacheType != 0)
		{
			std::thread DNSCacheTimerThread(DNSCacheTimerMonitor, Parameter.CacheType);
			DNSCacheTimerThread.detach();
		}

		if (Parameter.OperationMode == LISTEN_CUSTOMMODE || Parameter.Blacklist)
		{
			std::thread IPFilterThread(ReadIPFilter);
			IPFilterThread.detach();
		}

		std::thread HostsThread(ReadHosts);
		HostsThread.detach();
	}

//DNSCurve initialization
	if (Parameter.DNSCurve && DNSCurveParameter.Encryption)
	{
		randombytes_set_implementation(&randombytes_salsa20_implementation);
		DNSCurveInit();
	}

//Service initialization and start service.
	SERVICE_TABLE_ENTRYW ServiceTable[] = {{LOCAL_SERVICENAME, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain}, {nullptr, NULL}};
	if (!StartServiceCtrlDispatcherW(ServiceTable))
	{
		PrintError(SYSTEM_ERROR, L"Service start error", GetLastError(), nullptr, NULL);
	//Switch to run as a program.
		MonitorInit();

		WSACleanup();
		return EXIT_FAILURE;
	}

	WSACleanup();
	return EXIT_SUCCESS;
}

//Get path of program from the main function parameter and Winsock initialization
inline size_t __fastcall FileInit(const PWSTR wPath)
{
/* Get path of program from server information.
//Prepare
	SC_HANDLE SCM = nullptr, Service = nullptr;
	DWORD nResumeHandle = 0;

	if ((SCM = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS)) == nullptr)
		return EXIT_FAILURE;
 
	Service = OpenService(SCM, LOCAL_SERVICENAME, SERVICE_ALL_ACCESS);
	if (Service == nullptr)
		return EXIT_FAILURE;

	LPQUERY_SERVICE_CONFIG ServicesInfo = (LPQUERY_SERVICE_CONFIG)LocalAlloc(LPTR, QUERY_SERVICE_CONFIG_BUFFER_MAXSIZE);
	if (ServicesInfo == nullptr)
		return EXIT_FAILURE;

	if (QueryServiceConfig(Service, ServicesInfo, QUERY_SERVICE_CONFIG_BUFFER_MAXSIZE, &nResumeHandle) == FALSE)
	{
		LocalFree(ServicesInfo);
		return EXIT_FAILURE;
	}
	Path = ServicesInfo->lpBinaryPathName;
	LocalFree(ServicesInfo);
*/
//Path process.
	Path = wPath;
	Path.erase(Path.rfind(L"\\") + 1U);

	for (size_t Index = 0;Index < Path.length();Index++)
	{
		if (Path[Index] == L'\\')
		{
			Path.insert(Index, L"\\");
			Index++;
		}
	}

//Get path of error log file and delete the old one.
	ErrorLogPath = Path;
	ErrorLogPath.append(L"Error.log");
	DeleteFileW(ErrorLogPath.c_str());
	Parameter.PrintError = true;

//Winsock initialization
	WSAData WSAInitialization = {0};
	if (WSAStartup(MAKEWORD(2, 2), &WSAInitialization) != 0 || LOBYTE(WSAInitialization.wVersion) != 2 || HIBYTE(WSAInitialization.wVersion) != 2)
	{
		PrintError(WINSOCK_ERROR, L"Winsock initialization error", WSAGetLastError(), nullptr, NULL);

		WSACleanup();
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

//Windows Firewall Test
inline size_t __fastcall FirewallTest(const uint16_t Protocol)
{
	SYSTEM_SOCKET FirewallSocket = 0;
	sockaddr_storage SockAddr = {0};

//Ramdom number generator initialization
	std::random_device RamdomDevice;
	std::mt19937 RamdomEngine(RamdomDevice()); //Mersenne Twister Engine
	std::uniform_int_distribution<int> Distribution(1, U16_MAXNUM);
	auto RamdomGenerator = std::bind(Distribution, RamdomEngine);

//Socket initialization
	if (Protocol == AF_INET6) //IPv6
	{
		FirewallSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		SockAddr.ss_family = AF_INET6;
		((PSOCKADDR_IN6)&SockAddr)->sin6_addr = in6addr_any;
		((PSOCKADDR_IN6)&SockAddr)->sin6_port = htons((uint16_t)RamdomGenerator());

	//Bind local socket.
		if (FirewallSocket == INVALID_SOCKET)
		{
			return EXIT_FAILURE;
		}
		else if (bind(FirewallSocket, (PSOCKADDR)&SockAddr, sizeof(sockaddr_in6)) == SOCKET_ERROR)
		{
			closesocket(FirewallSocket);
			return EXIT_FAILURE;
		}
	}
	else { //IPv4
		FirewallSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		SockAddr.ss_family = AF_INET;
		((PSOCKADDR_IN)&SockAddr)->sin_addr.S_un.S_addr = INADDR_ANY;
		((PSOCKADDR_IN)&SockAddr)->sin_port = htons((uint16_t)RamdomGenerator());

	//Bind local socket.
		if (FirewallSocket == INVALID_SOCKET)
		{
			return EXIT_FAILURE;
		}
		else if (bind(FirewallSocket, (PSOCKADDR)&SockAddr, sizeof(sockaddr_in)) == SOCKET_ERROR)
		{
			closesocket(FirewallSocket);
			return EXIT_FAILURE;
		}
	}

	closesocket(FirewallSocket);
	return EXIT_SUCCESS;
}
