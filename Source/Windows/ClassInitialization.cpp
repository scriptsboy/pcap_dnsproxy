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

std::wstring Path, ErrorLogPath;
Configuration Parameter;
std::string LocalhostPTR[] = {("?"), ("?")};
timeval ReliableSocketTimeout = {RELIABLE_SOCKET_TIMEOUT, 0}, UnreliableSocketTimeout = {UNRELIABLE_SOCKET_TIMEOUT, 0};
PortTable PortList;
std::vector<uint16_t> AcceptTypeList;
std::vector<HostsTable> HostsList[2U], *HostsListUsing = &HostsList[0], *HostsListModificating = &HostsList[1U];
std::deque<DNSCacheData> DNSCacheList;
std::vector<AddressRange> AddressRangeList[2U], *AddressRangeUsing = &AddressRangeList[0], *AddressRangeModificating = &AddressRangeList[1U];
std::vector<ResultBlacklistTable> ResultBlacklistList[2U], *ResultBlacklistUsing = &ResultBlacklistList[0], *ResultBlacklistModificating = &ResultBlacklistList[1U];
std::mutex PortListLock, HostsListLock, DNSCacheListLock, AddressRangeLock, ResultBlacklistLock;
const char *DomainTable = (".-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"); //Preferred name syntax(Section 2.3.1 in RFC 1035)
AlternateSwapTable AlternateSwapList;
DNSCurveConfiguration DNSCurveParameter;

//Configuration class constructor
Configuration::Configuration(void)
{
	memset(this, 0, sizeof(Configuration));
	DomainTestOptions.DomainTest = nullptr, PaddingDataOptions.PaddingData = nullptr, LocalhostServerOptions.LocalhostServer = nullptr;
	try {
		DomainTestOptions.DomainTest = DomainTestOptions.DomainTest = new char[DOMAIN_MAXSIZE]();
		PaddingDataOptions.PaddingData = new char[ICMP_PADDING_MAXSIZE](); 
		LocalhostServerOptions.LocalhostServer = new char[DOMAIN_MAXSIZE]();
	}
	catch (std::bad_alloc)
	{
		delete[] DomainTestOptions.DomainTest;
		delete[] PaddingDataOptions.PaddingData;
		delete[] LocalhostServerOptions.LocalhostServer;

		WSACleanup();
		TerminateService();
		return;
	}

	return;
}

//Configuration class destructor
Configuration::~Configuration(void)
{
	delete[] DomainTestOptions.DomainTest;
	delete[] PaddingDataOptions.PaddingData;
	delete[] LocalhostServerOptions.LocalhostServer;

	return;
}

//HostsTable class constructor
HostsTable::HostsTable(void)
{
	memset(this, 0, sizeof(HostsTable) - sizeof(HostsTable::Response) - sizeof(HostsTable::Pattern) - sizeof(HostsTable::PatternString));
	return;
}

//AddressRange class constructor
AddressRange::AddressRange(void)
{
	memset(this, 0, sizeof(AddressRange));
	return;
}

//ResultBlacklistTable class constructor
ResultBlacklistTable::ResultBlacklistTable(void)
{
	memset(this, 0, sizeof(ResultBlacklistTable) - sizeof(std::vector<CompositeAddressData>) - sizeof(std::regex) - sizeof(std::string));
	return;
}

//PortTable class constructor
PortTable::PortTable(void)
{
	memset((PSTR)this + sizeof(SendData), 0, sizeof(PortTable) - sizeof(SendData));
	RecvData = nullptr;
	try {
		RecvData = new SOCKET_DATA[QUEUE_MAXLEN * QUEUE_PARTNUM]();
	}
	catch (std::bad_alloc)
	{
		WSACleanup();
		TerminateService();
		return;
	}

	return;
}

//PortTable class destructor
PortTable::~PortTable(void)
{
	delete[] RecvData;
	return;
}

//AlternateSwapTable class constructor
AlternateSwapTable::AlternateSwapTable(void)
{
	memset(this, 0, sizeof(AlternateSwapTable));
	try {
		PcapAlternateTimeout = new size_t[QUEUE_MAXLEN * QUEUE_PARTNUM]();
	}
	catch (std::bad_alloc)
	{
		WSACleanup();
		TerminateService();
		return;
	}

	return;
}

//DNSCurveConfiguration class constructor
DNSCurveConfiguration::DNSCurveConfiguration(void)
{
	memset(this, 0, sizeof(DNSCurveConfiguration));
	DNSCurveTarget.IPv4_ProviderName = nullptr, DNSCurveTarget.Alternate_IPv4_ProviderName = nullptr, DNSCurveTarget.IPv6_ProviderName = nullptr, DNSCurveTarget.Alternate_IPv6_ProviderName = nullptr;
	DNSCurveKey.Client_PublicKey = nullptr, DNSCurveKey.Client_SecretKey = nullptr;
	DNSCurveKey.IPv4_PrecomputationKey = nullptr, DNSCurveKey.Alternate_IPv4_PrecomputationKey = nullptr, DNSCurveKey.IPv6_PrecomputationKey = nullptr, DNSCurveKey.Alternate_IPv6_PrecomputationKey = nullptr;
	DNSCurveKey.IPv4_ServerPublicKey = nullptr, DNSCurveKey.Alternate_IPv4_ServerPublicKey = nullptr, DNSCurveKey.IPv6_ServerPublicKey = nullptr, DNSCurveKey.Alternate_IPv6_ServerPublicKey = nullptr;
	DNSCurveKey.IPv4_ServerFingerprint = nullptr, DNSCurveKey.Alternate_IPv4_ServerFingerprint = nullptr, DNSCurveKey.IPv6_ServerFingerprint = nullptr, DNSCurveKey.Alternate_IPv6_ServerFingerprint = nullptr;
	DNSCurveMagicNumber.IPv4_MagicNumber = nullptr, DNSCurveMagicNumber.Alternate_IPv4_MagicNumber = nullptr, DNSCurveMagicNumber.IPv6_MagicNumber = nullptr, DNSCurveMagicNumber.Alternate_IPv6_MagicNumber = nullptr;
	DNSCurveMagicNumber.IPv4_ReceiveMagicNumber = nullptr, DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber = nullptr, DNSCurveMagicNumber.IPv6_ReceiveMagicNumber = nullptr, DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber = nullptr;
	try {
	//DNSCurve Provider Names
		DNSCurveTarget.IPv4_ProviderName = new char[DOMAIN_MAXSIZE]();
		DNSCurveTarget.Alternate_IPv4_ProviderName = new char[DOMAIN_MAXSIZE]();
		DNSCurveTarget.IPv6_ProviderName = new char[DOMAIN_MAXSIZE]();
		DNSCurveTarget.Alternate_IPv6_ProviderName = new char[DOMAIN_MAXSIZE]();
	//DNSCurve Keys
		DNSCurveKey.Client_PublicKey = new uint8_t[crypto_box_PUBLICKEYBYTES]();
		DNSCurveKey.Client_SecretKey = new uint8_t[crypto_box_SECRETKEYBYTES]();
		DNSCurveKey.IPv4_PrecomputationKey = new uint8_t[crypto_box_BEFORENMBYTES]();
		DNSCurveKey.Alternate_IPv4_PrecomputationKey = new uint8_t[crypto_box_BEFORENMBYTES]();
		DNSCurveKey.IPv6_PrecomputationKey = new uint8_t[crypto_box_BEFORENMBYTES]();
		DNSCurveKey.Alternate_IPv6_PrecomputationKey = new uint8_t[crypto_box_BEFORENMBYTES]();
		DNSCurveKey.IPv4_ServerPublicKey = new uint8_t[crypto_box_PUBLICKEYBYTES]();
		DNSCurveKey.Alternate_IPv4_ServerPublicKey = new uint8_t[crypto_box_PUBLICKEYBYTES]();
		DNSCurveKey.IPv6_ServerPublicKey = new uint8_t[crypto_box_PUBLICKEYBYTES]();
		DNSCurveKey.Alternate_IPv6_ServerPublicKey = new uint8_t[crypto_box_PUBLICKEYBYTES]();
		DNSCurveKey.IPv4_ServerFingerprint = new uint8_t[crypto_box_PUBLICKEYBYTES]();
		DNSCurveKey.Alternate_IPv4_ServerFingerprint = new uint8_t[crypto_box_PUBLICKEYBYTES]();
		DNSCurveKey.IPv6_ServerFingerprint = new uint8_t[crypto_box_PUBLICKEYBYTES]();
		DNSCurveKey.Alternate_IPv6_ServerFingerprint = new uint8_t[crypto_box_PUBLICKEYBYTES]();
	//DNSCurve Magic Numbers
		DNSCurveMagicNumber.IPv4_ReceiveMagicNumber = new char[DNSCURVE_MAGIC_QUERY_LEN]();
		DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber = new char[DNSCURVE_MAGIC_QUERY_LEN]();
		DNSCurveMagicNumber.IPv6_ReceiveMagicNumber = new char[DNSCURVE_MAGIC_QUERY_LEN]();
		DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber = new char[DNSCURVE_MAGIC_QUERY_LEN]();
		DNSCurveMagicNumber.IPv4_MagicNumber = new char[DNSCURVE_MAGIC_QUERY_LEN]();
		DNSCurveMagicNumber.Alternate_IPv4_MagicNumber = new char[DNSCURVE_MAGIC_QUERY_LEN]();
		DNSCurveMagicNumber.IPv6_MagicNumber = new char[DNSCURVE_MAGIC_QUERY_LEN]();
		DNSCurveMagicNumber.Alternate_IPv6_MagicNumber = new char[DNSCURVE_MAGIC_QUERY_LEN]();
	}
	catch (std::bad_alloc)
	{
	//DNSCurve Provider Names
		delete[] DNSCurveTarget.IPv4_ProviderName;
		delete[] DNSCurveTarget.Alternate_IPv4_ProviderName;
		delete[] DNSCurveTarget.IPv6_ProviderName;
		delete[] DNSCurveTarget.Alternate_IPv6_ProviderName;
	//DNSCurve Keys
		delete[] DNSCurveKey.Client_PublicKey;
		delete[] DNSCurveKey.Client_SecretKey;
		delete[] DNSCurveKey.IPv4_PrecomputationKey;
		delete[] DNSCurveKey.Alternate_IPv4_PrecomputationKey;
		delete[] DNSCurveKey.IPv6_PrecomputationKey;
		delete[] DNSCurveKey.Alternate_IPv6_PrecomputationKey;
		delete[] DNSCurveKey.IPv4_ServerPublicKey;
		delete[] DNSCurveKey.Alternate_IPv4_ServerPublicKey;
		delete[] DNSCurveKey.IPv6_ServerPublicKey;
		delete[] DNSCurveKey.Alternate_IPv6_ServerPublicKey;
		delete[] DNSCurveKey.IPv4_ServerFingerprint;
		delete[] DNSCurveKey.Alternate_IPv4_ServerFingerprint;
		delete[] DNSCurveKey.IPv6_ServerFingerprint;
		delete[] DNSCurveKey.Alternate_IPv6_ServerFingerprint;
	//DNSCurve Magic Numbers
		delete[] DNSCurveMagicNumber.IPv4_ReceiveMagicNumber;
		delete[] DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber;
		delete[] DNSCurveMagicNumber.IPv6_ReceiveMagicNumber;
		delete[] DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber;
		delete[] DNSCurveMagicNumber.IPv4_MagicNumber;
		delete[] DNSCurveMagicNumber.Alternate_IPv4_MagicNumber;
		delete[] DNSCurveMagicNumber.IPv6_MagicNumber;
		delete[] DNSCurveMagicNumber.Alternate_IPv6_MagicNumber;

		WSACleanup();
		TerminateService();
		return;
	}

	return;
}

//DNSCurveConfiguration class destructor
DNSCurveConfiguration::~DNSCurveConfiguration(void)
{
//DNSCurve Provider Names
	delete[] DNSCurveTarget.IPv4_ProviderName;
	delete[] DNSCurveTarget.Alternate_IPv4_ProviderName;
	delete[] DNSCurveTarget.IPv6_ProviderName;
	delete[] DNSCurveTarget.Alternate_IPv6_ProviderName;
//DNSCurve Keys
	delete[] DNSCurveKey.Client_PublicKey;
	delete[] DNSCurveKey.Client_SecretKey;
	delete[] DNSCurveKey.IPv4_PrecomputationKey;
	delete[] DNSCurveKey.Alternate_IPv4_PrecomputationKey;
	delete[] DNSCurveKey.IPv6_PrecomputationKey;
	delete[] DNSCurveKey.Alternate_IPv6_PrecomputationKey;
	delete[] DNSCurveKey.IPv4_ServerPublicKey;
	delete[] DNSCurveKey.Alternate_IPv4_ServerPublicKey;
	delete[] DNSCurveKey.IPv6_ServerPublicKey;
	delete[] DNSCurveKey.Alternate_IPv6_ServerPublicKey;
	delete[] DNSCurveKey.IPv4_ServerFingerprint;
	delete[] DNSCurveKey.Alternate_IPv4_ServerFingerprint;
	delete[] DNSCurveKey.IPv6_ServerFingerprint;
	delete[] DNSCurveKey.Alternate_IPv6_ServerFingerprint;
//DNSCurve Magic Numbers
	delete[] DNSCurveMagicNumber.IPv4_ReceiveMagicNumber;
	delete[] DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber;
	delete[] DNSCurveMagicNumber.IPv6_ReceiveMagicNumber;
	delete[] DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber;
	delete[] DNSCurveMagicNumber.IPv4_MagicNumber;
	delete[] DNSCurveMagicNumber.Alternate_IPv4_MagicNumber;
	delete[] DNSCurveMagicNumber.IPv6_MagicNumber;
	delete[] DNSCurveMagicNumber.Alternate_IPv6_MagicNumber;

	return;
}
