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

std::string PcapFilterRules;
std::vector<std::string> PcapRunning;

extern Configuration Parameter;
extern PortTable PortList;
extern std::deque<DNSCacheData> DNSCacheList;
extern std::mutex DNSCacheListLock;
extern AlternateSwapTable AlternateSwapList;

//Capture initialization
size_t __fastcall CaptureInit(void)
{
//Initialization
	std::shared_ptr<char> ErrBuffer(new char[PCAP_ERRBUF_SIZE]());
	std::shared_ptr<wchar_t> wErrBuffer(new wchar_t[PCAP_ERRBUF_SIZE]());
	FilterRulesInit(PcapFilterRules);
	pcap_if *pThedevs = nullptr;
	pcap_if *pDrive = nullptr;
	std::vector<std::string>::iterator CaptureIter;
	std::string DeviceName;

//Capture Monitor
	size_t ErrorIndex = 0;
	while (true)
	{
	//Retry limit
		if (ErrorIndex == PCAP_FINALLDEVS_RETRY)
			break;

	//Open all devices
		if (pcap_findalldevs(&pThedevs, ErrBuffer.get()) == RETURN_ERROR)
		{
			MultiByteToWideChar(CP_ACP, NULL, ErrBuffer.get(), MBSTOWCS_NULLTERMINATE, wErrBuffer.get(), PCAP_ERRBUF_SIZE);
			PrintError(WINPCAP_ERROR, wErrBuffer.get(), NULL, nullptr, NULL);
			memset(ErrBuffer.get(), 0, PCAP_ERRBUF_SIZE);
			memset(wErrBuffer.get(), 0, PCAP_ERRBUF_SIZE * sizeof(wchar_t));

			ErrorIndex++;
			Sleep(PCAP_DEVICESRECHECK_TIME * TIME_OUT);
			continue;
		}

	//Permissions check and check available network devices.
		if (pThedevs == nullptr)
		{
			PrintError(WINPCAP_ERROR, L"Insufficient permissions or not any available network devices", NULL, nullptr, NULL);

			ErrorIndex++;
			Sleep(PCAP_DEVICESRECHECK_TIME * TIME_OUT);
			continue;
		}
		else {
			ErrorIndex = 0;
		//Mark captures
			if (PcapRunning.empty())
			{
				std::thread CaptureThread(Capture, pThedevs, true);
				CaptureThread.detach();
			}
			else {
				pDrive = pThedevs;
				while (true)
				{
					for (CaptureIter = PcapRunning.begin();CaptureIter != PcapRunning.end();CaptureIter++)
					{
						DeviceName = pDrive->name;
						if (*CaptureIter == DeviceName)
						{
							DeviceName.clear();
							DeviceName.shrink_to_fit();
							break;
						}

						if (CaptureIter == PcapRunning.end() - 1U)
						{
							std::thread CaptureThread(Capture, pDrive, false);
							CaptureThread.detach();
							continue;
						}
					}

					if (pDrive->next == nullptr)
						break;
					else 
						pDrive = pDrive->next;
				}
			}
		}

		Sleep(PCAP_DEVICESRECHECK_TIME * TIME_OUT);
		pcap_freealldevs(pThedevs);
	}

	WSACleanup();
	TerminateService();
	return EXIT_SUCCESS;
}

inline void __fastcall FilterRulesInit(std::string &FilterRules)
{
//Initialization
	std::string BothAddr;
	FilterRules = ("(src host ");
	std::shared_ptr<char> Addr(new char[ADDR_STRING_MAXSIZE]());

//Set capture filter.
	auto NonSingle = false, Connection = false;
	if (Parameter.DNSTarget.IPv6.sin6_family != NULL && Parameter.DNSTarget.IPv4.sin_family != NULL || 
		Parameter.DNSTarget.Alternate_IPv6.sin6_family != NULL || Parameter.DNSTarget.Alternate_IPv4.sin_family != NULL)
			NonSingle = true;

//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
#ifdef _WIN64
#else //x86
	sockaddr_storage SockAddr = {0};
	DWORD BufferLength = ADDR_STRING_MAXSIZE;
#endif

	if (NonSingle)
		BothAddr = ("(");
	//Main(IPv6)
	if (Parameter.DNSTarget.IPv6.sin6_family != NULL)
	{
	#ifdef _WIN64
		inet_ntop(AF_INET6, (PSTR)&Parameter.DNSTarget.IPv6.sin6_addr, Addr.get(), ADDR_STRING_MAXSIZE);
	#else //x86
		BufferLength = ADDR_STRING_MAXSIZE;
		SockAddr.ss_family = AF_INET6;
		((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.IPv6.sin6_addr;
		WSAAddressToStringA((LPSOCKADDR)&SockAddr, sizeof(sockaddr_in6), NULL, Addr.get(), &BufferLength);
	#endif

		if (!Connection)
			Connection = true;
		else 
			BothAddr.append(" or ");
		BothAddr.append(Addr.get());
		memset(Addr.get(), 0, ADDR_STRING_MAXSIZE);
	}
	//Main(IPv4)
	if (Parameter.DNSTarget.IPv4.sin_family != NULL)
	{
	#ifdef _WIN64
		inet_ntop(AF_INET, (PSTR)&Parameter.DNSTarget.IPv4.sin_addr, Addr.get(), ADDR_STRING_MAXSIZE);
	#else //x86
		BufferLength = ADDR_STRING_MAXSIZE;
		SockAddr.ss_family = AF_INET;
		((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.IPv4.sin_addr;
		WSAAddressToStringA((LPSOCKADDR)&SockAddr, sizeof(sockaddr_in), NULL, Addr.get(), &BufferLength);
	#endif

		if (!Connection)
			Connection = true;
		else 
			BothAddr.append(" or ");
		BothAddr.append(Addr.get());
		memset(Addr.get(), 0, ADDR_STRING_MAXSIZE);
	}
	//Alternate(IPv6)
	if (Parameter.DNSTarget.Alternate_IPv6.sin6_family != NULL)
	{
	#ifdef _WIN64
		inet_ntop(AF_INET6, (PSTR)&Parameter.DNSTarget.Alternate_IPv6.sin6_addr, Addr.get(), ADDR_STRING_MAXSIZE);
	#else //x86
		BufferLength = ADDR_STRING_MAXSIZE;
		SockAddr.ss_family = AF_INET6;
		((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.Alternate_IPv6.sin6_addr;
		WSAAddressToStringA((LPSOCKADDR)&SockAddr, sizeof(sockaddr_in6), NULL, Addr.get(), &BufferLength);
	#endif

		if (!Connection)
			Connection = true;
		else 
			BothAddr.append(" or ");
		BothAddr.append(Addr.get());
		memset(Addr.get(), 0, ADDR_STRING_MAXSIZE);
	}
	//Alternate(IPv4)
	if (Parameter.DNSTarget.Alternate_IPv4.sin_family != NULL)
	{
	#ifdef _WIN64
		inet_ntop(AF_INET, (PSTR)&Parameter.DNSTarget.Alternate_IPv4.sin_addr, Addr.get(), ADDR_STRING_MAXSIZE);
	#else //x86
		BufferLength = ADDR_STRING_MAXSIZE;
		SockAddr.ss_family = AF_INET;
		((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.Alternate_IPv4.sin_addr;
		WSAAddressToStringA((LPSOCKADDR)&SockAddr, sizeof(sockaddr_in), NULL, Addr.get(), &BufferLength);
	#endif

		if (!Connection)
			Connection = true;
		else 
			BothAddr.append(" or ");
		BothAddr.append(Addr.get());
		memset(Addr.get(), 0, ADDR_STRING_MAXSIZE);
	}

	if (NonSingle)
		BothAddr.append(")");
	FilterRules.append(BothAddr);
	FilterRules.append(") or (pppoes and src host ");
	FilterRules.append(BothAddr);
	FilterRules.append(")");

	return;
}

//Capture process
size_t __fastcall Capture(const pcap_if *pDrive, const bool List)
{
//Initialization
	pcap_t *pAdHandle = nullptr;
	pcap_pkthdr *pHeader = nullptr;
	const UCHAR *PacketData = nullptr;
	std::shared_ptr<wchar_t> ErrBuffer(new wchar_t[PCAP_ERRBUF_SIZE]()), DeviceName(new wchar_t[PCAP_ERRBUF_SIZE]());
	std::shared_ptr<char> Buffer(new char[PACKET_MAXSIZE * BUFFER_RING_MAXNUM]());

//Open device
	if ((pAdHandle = pcap_open(pDrive->name, PACKET_MAXSIZE, PCAP_OPENFLAG_NOCAPTURE_LOCAL, TIME_OUT/4U, NULL, Buffer.get())) == nullptr)
	{
		MultiByteToWideChar(CP_ACP, NULL, Buffer.get(), MBSTOWCS_NULLTERMINATE, ErrBuffer.get(), PCAP_ERRBUF_SIZE);
		PrintError(WINPCAP_ERROR, ErrBuffer.get(), NULL, nullptr, NULL);

		return EXIT_FAILURE;
	}

//Check device type
	MultiByteToWideChar(CP_ACP, NULL, pDrive->name, MBSTOWCS_NULLTERMINATE, DeviceName.get(), PCAP_ERRBUF_SIZE);
	if (pcap_datalink(pAdHandle) != DLT_EN10MB) //Ethernet
	{
		wcsncpy_s(ErrBuffer.get(), PCAP_ERRBUF_SIZE, DeviceName.get(), lstrlenW(DeviceName.get()));
		wcsncpy_s(ErrBuffer.get() + lstrlenW(DeviceName.get()), PCAP_ERRBUF_SIZE - lstrlenW(DeviceName.get()), L" is not a Ethernet device", lstrlenW(L" is not a Ethernet device"));
		PrintError(WINPCAP_ERROR, ErrBuffer.get(), NULL, nullptr, NULL);

		pcap_close(pAdHandle);
		return EXIT_FAILURE;
	}

//Compile the string into a filter program.
	bpf_program FCode = {0};
	if (pcap_compile(pAdHandle, &FCode, PcapFilterRules.c_str(), 1, (bpf_u_int32)pDrive->addresses->netmask) == RETURN_ERROR)
	{
		MultiByteToWideChar(CP_ACP, NULL, pcap_geterr(pAdHandle), MBSTOWCS_NULLTERMINATE, ErrBuffer.get(), PCAP_ERRBUF_SIZE);
		PrintError(WINPCAP_ERROR, ErrBuffer.get(), NULL, nullptr, NULL);

		pcap_freecode(&FCode);
		pcap_close(pAdHandle);
		return EXIT_FAILURE;
	}
	
//Specify a filter program.
	if (pcap_setfilter(pAdHandle, &FCode) == RETURN_ERROR)
	{
		MultiByteToWideChar(CP_ACP, NULL, pcap_geterr(pAdHandle), MBSTOWCS_NULLTERMINATE, ErrBuffer.get(), PCAP_ERRBUF_SIZE);
		PrintError(WINPCAP_ERROR, ErrBuffer.get(), NULL, nullptr, NULL);

		pcap_freecode(&FCode);
		pcap_close(pAdHandle);
		return EXIT_FAILURE;
	}

//Start capture(s) with other device(s).
	std::string CaptureDevice(pDrive->name);
	PcapRunning.push_back(CaptureDevice);
	if (List && pDrive->next != nullptr)
	{
		std::thread CaptureThread(Capture, pDrive->next, true);
		CaptureThread.detach();
	}

//Start capture.
	SSIZE_T Result = 0;
	size_t Index = 0, HeaderLength = 0;

	eth_hdr *eth = nullptr;
	pppoe_hdr *pppoe = nullptr;
	while (true)
	{
		Result = pcap_next_ex(pAdHandle, &pHeader, &PacketData);
		switch (Result)
		{
			case RETURN_ERROR: //An error occurred.
			{
			//Devices are offline or other errors, wait for retrying.
				wcsncpy_s(ErrBuffer.get(), PCAP_ERRBUF_SIZE, L"An error occurred in ", lstrlenW(L"An error occurred in "));
				wcsncpy_s(ErrBuffer.get() + lstrlenW(L"An error occurred in "), PCAP_ERRBUF_SIZE - lstrlenW(L"An error occurred in "), DeviceName.get(), lstrlenW(DeviceName.get()));
				PrintError(WINPCAP_ERROR, ErrBuffer.get(), NULL, nullptr, NULL);

				for (auto CaptureIter = PcapRunning.begin();CaptureIter != PcapRunning.end();CaptureIter++)
				{
					if (*CaptureIter == CaptureDevice)
					{
						PcapRunning.erase(CaptureIter);
						PcapRunning.shrink_to_fit();
						break;
					}
				}
				
				pcap_freecode(&FCode);
				pcap_close(pAdHandle);
				return EXIT_FAILURE;
			}break;
			case -2: //EOF was reached reading from an offline capture.
			{
				wcsncpy_s(ErrBuffer.get(), PCAP_ERRBUF_SIZE, L"EOF was reached reading from an offline capture in ", lstrlenW(L"EOF was reached reading from an offline capture in "));
				wcsncpy_s(ErrBuffer.get() + lstrlenW(L"EOF was reached reading from an offline capture in "), PCAP_ERRBUF_SIZE - lstrlenW(L"EOF was reached reading from an offline capture in "), DeviceName.get(), lstrlenW(DeviceName.get()));
				PrintError(WINPCAP_ERROR, ErrBuffer.get(), NULL, nullptr, NULL);

				for (auto CaptureIter = PcapRunning.begin();CaptureIter != PcapRunning.end();CaptureIter++)
				{
					if (*CaptureIter == CaptureDevice)
					{
						PcapRunning.erase(CaptureIter);
						break;
					}
				}
				
				pcap_freecode(&FCode);
				pcap_close(pAdHandle);
				return EXIT_FAILURE;
			}break;
			case FALSE: //0, Timeout set with pcap_open_live() has elapsed. In this case pkt_header and pkt_data don't point to a valid packet.
			{
				continue;
			}
			case TRUE: //1, Packet has been read without problems.
			{
				memset(Buffer.get() + PACKET_MAXSIZE * Index, 0, PACKET_MAXSIZE);

				eth = (eth_hdr *)PacketData;
				HeaderLength = sizeof(eth_hdr);
			//PPPoE(Such as ADSL, a part of organization networks)
				if (eth->Type == htons(ETHERTYPE_PPPOES) && pHeader->caplen > HeaderLength + sizeof(pppoe_hdr))
				{
					pppoe = (pppoe_hdr *)(PacketData + HeaderLength);
					HeaderLength += sizeof(pppoe_hdr);
					if (pppoe->Protocol == htons(PPPOETYPE_IPV6) && Parameter.DNSTarget.IPv6.sin6_family != NULL && pHeader->caplen > HeaderLength + sizeof(ipv6_hdr) || //IPv6 over PPPoE
						pppoe->Protocol == htons(PPPOETYPE_IPV4) && Parameter.DNSTarget.IPv4.sin_family != NULL && pHeader->caplen > HeaderLength + sizeof(ipv4_hdr)) //IPv4 over PPPoE
					{
						memcpy(Buffer.get() + PACKET_MAXSIZE * Index, PacketData + HeaderLength, pHeader->caplen - HeaderLength);
						std::thread NetworkLayerMethod(NetworkLayer, Buffer.get() + PACKET_MAXSIZE * Index, pHeader->caplen - HeaderLength, ntohs(pppoe->Protocol));
						NetworkLayerMethod.detach();

						Index = (Index + 1U) % BUFFER_RING_MAXNUM;
					}
				}
			//LAN/WLAN/IEEE 802.1X, some Mobile Communications Standard/MCS drives which disguise as a LAN
				else if (eth->Type == htons(ETHERTYPE_IPV6) && Parameter.DNSTarget.IPv6.sin6_family != NULL && pHeader->caplen > HeaderLength + sizeof(ipv6_hdr) || //IPv6 over Ethernet
					eth->Type == htons(ETHERTYPE_IP) && Parameter.DNSTarget.IPv4.sin_family != NULL && pHeader->caplen > HeaderLength + sizeof(ipv4_hdr)) //IPv4 over Ethernet
				{
					memcpy(Buffer.get() + PACKET_MAXSIZE * Index, PacketData + HeaderLength, pHeader->caplen - HeaderLength);
					std::thread NetworkLayerMethod(NetworkLayer, Buffer.get() + PACKET_MAXSIZE * Index, pHeader->caplen - HeaderLength, ntohs(eth->Type));
					NetworkLayerMethod.detach();

					Index = (Index + 1U) % BUFFER_RING_MAXNUM;
				}
				else {
					continue;
				}
			}break;
			default: {
				continue;
			}
		}
	}

	pcap_freecode(&FCode);
	pcap_close(pAdHandle);
	return EXIT_SUCCESS;
}

//Network Layer(Internet Protocol/IP) process
size_t __fastcall NetworkLayer(const PSTR Recv, const size_t Length, const uint16_t Protocol)
{
//Initialization
	std::shared_ptr<char> Buffer(new char[Length]());
	memcpy(Buffer.get(), Recv, Length);
	size_t ServerAttribute = 0;

	if (Protocol == PPPOETYPE_IPV6 || Protocol == ETHERTYPE_IPV6) //IPv6
	{
		auto ipv6 = (ipv6_hdr *)Buffer.get();
	//Validate IPv6 header.
		if (ntohs(ipv6->PayloadLength) > Length - sizeof(ipv6_hdr))
			return EXIT_FAILURE;

	//Mark Alternate address(es).
		if (memcmp(&ipv6->Src, &Parameter.DNSTarget.Alternate_IPv6.sin6_addr, sizeof(in6_addr)) == 0)
			ServerAttribute = ALTERNATE_SERVER;
//		else //Main
//			ServerAttribute = PREFERRED_SERVER;

	//Get Hop Limits from IPv6 DNS server.
		if (ServerAttribute == PREFERRED_SERVER || ServerAttribute == ALTERNATE_SERVER)
		{
		//ICMPv6 Protocol
			if (Parameter.ICMPOptions.ICMPSpeed > 0 && ipv6->NextHeader == IPPROTO_ICMPV6 && ntohs(ipv6->PayloadLength) >= sizeof(icmpv6_hdr))
			{
			//Validate ICMPv6 checksum.
				if (ICMPv6Checksum((PUINT8)Buffer.get(), ntohs(ipv6->PayloadLength), ipv6->Dst, ipv6->Src) != 0)
					return EXIT_FAILURE;

				if (ICMPCheck(Buffer.get(), ntohs(ipv6->PayloadLength), AF_INET6))
				{
					if (ServerAttribute == ALTERNATE_SERVER)
						Parameter.HopLimitOptions.Alternate_IPv6HopLimit = ipv6->HopLimit;
					else //PREFERRED_SERVER
						Parameter.HopLimitOptions.IPv6HopLimit = ipv6->HopLimit;
				}
			}

		//TCP Protocol
			if (Parameter.TCPDataCheck && ipv6->NextHeader == IPPROTO_TCP && ntohs(ipv6->PayloadLength) >= sizeof(tcp_hdr))
			{
			//Validate TCP checksum.
				if (TCPUDPChecksum((PUINT8)Buffer.get(), ntohs(ipv6->PayloadLength), AF_INET6, IPPROTO_TCP) != 0)
					return EXIT_FAILURE;

				if (TCPCheck(Buffer.get() + sizeof(ipv6_hdr)))
				{
					if (ServerAttribute == ALTERNATE_SERVER)
						Parameter.HopLimitOptions.Alternate_IPv6HopLimit = ipv6->HopLimit;
					else //PREFERRED_SERVER
						Parameter.HopLimitOptions.IPv6HopLimit = ipv6->HopLimit;
				}

				return EXIT_SUCCESS;
			}
		}

	//UDP Protocol
		if (ipv6->NextHeader == IPPROTO_UDP && ntohs(ipv6->PayloadLength) > sizeof(udp_hdr) + sizeof(dns_hdr))
		{
		//Validate UDP checksum.
			if (TCPUDPChecksum((PUINT8)Buffer.get(), ntohs(ipv6->PayloadLength), AF_INET6, IPPROTO_UDP) != 0)
				return EXIT_FAILURE;

			auto udp = (udp_hdr *)(Buffer.get() + sizeof(ipv6_hdr));
			if (udp->Src_Port != 0 && 
				(udp->Src_Port == Parameter.DNSTarget.IPv6.sin6_port || 
				udp->Src_Port == Parameter.DNSTarget.Alternate_IPv6.sin6_port))
			{
			//Domain Test and DNS Options check and get Hop Limit form Domain Test.
				auto SignHopLimit = false;
				if (DTDNSDCheck(Buffer.get() + sizeof(ipv6_hdr) + sizeof(udp_hdr), ntohs(ipv6->PayloadLength) - sizeof(udp_hdr), SignHopLimit))
				{
					if (SignHopLimit)
						Parameter.HopLimitOptions.IPv6HopLimit = ipv6->HopLimit;
				}
				else {
					return EXIT_FAILURE;
				}

			//Hop Limit must not a ramdom value.
				if ((SSIZE_T)ipv6->HopLimit > (SSIZE_T)Parameter.HopLimitOptions.IPv6HopLimit - (SSIZE_T)Parameter.HopLimitOptions.HopLimitFluctuation && (size_t)ipv6->HopLimit < (size_t)Parameter.HopLimitOptions.IPv6HopLimit + (size_t)Parameter.HopLimitOptions.HopLimitFluctuation)
				{
					DNSMethod(Buffer.get() + sizeof(ipv6_hdr), ntohs(ipv6->PayloadLength), AF_INET6);
					return EXIT_SUCCESS;
				}
			}
		}
	}
	else { //IPv4
		auto ipv4 = (ipv4_hdr *)Buffer.get();
	//Validate IPv4 header(Part 1).
		if (ipv4->IHL != IPV4_STANDARDIHL || ntohs(ipv4->Length) < sizeof(ipv4) || ntohs(ipv4->Length) > Length)
		{
			return EXIT_FAILURE;
		}
		else {
		//Validate IPv4 header checksum.
			if (GetChecksum((PUINT16)Buffer.get(), sizeof(ipv4_hdr)) != 0)
				return EXIT_FAILURE;
		}

	//Mark Alternate address(es).
		if (ipv4->Src.S_un.S_addr == Parameter.DNSTarget.Alternate_IPv4.sin_addr.S_un.S_addr)
			ServerAttribute = ALTERNATE_SERVER;
//		else //Main
//			ServerAttribute = PREFERRED_SERVER;

	//Get TTL from IPv4 DNS server.
		if (ServerAttribute == PREFERRED_SERVER || ServerAttribute == ALTERNATE_SERVER)
		{
		//Validate IPv4 header(Part 2).
			if (Parameter.IPv4DataCheck && ipv4->TOS != 0 && ipv4->Flags != 0) //TOS and Flags should not be set.
				return EXIT_FAILURE;

		//ICMP Protocol
			if (Parameter.ICMPOptions.ICMPSpeed > 0 && ipv4->Protocol == IPPROTO_ICMP && ntohs(ipv4->Length) >= sizeof(ipv4_hdr) + sizeof(icmp_hdr))
			{
			//Validate ICMP checksum.
				if (GetChecksum((PUINT16)(Buffer.get() + sizeof(ipv4_hdr)), ntohs(ipv4->Length) - sizeof(ipv4_hdr)) != 0)
					return EXIT_FAILURE;

				if (ICMPCheck(Buffer.get(), ntohs(ipv4->Length), AF_INET))
				{
					if (ServerAttribute == ALTERNATE_SERVER)
						Parameter.HopLimitOptions.Alternate_IPv4TTL = ipv4->TTL;
					else //PREFERRED_SERVER
						Parameter.HopLimitOptions.IPv4TTL = ipv4->TTL;
				}

				return EXIT_SUCCESS;
			}

		//TCP Protocol
			if (Parameter.TCPDataCheck && ipv4->Protocol == IPPROTO_TCP && ntohs(ipv4->Length) >= sizeof(ipv4_hdr) + sizeof(tcp_hdr))
			{
			//Validate TCP checksum.
				if (TCPUDPChecksum((PUINT8)Buffer.get(), ntohs(ipv4->Length), AF_INET, IPPROTO_TCP) != 0)
					return EXIT_FAILURE;

				if (TCPCheck(Buffer.get() + sizeof(ipv4_hdr)))
				{
					if (ServerAttribute == ALTERNATE_SERVER)
						Parameter.HopLimitOptions.Alternate_IPv4TTL = ipv4->TTL;
					else //PREFERRED_SERVER
						Parameter.HopLimitOptions.IPv4TTL = ipv4->TTL;
				}

				return EXIT_SUCCESS;
			}
		}

	//UDP Protocol
		if (ipv4->Protocol == IPPROTO_UDP && ntohs(ipv4->Length) > sizeof(ipv4_hdr) + sizeof(udp_hdr) + sizeof(dns_hdr))
		{
		//Validate UDP checksum.
			if (TCPUDPChecksum((PUINT8)Buffer.get(), ntohs(ipv4->Length), AF_INET, IPPROTO_UDP) != 0)
				return EXIT_FAILURE;

			auto udp = (udp_hdr *)(Buffer.get() + sizeof(ipv4_hdr));
			if (udp->Src_Port != 0 && 
				(udp->Src_Port == Parameter.DNSTarget.IPv4.sin_port || 
				udp->Src_Port == Parameter.DNSTarget.Alternate_IPv4.sin_port))
			{
			//Domain Test and DNS Options check and get TTL form Domain Test.
				auto SignHopLimit = false;
				if (DTDNSDCheck(Buffer.get() + sizeof(ipv4_hdr) + sizeof(udp_hdr), ntohs(ipv4->Length) - sizeof(ipv4_hdr) - sizeof(udp_hdr), SignHopLimit))
				{
					if (SignHopLimit)
						Parameter.HopLimitOptions.IPv4TTL = ipv4->TTL;
				}
				else {
					return EXIT_FAILURE;
				}

			//TTL must not a ramdom value.
				if ((SSIZE_T)ipv4->TTL > (SSIZE_T)Parameter.HopLimitOptions.IPv4TTL - (SSIZE_T)Parameter.HopLimitOptions.HopLimitFluctuation && (size_t)ipv4->TTL < (size_t)Parameter.HopLimitOptions.IPv4TTL + (size_t)Parameter.HopLimitOptions.HopLimitFluctuation)
				{
					DNSMethod(Buffer.get() + sizeof(ipv4_hdr), ntohs(ipv4->Length) - sizeof(ipv4_hdr), AF_INET);
					return EXIT_SUCCESS;
				}
			}
		}
	}

	return EXIT_SUCCESS;
}

//ICMP header options check
inline bool __fastcall ICMPCheck(const PSTR Buffer, const size_t Length, const uint16_t Protocol)
{
	if (Protocol == AF_INET6) //ICMPv6
	{
		auto icmpv6 = (icmpv6_hdr *)Buffer;
		if (icmpv6->Type == ICMPV6_REPLY && icmpv6->Code == 0 && //ICMPv6 reply
			ntohs(icmpv6->ID) == Parameter.ICMPOptions.ICMPID && icmpv6->Sequence == Parameter.ICMPOptions.ICMPSequence) //Validate ICMP packet.
				return true;
	}
	else { //ICMP
		auto icmp = (icmp_hdr *)(Buffer + sizeof(ipv4_hdr));
		if (icmp->Type == 0 && icmp->Code == 0 && //ICMP reply
		//Validate ICMP packet
			ntohs(icmp->ID) == Parameter.ICMPOptions.ICMPID && icmp->Sequence == Parameter.ICMPOptions.ICMPSequence && 
			Length == sizeof(ipv4_hdr) + sizeof(icmp_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U && 
			memcmp(Parameter.PaddingDataOptions.PaddingData, (PSTR)icmp + sizeof(icmp_hdr), Parameter.PaddingDataOptions.PaddingDataLength - 1U) == 0) //Validate ICMP additional data.
				return true;
	}

	return false;
}

//TCP header options check
inline bool __fastcall TCPCheck(const PSTR Buffer)
{
	auto tcp = (tcp_hdr *)Buffer;
	if (tcp->Acknowledge == 0 && tcp->StatusFlags.Flags == TCP_RSTSTATUS && tcp->Windows == 0 || //TCP Flags are 0x004(RST) which ACK shoule be 0 and Window size should be 0.
		tcp->HeaderLength > TCP_STANDARDHL && tcp->StatusFlags.Flags == TCP_SYNACKSTATUS) //TCP option usually should not empty(MSS, SACK_PERM and WS) whose Flags are 0x012(SYN/ACK).
			return true;

	return false;
}

//Domain Test and DNS Data check/DomainTestAndDNSDataCheck
inline bool __fastcall DTDNSDCheck(const PSTR Buffer, const size_t Length, bool &SignHopLimit)
{
	auto pdns_hdr = (dns_hdr *)Buffer;

//No Such Name
//	xxxxxxxxxxxxxx11 & 0000000000000011 == 0000000000000011 -> 0x0003
	if ((ntohs(pdns_hdr->Flags) & DNS_RCODE_NO_SUCH_NAME) == DNS_RCODE_NO_SUCH_NAME)
	{
		SignHopLimit = true;
		return true;
	}

//DNS Options part
	if (pdns_hdr->Questions == 0 || //Not any Answer Record in response(s)
	//Responses are not authoritative when there are not any Authoritative Nameservers Records/Additional Records.
	//	xxxxx1xxxxxxxxxx & 0000010000000000 >> 10 == 1
		Parameter.DNSDataCheck && pdns_hdr->Authority == 0 && pdns_hdr->Additional == 0 && pdns_hdr->Answer == 0 && //No any Records
		(ntohs(pdns_hdr->Flags) & 0x0400) >> 10U == 1U)
			return false;

	if (Parameter.DNSDataCheck && 
		(ntohs(pdns_hdr->Answer) != 0x0001 || //Less than or more than 1 Answer Record
		pdns_hdr->Authority != 0 || pdns_hdr->Additional != 0)) //Authority Record(s) and/or Additional Record(s)
			SignHopLimit = true;

	std::shared_ptr<char> Result;
//Domain Test part
	size_t DomainLength[] = {0, 0};
	if (Parameter.DomainTestOptions.DomainTestCheck && strlen(Buffer + sizeof(dns_hdr)) < DOMAIN_MAXSIZE)
	{
		std::shared_ptr<char> TempBuffer(new char[DOMAIN_MAXSIZE]());
		Result.swap(TempBuffer);
		TempBuffer.reset();

		DomainLength[0] = DNSQueryToChar(Buffer + sizeof(dns_hdr), Result.get());
		if (strlen(Parameter.DomainTestOptions.DomainTest) >= DomainLength[0] && 
			memcmp(Result.get(), Parameter.DomainTestOptions.DomainTest, DomainLength[0]) == 0 && pdns_hdr->ID == Parameter.DomainTestOptions.DomainTestID)
		{
			SignHopLimit = true;
			return true;
		}
	}

//Check DNS Compression.
	if (Parameter.DNSDataCheck && DomainLength[0] > 0 && Result && 
		strlen(Buffer + sizeof(dns_hdr)) < DOMAIN_MAXSIZE && strlen(Buffer + sizeof(dns_hdr) + strlen(Buffer + sizeof(dns_hdr)) + sizeof(dns_qry) * 2U + 1U) < DOMAIN_MAXSIZE)
	{
		std::shared_ptr<char> Compression(new char[DOMAIN_MAXSIZE]());

		DomainLength[1U] = DNSQueryToChar(Buffer + sizeof(dns_hdr) + strlen(Buffer + sizeof(dns_hdr)) + sizeof(dns_qry) * 2U + 1U, Compression.get());
		if (DomainLength[0] == DomainLength[1U] && 
			memcmp(Result.get(), Compression.get(), DomainLength[1U]) == 0)
				return false;
	}

	return true;
}

//Application Layer(Domain Name System/DNS) process
inline size_t __fastcall DNSMethod(const PSTR Recv, const size_t Length, const uint16_t Protocol)
{
//Mark port and responses answer(s) check
	auto udp = (udp_hdr *)Recv;
	if (Length <= sizeof(udp_hdr) + sizeof(dns_hdr) + 1U + sizeof(dns_qry))
		return EXIT_FAILURE;

	size_t DataLength = Length - sizeof(udp_hdr);
//DNS Responses which only have 1 Answer RR and no any Authority RRs or Additional RRs need to check.
	auto pdns_hdr = (dns_hdr *)(Recv + sizeof(udp_hdr));
	if (pdns_hdr->Questions == htons(0x0001) && pdns_hdr->Answer == htons(0x0001) && 
		pdns_hdr->Authority == 0 && pdns_hdr->Additional == 0)
	{
		auto pdns_qry = (dns_qry *)(Recv + sizeof(udp_hdr) + sizeof(dns_hdr) + strlen(Recv + sizeof(udp_hdr) + sizeof(dns_hdr)) + 1U);
		if (pdns_qry->Classes == htons(DNS_CLASS_IN)) //Class IN
		{
		//Record(s) Type in responses check
			if (Parameter.DNSDataCheck)
			{
				auto AnswerName = (PUINT16)(&pdns_qry->Classes + 1U), AnswerType = AnswerName + 1U;
				if (*AnswerName == htons(DNS_QUERY_PTR) && *AnswerType != pdns_qry->Type) //Types in Queries and Answers are different.
					return EXIT_FAILURE;
			}

		//Fake responses check
			if (Parameter.Blacklist)
			{
			//IPv6
				if (pdns_qry->Type == htons(DNS_AAAA_RECORDS)) //AAAA Records
				{
					auto DNS_AAAA = (dns_aaaa_record *)(Recv + sizeof(udp_hdr) + (DataLength - sizeof(dns_aaaa_record)));
					if (DNS_AAAA->Type == htons(DNS_AAAA_RECORDS) && DNS_AAAA->Classes == htons(DNS_CLASS_IN) && 
						DNS_AAAA->TTL != 0 && DNS_AAAA->Length == sizeof(in6_addr) && 
						CheckSpecialAddress(&DNS_AAAA->Addr, AF_INET6))
							return EXIT_FAILURE;
				}
			//IPv4
				else if (pdns_qry->Type == htons(DNS_A_RECORDS)) //A Records
				{
					auto DNS_A = (dns_a_record *)(Recv + sizeof(udp_hdr) + (DataLength - sizeof(dns_a_record)));
					if (DNS_A->Type == htons(DNS_A_RECORDS) && DNS_A->Classes == htons(DNS_CLASS_IN) && 
						DNS_A->TTL != 0 && DNS_A->Length == sizeof(in_addr) && 
						CheckSpecialAddress(&DNS_A->Addr, AF_INET))
							return EXIT_FAILURE;
				}
			}
		}
	}

//UDP Truncated check(TC bit in DNS Header has been set)
//	xxxxxx1xxxxxxxxx & 0000001000000000 >> 9 == 1
	if ((ntohs(pdns_hdr->Flags) & 0x0200) >> 9U == 1U && pdns_hdr->Answer == 0)
	{
		std::shared_ptr<char> RecvBuffer(new char[LARGE_PACKET_MAXSIZE]());
		SOCKET_DATA Target = {0};
		if (Protocol == AF_INET6) //IPv6
			Target.AddrLen = sizeof(sockaddr_in6);
		else //IPv4
			Target.AddrLen = sizeof(sockaddr_in);

	//Retry with TCP
		pdns_hdr->Flags = htons(DNS_STANDARD);
		DataLength = TCPRequest(Recv + sizeof(udp_hdr), DataLength, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, Target, false, false);
		if (DataLength > sizeof(dns_hdr) + 1U + sizeof(dns_qry))
		{
			PortList.MatchToSend(RecvBuffer.get(), DataLength, udp->Dst_Port);
			return EXIT_SUCCESS;
		}
		else {
			DataLength = Length - sizeof(udp_hdr);
		}
	}

//Send
	PortList.MatchToSend(Recv + sizeof(udp_hdr), DataLength, udp->Dst_Port);
	return EXIT_SUCCESS;
}

//Match port of responses and send responses to system socket(s) process
size_t PortTable::MatchToSend(const PSTR Buffer, const size_t Length, const uint16_t RequestPort)
{
//Match port
	SOCKET_DATA SystemPort = {0};
	size_t Index = 0;

	for (Index = 0;Index < QUEUE_MAXLENGTH * QUEUE_PARTNUM;Index++)
	{
		if (SendData[Index].SockAddr.ss_family == sizeof(sockaddr_in6)) //IPv6
		{
			if (RequestPort == ((PSOCKADDR_IN6)&SendData[Index].SockAddr)->sin6_port)
			{
				AlternateSwapList.PcapAlternateTimeout[Index] = 0;
				SystemPort = RecvData[Index];
				memset(&RecvData[Index], 0, sizeof(SOCKET_DATA));
				memset(&SendData[Index], 0, sizeof(SOCKET_DATA));
				break;
			}
		}
		else { //IPv4
			if (RequestPort == ((PSOCKADDR_IN)&SendData[Index].SockAddr)->sin_port)
			{
				AlternateSwapList.PcapAlternateTimeout[Index] = 0;
				SystemPort = RecvData[Index];
				memset(&RecvData[Index], 0, sizeof(SOCKET_DATA));
				memset(&SendData[Index], 0, sizeof(SOCKET_DATA));
				break;
			}
		}
	}

//Drop resopnses which not in PortList.
	if (SystemPort.Socket == 0 || SystemPort.AddrLen == 0 || SystemPort.SockAddr.ss_family == 0)
		return EXIT_FAILURE;

//Mark DNS Cache
	if (Parameter.CacheType != 0)
		MarkDNSCache(Buffer, Length);

//Send to localhost
	if (Index >= QUEUE_MAXLENGTH * QUEUE_PARTNUM / 2U) //TCP area
	{
		std::shared_ptr<char> TCPBuffer(new char[Length + sizeof(uint16_t)]());
		auto ptcp_dns_hdr = (tcp_dns_hdr *)TCPBuffer.get();
		ptcp_dns_hdr->Length = htons((uint16_t)Length);
		memcpy(TCPBuffer.get() + sizeof(uint16_t), Buffer, Length);

		send(SystemPort.Socket, TCPBuffer.get(), (int)(Length + sizeof(uint16_t)), NULL);
		closesocket(SystemPort.Socket);
		return EXIT_SUCCESS;
	}
	else { //UDP
	//UDP Truncated check
		if (Length > Parameter.EDNS0PayloadSize)
		{
			std::shared_ptr<char> UDPBuffer(new char[sizeof(dns_hdr) + strlen(Buffer + sizeof(dns_hdr)) + 1U + sizeof(dns_qry) + sizeof(dns_edns0_label)]());
			memcpy(UDPBuffer.get(), Buffer, sizeof(dns_hdr) + strlen(Buffer + sizeof(dns_hdr)) + 1U + sizeof(dns_qry));
			auto pdns_hdr = (dns_hdr *)UDPBuffer.get();
			pdns_hdr->Flags = htons(DNS_SQRNE_TC);
			pdns_hdr->Additional = htons(0x0001);
			auto EDNS0 = (dns_edns0_label *)(UDPBuffer.get() + sizeof(dns_hdr) + strlen(Buffer + sizeof(dns_hdr)) + 1U + sizeof(dns_qry));
			EDNS0->Type = htons(DNS_EDNS0_RECORDS);
			EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);

			sendto(SystemPort.Socket, UDPBuffer.get(), (int)(sizeof(dns_hdr) + strlen(Buffer + sizeof(dns_hdr)) + 1U + sizeof(dns_qry) + sizeof(dns_edns0_label)), NULL, (PSOCKADDR)&SystemPort.SockAddr, SystemPort.AddrLen);
		}
		else {
			sendto(SystemPort.Socket, Buffer, (int)Length, NULL, (PSOCKADDR)&SystemPort.SockAddr, SystemPort.AddrLen);
		}
	}

//Cleanup socket
	for (auto LocalSocketIter:Parameter.LocalSocket)
	{
		if (LocalSocketIter == SystemPort.Socket)
			return EXIT_SUCCESS;
	}

	closesocket(SystemPort.Socket);
	return EXIT_SUCCESS;
}
