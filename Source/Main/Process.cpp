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

static const std::regex IPv4PrivateB(".(1[6-9]|2[0-9]|3[01]).172.in-addr.arpa"/* , std::regex_constants::extended */);
static const std::regex IPv6ULA(".f.[cd].([0-9]|[a-f]).([0-9]|[a-f]).ip6.arpa"/* , std::regex_constants::extended */);
static const std::regex IPv6LUC(".f.f.([89]|[ab]).([0-9]|[a-f]).ip6.arpa"/* , std::regex_constants::extended */);

extern Configuration Parameter;
extern PortTable PortList;
extern std::string LocalhostPTR[QUEUE_PARTNUM / 2U];
extern std::vector<HostsTable> *HostsListUsing;
extern std::deque<DNSCacheData> DNSCacheList;
extern std::mutex HostsListLock, DNSCacheListLock;
extern AlternateSwapTable AlternateSwapList;
extern DNSCurveConfiguration DNSCurveParameter;

//Independent request process
size_t __fastcall RequestProcess(const PSTR Send, const size_t Length, const SOCKET_DATA FunctionData, const uint16_t Protocol, const size_t Index)
{
//Initialization
	std::shared_ptr<char> SendBuffer(new char[Length]()), RecvBuffer;
	if (Parameter.RquestMode == REQUEST_TCPMODE || DNSCurveParameter.DNSCurveMode == DNSCURVE_REQUEST_TCPMODE || Protocol == IPPROTO_TCP) //TCP
	{
		std::shared_ptr<char> TCPRecvBuffer(new char[sizeof(uint16_t) + LARGE_PACKET_MAXSIZE]());
		RecvBuffer.swap(TCPRecvBuffer);
	}
	else { //UDP
		std::shared_ptr<char> UDPRecvBuffer(new char[PACKET_MAXSIZE]());
		RecvBuffer.swap(UDPRecvBuffer);
	}
	memcpy(SendBuffer.get(), Send, Length);
	auto pdns_hdr = (dns_hdr *)SendBuffer.get();
	auto Local = false;

	size_t DataLength = 0;
//Check hosts.
	if (ntohs(pdns_hdr->Questions) > 0)
	{
	//TCP
		if (Protocol == IPPROTO_TCP)
		{
			DataLength = CheckHosts(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, Local);
			if (DataLength > sizeof(dns_hdr) + 1U + sizeof(dns_qry))
			{
				SendBuffer.reset();
				memmove(RecvBuffer.get() + sizeof(uint16_t), RecvBuffer.get(), DataLength);
				auto ptcp_dns_hdr = (tcp_dns_hdr *)RecvBuffer.get();
				ptcp_dns_hdr->Length = htons((uint16_t)DataLength);
				DataLength += sizeof(uint16_t);

				send(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL);
				closesocket(FunctionData.Socket);
				return EXIT_SUCCESS;
			}
		}
	//UDP
		else {
			DataLength = CheckHosts(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, Local);
			if (DataLength > sizeof(dns_hdr) + 1U + sizeof(dns_qry))
			{
				SendBuffer.reset();
				sendto(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL, (PSOCKADDR)&FunctionData.SockAddr, FunctionData.AddrLen);
				return EXIT_SUCCESS;
			}
		}
	}
	else { //Not any questions
		SendBuffer.reset();
		pdns_hdr = (dns_hdr *)RecvBuffer.get();
		pdns_hdr->Flags = htons(DNS_SQRNE_FE);

	//TCP
		if (Protocol == IPPROTO_TCP)
		{
			memmove(RecvBuffer.get() + sizeof(uint16_t), RecvBuffer.get(), DataLength);
			auto ptcp_dns_hdr = (tcp_dns_hdr *)RecvBuffer.get();
			ptcp_dns_hdr->Length = htons((uint16_t)DataLength);

			send(FunctionData.Socket, RecvBuffer.get(), (int)Length, NULL);
			closesocket(FunctionData.Socket);
		}
	//UDP
		else {
			sendto(FunctionData.Socket, RecvBuffer.get(), (int)Length, NULL, (PSOCKADDR)&FunctionData.SockAddr, FunctionData.AddrLen);
		}

		return EXIT_FAILURE;
	}

//Local server or hosts only requesting
	if (Local || Parameter.HostsOnly)
	{
		if (Parameter.RquestMode == REQUEST_TCPMODE)
		{
			if (Local)
			{
				if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
				{
					if (AlternateSwapList.Swap[4U])
					{
						DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, true);
					}
					else {
						DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[4U]++;
					}
				}
				else { //IPv4
					if (AlternateSwapList.Swap[5U])
					{
						DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, true);
					}
					else {
						DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[5U]++;
					}
				}
			}
			else {
				if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
				{
					if (AlternateSwapList.Swap[0])
					{
						DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, true);
					}
					else {
						DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[0]++;
					}
				}
				else { //IPv4
					if (AlternateSwapList.Swap[1U])
					{
						DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, true);
					}
					else {
						DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[1U]++;
					}
				}
				if (DataLength == WSAETIMEDOUT)
				{
					if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
						AlternateSwapList.TimeoutTimes[0]++;
					else //IPv4
						AlternateSwapList.TimeoutTimes[1U]++;
				}
			}
			if (DataLength > sizeof(dns_hdr) + 1U + sizeof(dns_qry) && DataLength < LARGE_PACKET_MAXSIZE)
			{
				SendBuffer.reset();

			//TCP
				if (Protocol == IPPROTO_TCP)
				{
					memmove(RecvBuffer.get() + sizeof(uint16_t), RecvBuffer.get(), DataLength);
					auto ptcp_dns_hdr = (tcp_dns_hdr *)RecvBuffer.get();
					ptcp_dns_hdr->Length = htons((uint16_t)DataLength);

					send(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL);
					closesocket(FunctionData.Socket);
				}
			//UDP
				else {
					sendto(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL, (PSOCKADDR)&FunctionData.SockAddr, FunctionData.AddrLen);
				}

				return EXIT_SUCCESS;
			}
		}

	//REQUEST_UDPMODE
		if (Protocol == IPPROTO_TCP)
		{
			if (Local)
			{
				if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
				{
					if (AlternateSwapList.Swap[6U])
					{
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, true);
					}
					else {
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[6U]++;
					}
				}
				else { //IPv4
					if (AlternateSwapList.Swap[7U])
					{
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, true);
					}
					else {
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[7U]++;
					}
				}
			}
			else {
				if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
				{
					if (AlternateSwapList.Swap[2U])
					{
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, true);
					}
					else {
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[2U]++;
					}
				}
				else { //IPv4
					if (AlternateSwapList.Swap[3U])
					{
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, true);
					}
					else {
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[3U]++;
					}
				}
			}
		}
		else {
			if (Local)
			{
				if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
				{
					if (AlternateSwapList.Swap[6U])
					{
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, true, true);
					}
					else {
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, true, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[6U]++;
					}
				}
				else { //IPv4
					if (AlternateSwapList.Swap[7U])
					{
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, true, true);
					}
					else {
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, true, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[7U]++;
					}
				}
			}
			else {
				if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
				{
					if (AlternateSwapList.Swap[2U])
					{
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, false, true);
					}
					else {
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, false, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[2U]++;
					}
				}
				else { //IPv4
					if (AlternateSwapList.Swap[3U])
					{
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, false, true);
					}
					else {
						DataLength = UDPCompleteRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, false, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[3U]++;
					}
				}
			}
		}
		if (DataLength > sizeof(dns_hdr) + 1U + sizeof(dns_qry) && (DataLength < PACKET_MAXSIZE || Protocol == IPPROTO_TCP && DataLength < LARGE_PACKET_MAXSIZE))
		{
			SendBuffer.reset();

		//TCP
			if (Protocol == IPPROTO_TCP)
			{
				memmove(RecvBuffer.get() + sizeof(uint16_t), RecvBuffer.get(), DataLength);
				auto ptcp_dns_hdr = (tcp_dns_hdr *)RecvBuffer.get();
				ptcp_dns_hdr->Length = htons((uint16_t)DataLength);

				send(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL);
				closesocket(FunctionData.Socket);
			}
		//UDP
			else {
				sendto(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL, (PSOCKADDR)&FunctionData.SockAddr, FunctionData.AddrLen);
			}

			return EXIT_SUCCESS;
		}
		else {
			return EXIT_FAILURE;
		}
	}

//DNSCurve requesting.
	if (Parameter.DNSCurve)
	{
	//Encryption mode
		if (DNSCurveParameter.Encryption)
		{
		//TCP requesting
			if (DNSCurveParameter.DNSCurveMode == DNSCURVE_REQUEST_TCPMODE)
			{
				if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
				{
					if (AlternateSwapList.Swap[8U])
					{
						DataLength = TCPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, true);
					}
					else {
						DataLength = TCPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, true);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[8U]++;
					}
				}
				else { //IPv4
					if (AlternateSwapList.Swap[9U])
					{
						DataLength = TCPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, true);
					}
					else {
						DataLength = TCPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, true);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[9U]++;
					}
				}
				if (DataLength > sizeof(dns_hdr) + 1U + sizeof(dns_qry) && DataLength < LARGE_PACKET_MAXSIZE)
				{
					SendBuffer.reset();

				//TCP
					if (Protocol == IPPROTO_TCP)
					{
						memmove(RecvBuffer.get() + sizeof(uint16_t), RecvBuffer.get(), DataLength);
						auto ptcp_dns_hdr = (tcp_dns_hdr *)RecvBuffer.get();
						ptcp_dns_hdr->Length = htons((uint16_t)DataLength);

						send(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL);
						closesocket(FunctionData.Socket);
					}
				//UDP
					else {
						sendto(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL, (PSOCKADDR)&FunctionData.SockAddr, FunctionData.AddrLen);
					}

					return EXIT_SUCCESS;
				}
			}

		//UDP requesting
			if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
			{
				if (AlternateSwapList.Swap[10U])
				{
					DataLength = UDPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, true, true);
				}
				else {
					DataLength = UDPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, false, true);
					if (DataLength == WSAETIMEDOUT)
						AlternateSwapList.TimeoutTimes[10U]++;
				}
			}
			else { //IPv4
				if (AlternateSwapList.Swap[11U])
				{
					DataLength = UDPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, true, true);
				}
				else {
					DataLength = UDPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, false, true);
					if (DataLength == WSAETIMEDOUT)
						AlternateSwapList.TimeoutTimes[11U]++;
				}
			}
			if (DataLength > sizeof(dns_hdr) + 1U + sizeof(dns_qry) && DataLength < PACKET_MAXSIZE)
			{
				SendBuffer.reset();

			//TCP
				if (Protocol == IPPROTO_TCP)
				{
					memmove(RecvBuffer.get() + sizeof(uint16_t), RecvBuffer.get(), DataLength);
					auto ptcp_dns_hdr = (tcp_dns_hdr *)RecvBuffer.get();
					ptcp_dns_hdr->Length = htons((uint16_t)DataLength);

					send(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL);
					closesocket(FunctionData.Socket);
				}
			//UDP
				else {
					sendto(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL, (PSOCKADDR)&FunctionData.SockAddr, FunctionData.AddrLen);
				}

				return EXIT_SUCCESS;
			}
		}
	//Normal mode
		else {
		//TCP requesting
			if (DNSCurveParameter.DNSCurveMode == DNSCURVE_REQUEST_TCPMODE)
			{
				if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
				{
					if (AlternateSwapList.Swap[8U])
					{
						DataLength = TCPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, false);
					}
					else {
						DataLength = TCPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[8U]++;
					}
				}
				else { //IPv4
					if (AlternateSwapList.Swap[9U])
					{
						DataLength = TCPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, true, false);
					}
					else {
						DataLength = TCPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, false);
						if (DataLength == WSAETIMEDOUT)
							AlternateSwapList.TimeoutTimes[9U]++;
					}
				}
				if (DataLength > sizeof(dns_hdr) + 1U + sizeof(dns_qry) && DataLength < LARGE_PACKET_MAXSIZE)
				{
					SendBuffer.reset();

				//TCP
					if (Protocol == IPPROTO_TCP)
					{
						memmove(RecvBuffer.get() + sizeof(uint16_t), RecvBuffer.get(), DataLength);
						auto ptcp_dns_hdr = (tcp_dns_hdr *)RecvBuffer.get();
						ptcp_dns_hdr->Length = htons((uint16_t)DataLength);

						send(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL);
						closesocket(FunctionData.Socket);
					}
				//UDP
					else {
						sendto(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL, (PSOCKADDR)&FunctionData.SockAddr, FunctionData.AddrLen);
					}

					return EXIT_SUCCESS;
				}
			}

		//UDP requesting
			if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
			{
				if (AlternateSwapList.Swap[10U])
				{
					DataLength = UDPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, true, false);
				}
				else {
					DataLength = UDPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, false, false);
					if (DataLength == WSAETIMEDOUT)
						AlternateSwapList.TimeoutTimes[10U]++;
				}
			}
			else { //IPv4
				if (AlternateSwapList.Swap[11U])
				{
					DataLength = UDPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, true, false);
				}
				else {
					DataLength = UDPDNSCurveRequest(SendBuffer.get(), Length, RecvBuffer.get(), PACKET_MAXSIZE, FunctionData, false, false);
					if (DataLength == WSAETIMEDOUT)
						AlternateSwapList.TimeoutTimes[11U]++;
				}
			}
			if (DataLength > sizeof(dns_hdr) + 1U + sizeof(dns_qry) && DataLength < PACKET_MAXSIZE)
			{
				SendBuffer.reset();

			//TCP
				if (Protocol == IPPROTO_TCP)
				{
					memmove(RecvBuffer.get() + sizeof(uint16_t), RecvBuffer.get(), DataLength);
					auto ptcp_dns_hdr = (tcp_dns_hdr *)RecvBuffer.get();
					ptcp_dns_hdr->Length = htons((uint16_t)DataLength);

					send(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL);
					closesocket(FunctionData.Socket);
				}
			//UDP
				else {
					sendto(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL, (PSOCKADDR)&FunctionData.SockAddr, FunctionData.AddrLen);
				}

				return EXIT_SUCCESS;
			}
		}

	//Encryption Only mode
		if (DNSCurveParameter.EncryptionOnly)
			return EXIT_SUCCESS;
	}

//TCP requesting
	if (Parameter.RquestMode == REQUEST_TCPMODE)
	{
		if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
		{
			if (AlternateSwapList.Swap[0])
			{
				DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, true);
			}
			else {
				DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, false);
				if (DataLength == WSAETIMEDOUT)
					AlternateSwapList.TimeoutTimes[0]++;
			}
		}
		else { //IPv4
			if (AlternateSwapList.Swap[1U])
			{
				DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, true);
			}
			else {
				DataLength = TCPRequest(SendBuffer.get(), Length, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, FunctionData, false, false);
				if (DataLength == WSAETIMEDOUT)
					AlternateSwapList.TimeoutTimes[1U]++;
			}
		}
		if (DataLength > sizeof(uint16_t) + sizeof(dns_hdr) + 1U + sizeof(dns_qry) && DataLength < LARGE_PACKET_MAXSIZE)
		{
			if (Protocol == IPPROTO_TCP) //TCP
			{
				SendBuffer.reset();
				memmove(RecvBuffer.get() + sizeof(uint16_t), RecvBuffer.get(), DataLength);
				auto ptcp_dns_hdr = (tcp_dns_hdr *)RecvBuffer.get();
				ptcp_dns_hdr->Length = htons((uint16_t)DataLength);

				send(FunctionData.Socket, RecvBuffer.get(), (int)(DataLength + sizeof(uint16_t)), NULL);
				closesocket(FunctionData.Socket);
			}
			else { //UDP
			//EDNS0 Label
				if (pdns_hdr->Additional != 0)
				{
					pdns_hdr = (dns_hdr *)RecvBuffer.get();
					pdns_hdr->Additional = htons(0x0001);
					auto EDNS0 = (dns_edns0_label *)(RecvBuffer.get() + DataLength);
					EDNS0->Type = htons(DNS_EDNS0_RECORDS);
					EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);

				//DNSSEC
					if (Parameter.DNSSECRequest)
						EDNS0->Z_Bits.DO = ~EDNS0->Z_Bits.DO; //Accepts DNSSEC security RRs
					DataLength += sizeof(dns_edns0_label);
				}

				sendto(FunctionData.Socket, RecvBuffer.get(), (int)DataLength, NULL, (PSOCKADDR)&FunctionData.SockAddr, FunctionData.AddrLen);
			}

			return EXIT_SUCCESS;
		}
		else {
			RecvBuffer.reset();
		}
	}
	else {
		RecvBuffer.reset();
	}

//UDP requesting
	if (Length <= Parameter.EDNS0PayloadSize)
	{
		PortList.RecvData[Index] = FunctionData;
		UDPRequest(SendBuffer.get(), Length, FunctionData, Index, Local, false);
	}
	else { //UDP Truncated retry TCP protocol failed.
		pdns_hdr->Flags = htons(DNS_SQRNE_SF);
		sendto(FunctionData.Socket, SendBuffer.get(), (int)DataLength, NULL, (PSOCKADDR)&FunctionData.SockAddr, FunctionData.AddrLen);
	}

	return EXIT_SUCCESS;
}

//Check hosts from list
inline size_t __fastcall CheckHosts(const PSTR Request, const size_t Length, PSTR Result, const size_t ResultSize, bool &Local)
{
//Initilization
	auto pdns_hdr = (dns_hdr *)Request;
	std::string Domain;
	if (pdns_hdr->Questions == htons(0x0001) && strlen(Request + sizeof(dns_hdr)) < DOMAIN_MAXSIZE)
	{
		if (DNSQueryToChar(Request + sizeof(dns_hdr), Result) > 2U)
		{
			Domain = Result;
			memset(Result, 0, ResultSize);
		}
		else {
			memset(Result, 0, ResultSize);
			return EXIT_FAILURE;
		}
	}
	else {
		return FALSE;
	}

//Response
	memcpy(Result, Request, Length);
	auto pdns_qry = (dns_qry *)(Result + sizeof(dns_hdr) + strlen(Result + sizeof(dns_hdr)) + 1U);
	if (pdns_qry->Classes != htons(DNS_CLASS_IN))
	{
		memset(Result, 0, ResultSize);
		return FALSE;
	}
	pdns_hdr = (dns_hdr *)Result;

//PTR Records
	if (pdns_qry->Type == htons(DNS_PTR_RECORDS))
	{
		//IPv4 check
		if (Domain.find(LocalhostPTR[0]) != std::string::npos || //IPv4 Localhost
			Domain.find(".10.in-addr.arpa") != std::string::npos || //Private class A address(10.0.0.0/8, Section 3 in RFC 1918)
			Domain.find(".127.in-addr.arpa") != std::string::npos || //Loopback address(127.0.0.0/8, Section 3.2.1.3 in RFC 1122)
			Domain.find(".254.169.in-addr.arpa") != std::string::npos || //Link-local address(169.254.0.0/16, RFC 3927)
			std::regex_match(Domain, IPv4PrivateB) || //Private class B address(172.16.0.0/16, Section 3 in RFC 1918)
			Domain.find(".168.192.in-addr.arpa") != std::string::npos || //Private class C address(192.168.0.0/24, Section 3 in RFC 1918)
		//IPv6 check
			Domain.find("1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.ip6.arpa") != std::string::npos || //Loopback address(::1, Section 2.5.3 in RFC 4291)
			Domain.find(LocalhostPTR[1U]) != std::string::npos || //IPv6 Localhost
			std::regex_match(Domain, IPv6ULA) || //Unique Local Unicast address/ULA(FC00::/7, Section 2.5.7 in RFC 4193)
			std::regex_match(Domain, IPv6LUC)) //Link-Local Unicast Contrast address(FE80::/10, Section 2.5.6 in RFC 4291)
		{
			pdns_hdr->Flags = htons(DNS_SQRNE);
			pdns_hdr->Answer = htons(0x0001);
			dns_ptr_record *pdns_rsp = nullptr;

		//EDNS0 Lebal
			if (pdns_hdr->Additional != 0)
			{
				memset(Result + Length - sizeof(dns_edns0_label), 0, sizeof(dns_edns0_label));

			//Response
				pdns_rsp = (dns_ptr_record *)(Result + Length - sizeof(dns_edns0_label));
				pdns_rsp->PTR = htons(DNS_QUERY_PTR);
				pdns_rsp->Classes = htons(DNS_CLASS_IN); //Class IN
				pdns_rsp->TTL = htonl(Parameter.HostsDefaultTTL);
				pdns_rsp->Type = htons(DNS_PTR_RECORDS);
				pdns_rsp->Length = htons((uint16_t)Parameter.LocalhostServerOptions.LocalhostServerLength);
				memcpy(Result + Length - sizeof(dns_edns0_label) + sizeof(dns_ptr_record), Parameter.LocalhostServerOptions.LocalhostServer, Parameter.LocalhostServerOptions.LocalhostServerLength);

			//EDNS0
				auto EDNS0 = (dns_edns0_label *)(Result + Length - sizeof(dns_edns0_label) + sizeof(dns_ptr_record) + Parameter.LocalhostServerOptions.LocalhostServerLength);
				EDNS0->Type = htons(DNS_EDNS0_RECORDS);
				EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);
				return Length + sizeof(dns_ptr_record) + Parameter.LocalhostServerOptions.LocalhostServerLength;
			}

		//Response
			pdns_rsp = (dns_ptr_record *)(Result + Length);
			memcpy(Result + Length + sizeof(dns_ptr_record), Parameter.LocalhostServerOptions.LocalhostServer, Parameter.LocalhostServerOptions.LocalhostServerLength);
			pdns_rsp->PTR = htons(DNS_QUERY_PTR);
			pdns_rsp->Classes = htons(DNS_CLASS_IN); //Class IN
			pdns_rsp->TTL = htonl(Parameter.HostsDefaultTTL);
			pdns_rsp->Type = htons(DNS_PTR_RECORDS);
			pdns_rsp->Length = htons((uint16_t)Parameter.LocalhostServerOptions.LocalhostServerLength);
			return Length + sizeof(dns_ptr_record) + Parameter.LocalhostServerOptions.LocalhostServerLength;
		}
	}

//Main check
	std::unique_lock<std::mutex> HostsListMutex(HostsListLock);
	for (auto HostsTableIter:*HostsListUsing)
	{
		if (std::regex_match(Domain, HostsTableIter.Pattern))
		{
		//Check white list.
			if (HostsTableIter.Type == HOSTS_WHITE)
			{
				break;
			}
		//Check local request.
			else if (HostsTableIter.Type == HOSTS_LOCAL)
			{
				Local = true;
				break;
			}
		//Check banned list.
			else if (HostsTableIter.Type == HOSTS_BANNED)
			{
				pdns_hdr->Flags = htons(DNS_NO_SUCH_NAME);
				return Length;
			}
		//Check Hosts.
			else {
			//IPv6
				if (pdns_qry->Type == htons(DNS_AAAA_RECORDS) && HostsTableIter.Protocol == AF_INET6)
				{
				//EDNS0 Lebal
					if (ntohs(pdns_hdr->Additional) == 0x0001)
					{
						memset(Result + Length - sizeof(dns_edns0_label), 0, sizeof(dns_edns0_label));
						pdns_hdr->Flags = htons(DNS_SQRNE);
						pdns_hdr->Answer = htons((uint16_t)(HostsTableIter.Length/sizeof(dns_aaaa_record)));
						memcpy(Result + Length - sizeof(dns_edns0_label), HostsTableIter.Response.get(), HostsTableIter.Length);

						if (!Parameter.EDNS0Label)
						{
							auto EDNS0 = (dns_edns0_label *)(Result + Length - sizeof(dns_edns0_label) + HostsTableIter.Length);
							EDNS0->Type = htons(DNS_EDNS0_RECORDS);
							EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);

						//DNSSEC
							if (Parameter.DNSSECRequest)
								EDNS0->Z_Bits.DO = ~EDNS0->Z_Bits.DO; //Accepts DNSSEC security RRs

							return Length + HostsTableIter.Length;
						}
						else {
							return Length - sizeof(dns_edns0_label) + HostsTableIter.Length;
						}
					}
					else {
						pdns_hdr->Flags = htons(DNS_SQRNE);
						pdns_hdr->Answer = htons((uint16_t)(HostsTableIter.Length / sizeof(dns_aaaa_record)));
						memcpy(Result + Length, HostsTableIter.Response.get(), HostsTableIter.Length);
						return Length + HostsTableIter.Length;
					}
				}
			//IPv4
				else if (pdns_qry->Type == htons(DNS_A_RECORDS) && HostsTableIter.Protocol == AF_INET)
				{
				//EDNS0 Lebal
					if (ntohs(pdns_hdr->Additional) == 0x0001)
					{
						memset(Result + Length - sizeof(dns_edns0_label), 0, sizeof(dns_edns0_label));
						pdns_hdr->Flags = htons(DNS_SQRNE);
						pdns_hdr->Answer = htons((uint16_t)(HostsTableIter.Length / sizeof(dns_a_record)));
						memcpy(Result + Length - sizeof(dns_edns0_label), HostsTableIter.Response.get(), HostsTableIter.Length);

						if (!Parameter.EDNS0Label)
						{
							auto EDNS0 = (dns_edns0_label *)(Result + Length - sizeof(dns_edns0_label) + HostsTableIter.Length);
							EDNS0->Type = htons(DNS_EDNS0_RECORDS);
							EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);

						//DNSSEC
							if (Parameter.DNSSECRequest)
								EDNS0->Z_Bits.DO = ~EDNS0->Z_Bits.DO; //Accepts DNSSEC security RRs

							return Length + HostsTableIter.Length;
						}
						else {
							return Length - sizeof(dns_edns0_label) + HostsTableIter.Length;
						}
					}
					else {
						pdns_hdr->Flags = htons(DNS_SQRNE);
						pdns_hdr->Answer = htons((uint16_t)(HostsTableIter.Length / sizeof(dns_a_record)));
						memcpy(Result + Length, HostsTableIter.Response.get(), HostsTableIter.Length);
						return Length + HostsTableIter.Length;
					}
				}
			}
		}
	}
	HostsListMutex.unlock();

//DNS Cache check
	std::unique_lock<std::mutex> DNSCacheListMutex(DNSCacheListLock);
	for (auto DNSCacheIter:DNSCacheList)
	{
		if (Domain == DNSCacheIter.Domain && 
			(pdns_qry->Type == htons(DNS_AAAA_RECORDS) && DNSCacheIter.Protocol == AF_INET6 || //IPv6
			pdns_qry->Type == htons(DNS_A_RECORDS) && DNSCacheIter.Protocol == AF_INET)) //IPv4
		{
			memset(Result + sizeof(uint16_t), 0, ResultSize - sizeof(uint16_t));
			memcpy(Result + sizeof(uint16_t), DNSCacheIter.Response.get(), DNSCacheIter.Length);
			return DNSCacheIter.Length + sizeof(uint16_t);
		}
	}
	DNSCacheListMutex.unlock();
	
	memset(Result, 0, ResultSize);
	return EXIT_SUCCESS;
}

//TCP protocol receive process
size_t __fastcall TCPReceiveProcess(const SOCKET_DATA FunctionData, const size_t Index)
{
	std::shared_ptr<char> Buffer(new char[LARGE_PACKET_MAXSIZE]());

//Receive
	SSIZE_T RecvLen = 0;
	if (Parameter.EDNS0Label) //EDNS0 Label
		RecvLen = recv(FunctionData.Socket, Buffer.get(), LARGE_PACKET_MAXSIZE - sizeof(dns_edns0_label), NULL);
	else 
		RecvLen = recv(FunctionData.Socket, Buffer.get(), LARGE_PACKET_MAXSIZE, NULL);
	if (RecvLen == (SSIZE_T)sizeof(uint16_t)) //TCP segment of a reassembled PDU
	{
		uint16_t PDU_Len = ntohs(((uint16_t *)Buffer.get())[0]);
		RecvLen = recv(FunctionData.Socket, Buffer.get(), LARGE_PACKET_MAXSIZE, NULL); //Receive without PDU.
		if ((SSIZE_T)PDU_Len >= RecvLen && RecvLen > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry)))
		{
			if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
				RequestProcess(Buffer.get(), PDU_Len, FunctionData, IPPROTO_TCP, Index + QUEUE_MAXLENGTH * (QUEUE_PARTNUM - 2U));
			else //IPv4
				RequestProcess(Buffer.get(), PDU_Len, FunctionData, IPPROTO_TCP, Index + QUEUE_MAXLENGTH * (QUEUE_PARTNUM - 1U));
		}
	}
	else if (RecvLen > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry)) && (SSIZE_T)htons(((uint16_t *)Buffer.get())[0]) <= RecvLen)
	{
		RecvLen = htons(((uint16_t *)Buffer.get())[0]);

	//EDNS0 Label
		if (Parameter.EDNS0Label)
		{
			auto pdns_hdr = (dns_hdr *)(Buffer.get() + sizeof(uint16_t));
			dns_edns0_label *EDNS0 = nullptr;
			if (pdns_hdr->Additional == 0) //No additional
			{
				pdns_hdr->Additional = htons(0x0001);
				EDNS0 = (dns_edns0_label *)(Buffer.get() + sizeof(uint16_t) + RecvLen);
				EDNS0->Type = htons(DNS_EDNS0_RECORDS);
				EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);
				RecvLen += sizeof(dns_edns0_label);
			}
			else { //Already EDNS0 Lebal
				EDNS0 = (dns_edns0_label *)(Buffer.get() + sizeof(uint16_t) + RecvLen - sizeof(dns_edns0_label));
				EDNS0->Type = htons(DNS_EDNS0_RECORDS);
				EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);
			}

		//DNSSEC
			if (Parameter.DNSSECRequest)
			{
				pdns_hdr->FlagsBits.AD = ~pdns_hdr->FlagsBits.AD; //Local DNSSEC Server validate
				pdns_hdr->FlagsBits.CD = ~pdns_hdr->FlagsBits.CD; //Client validate
				EDNS0->Z_Bits.DO = ~EDNS0->Z_Bits.DO; //Accepts DNSSEC security RRs
			}
		}

	//Request process
		if (FunctionData.AddrLen == sizeof(sockaddr_in6)) //IPv6
			RequestProcess(Buffer.get() + sizeof(uint16_t), RecvLen, FunctionData, IPPROTO_TCP, Index + QUEUE_MAXLENGTH * (QUEUE_PARTNUM - 2U));
		else //IPv4
			RequestProcess(Buffer.get() + sizeof(uint16_t), RecvLen, FunctionData, IPPROTO_TCP, Index + QUEUE_MAXLENGTH * (QUEUE_PARTNUM - 1U));
	}

//Block Port Unreachable messages of system.
	Sleep(TIME_OUT * RELIABLE_SOCKET_TIMEOUT);
	closesocket(FunctionData.Socket);
	return EXIT_SUCCESS;
}

//Mark responses to DNS Cache
size_t __fastcall MarkDNSCache(const PSTR Buffer, const size_t Length)
{
//Initialization
	DNSCacheData Temp;
	memset((PSTR)&Temp + sizeof(Temp.Domain) + sizeof(Temp.Response), 0, sizeof(DNSCacheData) - sizeof(Temp.Domain) - sizeof(Temp.Response));
	
//Check conditions
	auto pdns_hdr = (dns_hdr *)Buffer;
	if (pdns_hdr->Answer == 0 && pdns_hdr->Authority == 0 && pdns_hdr->Additional == 0 || //No any result
		ntohs(pdns_hdr->Questions) == 0) //No any questions
			return EXIT_FAILURE;
	auto pdns_qry = (dns_qry *)(Buffer + sizeof(dns_hdr) + strlen(Buffer + sizeof(dns_hdr)) + 1U);
	if (pdns_qry->Type == htons(DNS_AAAA_RECORDS)) //IPv6
		Temp.Protocol = AF_INET6;
	else if (pdns_qry->Type == htons(DNS_A_RECORDS)) //IPv4
		Temp.Protocol = AF_INET;
	else 
		return EXIT_FAILURE;

//Mark DNS A records and AAAA records only.
	if (Length <= DOMAIN_MAXSIZE)
	{
		std::shared_ptr<char> TempBuffer(new char[DOMAIN_MAXSIZE]());
		Temp.Response.swap(TempBuffer);
	}
	else {
		std::shared_ptr<char> TempBuffer(new char[Length]());
		Temp.Response.swap(TempBuffer);
	}

	if (DNSQueryToChar(Buffer + sizeof(dns_hdr), Temp.Response.get()) > 2U)
	{
	//Mark
		Temp.Domain = Temp.Response.get();
		memset(Temp.Response.get(), 0, DOMAIN_MAXSIZE);
		memcpy(Temp.Response.get(), Buffer + sizeof(uint16_t), Length - sizeof(uint16_t));
		Temp.Length = Length - sizeof(uint16_t);

	//Minimum supported system of GetTickCount64() is Windows Vista.
	#ifdef _WIN64
		Temp.Time = GetTickCount64();
	#else //x86
		Temp.Time = GetTickCount();
	#endif

	//Check repeating items, delete dueue rear and add new item to deque front.
		std::unique_lock<std::mutex> DNSCacheListMutex(DNSCacheListLock);
		for (auto DNSCacheDataIter = DNSCacheList.begin();DNSCacheDataIter != DNSCacheList.end();DNSCacheDataIter++)
		{
			if (Temp.Protocol == DNSCacheDataIter->Protocol && Temp.Domain == DNSCacheDataIter->Domain)
			{
				DNSCacheList.erase(DNSCacheDataIter);
				break;
			}
		}

		if (Parameter.CacheType == CACHE_QUEUE)
		{
			while (DNSCacheList.size() > Parameter.CacheParameter)
				DNSCacheList.pop_front();
		}

		DNSCacheList.push_back(Temp);
		DNSCacheList.shrink_to_fit();
	}
	else {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
