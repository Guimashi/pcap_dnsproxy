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

extern Configuration Parameter;
extern timeval ReliableSocketTimeout, UnreliableSocketTimeout;
extern PortTable PortList;
extern AlternateSwapTable AlternateSwapList;

//Get TTL(IPv4)/Hop Limits(IPv6) with normal DNS request
size_t __fastcall DomainTestRequest(const uint16_t Protocol)
{
//Initialization
	std::shared_ptr<char> Buffer(new char[PACKET_MAXSIZE]()), DNSQuery(new char[PACKET_MAXSIZE]());
	SOCKET_DATA SetProtocol = {0};

//Set request protocol.
	if (Protocol == AF_INET6) //IPv6
		SetProtocol.AddrLen = sizeof(sockaddr_in6);
	else //IPv4
		SetProtocol.AddrLen = sizeof(sockaddr_in);

//Make a DNS request with Doamin Test packet.
	auto pdns_hdr = (dns_hdr *)Buffer.get();
	pdns_hdr->ID = Parameter.DomainTestOptions.DomainTestID;
	pdns_hdr->Flags = htons(DNS_STANDARD);
	pdns_hdr->Questions = htons(0x0001);
	size_t DataLength = 0;

//From Parameter
	if (Parameter.DomainTestOptions.DomainTestCheck)
	{
		DataLength = CharToDNSQuery(Parameter.DomainTestOptions.DomainTest, DNSQuery.get());
		if (DataLength > 0 && DataLength < PACKET_MAXSIZE - sizeof(dns_hdr))
		{
			memcpy(Buffer.get() + sizeof(dns_hdr), DNSQuery.get(), DataLength);
			auto TestQry = (dns_qry *)(Buffer.get() + sizeof(dns_hdr) + DataLength);
			TestQry->Classes = htons(DNS_CLASS_IN);
			if (Protocol == AF_INET6)
				TestQry->Type = htons(DNS_AAAA_RECORDS);
			else 
				TestQry->Type = htons(DNS_A_RECORDS);
			DNSQuery.reset();
		}
		else {
			return EXIT_FAILURE;
		}
	}

//Send request.
	size_t Times = 0;
	auto Alternate = false;
	dns_qry *TestQry = nullptr;
	while (true)
	{
		if (Times == ONCESEND_TIME)
		{
			Times = 0;
			if (Parameter.DNSTarget.IPv4.sin_family != NULL && Parameter.HopLimitOptions.IPv4TTL == 0 || //IPv4
				Parameter.DNSTarget.IPv6.sin6_family != NULL && Parameter.HopLimitOptions.IPv6HopLimit == 0) //IPv6
			{
				Sleep(SENDING_INTERVAL_TIME * TIME_OUT); //5 seconds between every sending.
				continue;
			}

			Sleep((DWORD)Parameter.DomainTestOptions.DomainTestSpeed);
		}
		else {
		//Ramdom domain request
			if (!Parameter.DomainTestOptions.DomainTestCheck)
			{
				memset(Parameter.DomainTestOptions.DomainTest, 0, DOMAIN_MAXSIZE);
				RamdomDomain(Parameter.DomainTestOptions.DomainTest);
				DataLength = CharToDNSQuery(Parameter.DomainTestOptions.DomainTest, DNSQuery.get());
				memcpy(Buffer.get() + sizeof(dns_hdr), DNSQuery.get(), DataLength);
				
				TestQry = (dns_qry *)(Buffer.get() + sizeof(dns_hdr) + DataLength);
				TestQry->Classes = htons(DNS_CLASS_IN);
				if (Protocol == AF_INET6)
					TestQry->Type = htons(DNS_AAAA_RECORDS);
				else 
					TestQry->Type = htons(DNS_A_RECORDS);
			}

		//Send
			if (Alternate)
			{
				if (Protocol == AF_INET6 && Parameter.DNSTarget.Alternate_IPv6.sin6_family != NULL)
					UDPRequest(Buffer.get(), DataLength + sizeof(dns_hdr) + 4U, SetProtocol, QUEUE_MAXLENGTH * QUEUE_PARTNUM, false, Alternate);
				if (Protocol == AF_INET && Parameter.DNSTarget.Alternate_IPv4.sin_family != NULL)
					UDPRequest(Buffer.get(), DataLength + sizeof(dns_hdr) + 4U, SetProtocol, QUEUE_MAXLENGTH * QUEUE_PARTNUM, false, Alternate);
				Alternate = false;
			}
			else {
				UDPRequest(Buffer.get(), DataLength + sizeof(dns_hdr) + 4U, SetProtocol, QUEUE_MAXLENGTH * QUEUE_PARTNUM, false, Alternate);
				Alternate = true;
				continue;
			}

		//Repeat
			Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
			Times++;
		}
	}

	return EXIT_SUCCESS;
}

//Internet Control Message Protocol/ICMP Echo(Ping) request
size_t __fastcall ICMPEcho(void)
{
//Initialization
	std::shared_ptr<char> Buffer(new char[sizeof(icmp_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U]());
	sockaddr_storage SockAddr[] = {{0}, {0}};

//Make a ICMP request echo packet.
	auto icmp = (icmp_hdr *)Buffer.get();
	icmp->Type = 8; //Echo(Ping) request type
	icmp->ID = htons(Parameter.ICMPOptions.ICMPID);
	icmp->Sequence = Parameter.ICMPOptions.ICMPSequence;
	memcpy(Buffer.get() + sizeof(icmp_hdr), Parameter.PaddingDataOptions.PaddingData, Parameter.PaddingDataOptions.PaddingDataLength - 1U);
	icmp->Checksum = GetChecksum((PUINT16)Buffer.get(), sizeof(icmp_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U);
	SYSTEM_SOCKET ICMPSocket = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
	SockAddr[0].ss_family = AF_INET;
	((PSOCKADDR_IN)&(SockAddr[0]))->sin_addr = Parameter.DNSTarget.IPv4.sin_addr;
	if (Parameter.DNSTarget.Alternate_IPv4.sin_family != NULL)
	{
		SockAddr[1U].ss_family = AF_INET;
		((PSOCKADDR_IN)&(SockAddr[1U]))->sin_addr = Parameter.DNSTarget.Alternate_IPv4.sin_addr;
	}

//Check socket.
	if (ICMPSocket == INVALID_SOCKET)
	{
		PrintError(WINSOCK_ERROR, L"ICMP Echo(Ping) request error", WSAGetLastError(), nullptr, NULL);
		return EXIT_FAILURE;
	}

//Set socket timeout.
	if (setsockopt(ICMPSocket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(ICMPSocket);
		PrintError(WINSOCK_ERROR, L"Set ICMP socket timeout error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Send request.
	size_t Times = 0;
	while (true)
	{
		sendto(ICMPSocket, Buffer.get(), (int)(sizeof(icmp_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U), NULL, (PSOCKADDR)&(SockAddr[0]), sizeof(sockaddr_in));
		if (Parameter.DNSTarget.Alternate_IPv4.sin_family != NULL)
			sendto(ICMPSocket, Buffer.get(), (int)(sizeof(icmp_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U), NULL, (PSOCKADDR)&(SockAddr[1U]), sizeof(sockaddr_in));

		if (Times == ONCESEND_TIME)
		{
			Times = 0;
			if (Parameter.HopLimitOptions.IPv4TTL == 0)
			{
				Sleep(SENDING_INTERVAL_TIME * TIME_OUT); //5 seconds between every sending.
				continue;
			}

			Sleep((DWORD)Parameter.ICMPOptions.ICMPSpeed);
		}
		else {
			Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
			Times++;
		}
	}

	closesocket(ICMPSocket);
	return EXIT_SUCCESS;
}

//Internet Control Message Protocol Echo version 6/ICMPv6 Echo(Ping) request
size_t __fastcall ICMPv6Echo(void)
{
//Initialization
	std::shared_ptr<char> Buffer(new char[sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U]()), Exchange(new char[sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U]());
	sockaddr_storage SockAddr[] = {{0}, {0}};
	memset(&SockAddr, 0, sizeof(sockaddr_storage) * 2U);

//Make a IPv6 ICMPv6 request echo packet.
	auto icmpv6 = (icmpv6_hdr *)Buffer.get();
	icmpv6->Type = ICMPV6_REQUEST;
	icmpv6->Code = 0;
	icmpv6->ID = htons(Parameter.ICMPOptions.ICMPID);
	icmpv6->Sequence = htons(Parameter.ICMPOptions.ICMPSequence);
	memcpy(Buffer.get() + sizeof(icmpv6_hdr), Parameter.PaddingDataOptions.PaddingData, Parameter.PaddingDataOptions.PaddingDataLength - 1U);

//Validate local IPv6 address.
	if (!GetLocalAddress(SockAddr[0], AF_INET6))
	{
		PrintError(WINSOCK_ERROR, L"Get localhost address(es) error", NULL, nullptr, NULL);
		return EXIT_FAILURE;
	}
	icmpv6->Checksum = ICMPv6Checksum((PUINT8)Buffer.get(), sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U, Parameter.DNSTarget.IPv6.sin6_addr, ((PSOCKADDR_IN6)&SockAddr[0])->sin6_addr);

	if (Parameter.DNSTarget.Alternate_IPv6.sin6_family != NULL)
	{
		memcpy(Exchange.get(), Buffer.get(), sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U);
		icmpv6->Checksum = ICMPv6Checksum((PUINT8)Buffer.get(), sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U, Parameter.DNSTarget.Alternate_IPv6.sin6_addr, ((PSOCKADDR_IN6)&SockAddr[0])->sin6_addr);
		SockAddr[1U].ss_family = AF_INET6;
		((PSOCKADDR_IN6)&SockAddr[1U])->sin6_addr = Parameter.DNSTarget.Alternate_IPv6.sin6_addr;
	}
	
	SYSTEM_SOCKET ICMPv6Socket = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
	SockAddr[0].ss_family = AF_INET6;
	((PSOCKADDR_IN6)&SockAddr[0])->sin6_addr = Parameter.DNSTarget.IPv6.sin6_addr;
//Check socket.
	if (ICMPv6Socket == INVALID_SOCKET)
	{
		PrintError(WINSOCK_ERROR, L"ICMPv6 Echo(Ping) request error", WSAGetLastError(), nullptr, NULL);
		return EXIT_FAILURE;
	}

//Set socket timeout.
	if (setsockopt(ICMPv6Socket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(ICMPv6Socket);
		PrintError(WINSOCK_ERROR, L"Set ICMPv6 socket timeout error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Send request.
	size_t Times = 0;
	while (true)
	{
		sendto(ICMPv6Socket, Exchange.get(), (int)(sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U), NULL, (PSOCKADDR)&(SockAddr[0]), sizeof(sockaddr_in6));
		if (Parameter.DNSTarget.Alternate_IPv6.sin6_family != NULL)
			sendto(ICMPv6Socket, Buffer.get(), (int)(sizeof(icmpv6_hdr) + Parameter.PaddingDataOptions.PaddingDataLength - 1U), NULL, (PSOCKADDR)&(SockAddr[1U]), sizeof(sockaddr_in6));

		if (Times == ONCESEND_TIME)
		{
			Times = 0;
			if (Parameter.HopLimitOptions.IPv6HopLimit == 0)
			{
				Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
				continue;
			}

			Sleep((DWORD)Parameter.ICMPOptions.ICMPSpeed);
		}
		else {
			Times++;
			Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
		}
	}

	closesocket(ICMPv6Socket);
	return EXIT_SUCCESS;
}

//Transmission and reception of TCP protocol(Independent)
size_t __fastcall TCPRequest(const PSTR Send, const size_t SendSize, PSTR Recv, const size_t RecvSize, const SOCKET_DATA TargetData, const bool Local, const bool Alternate)
{
//Initialization
	std::shared_ptr<char> SendBuffer(new char[sizeof(uint16_t) + SendSize]());
	sockaddr_storage SockAddr = {0};
	SYSTEM_SOCKET TCPSocket = 0;
	memcpy(SendBuffer.get() + sizeof(uint16_t), Send, SendSize);

//Add length of request packet(It must be written in header when transpot with TCP protocol).
	size_t DataLength = SendSize;
	auto ptcp_dns_hdr = (tcp_dns_hdr *)SendBuffer.get();
	ptcp_dns_hdr->Length = htons((uint16_t)SendSize);
	DataLength += sizeof(uint16_t);

	SSIZE_T RecvLen = 0;
//Socket initialization
	if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
	{
		if (Alternate)
		{
			if (Local && Parameter.DNSTarget.Alternate_Local_IPv6.sin6_family != NULL)
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.Alternate_Local_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.Alternate_Local_IPv6.sin6_port;
			}
			else if (Parameter.DNSTarget.Alternate_IPv6.sin6_family != NULL)
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.Alternate_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.Alternate_IPv6.sin6_port;
			}
			else {
				return FALSE;
			}
		}
		else { //Main
			if (Local && Parameter.DNSTarget.Local_IPv6.sin6_family != NULL)
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.Local_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.Local_IPv6.sin6_port;
			}
			else if (Parameter.DNSTarget.IPv6.sin6_family != NULL)
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.IPv6.sin6_port;
			}
			else {
				return FALSE;
			}
		}

		TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		SockAddr.ss_family = AF_INET6;
	}
	else { //IPv4
		if (Alternate)
		{
			if (Local && Parameter.DNSTarget.Alternate_Local_IPv4.sin_family != NULL)
			{
				((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.Alternate_Local_IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = Parameter.DNSTarget.Alternate_Local_IPv4.sin_port;
			}
			else if (Parameter.DNSTarget.Alternate_IPv4.sin_family != NULL)
			{
				((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.Alternate_IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = Parameter.DNSTarget.Alternate_IPv4.sin_port;
			}
			else {
				return FALSE;
			}
		}
		else { //Main
			if (Local && Parameter.DNSTarget.Local_IPv4.sin_family != NULL)
			{
				((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.Local_IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = Parameter.DNSTarget.Local_IPv4.sin_port;
			}
			else if (Parameter.DNSTarget.IPv4.sin_family != NULL)
			{
				((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = Parameter.DNSTarget.IPv4.sin_port;
			}
			else {
				return FALSE;
			}
		}

		TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		SockAddr.ss_family = AF_INET;
	}

//Check socket.
	if (TCPSocket == INVALID_SOCKET)
	{
		closesocket(TCPSocket);
		PrintError(WINSOCK_ERROR, L"TCP request initialization error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}
/*
//TCP KeepAlive Mode
	BOOL bKeepAlive = TRUE;
	if (setsockopt(TCPSocket, SOL_SOCKET, SO_KEEPALIVE, (PSTR)&bKeepAlive, sizeof(bKeepAlive)) == SOCKET_ERROR)
	{
		closesocket(TCPSocket);
		return EXIT_FAILURE;
	}

	tcp_keepalive alive_in = {0};
	tcp_keepalive alive_out = {0};
	alive_in.keepalivetime = TIME_OUT;
	alive_in.keepaliveinterval = TIME_OUT*RELIABLE_SOCKET_TIMEOUT;
	alive_in.onoff = TRUE;
	ULONG ulBytesReturn = 0;
	if (WSAIoctl(TCPSocket, SIO_KEEPALIVE_VALS, &alive_in, sizeof(alive_in), &alive_out, sizeof(alive_out), &ulBytesReturn, NULL, NULL) == SOCKET_ERROR)
	{
		closesocket(TCPSocket);
		return EXIT_FAILURE;
	}
*/
//Set socket timeout.
	if (setsockopt(TCPSocket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&ReliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR || 
		setsockopt(TCPSocket, SOL_SOCKET, SO_RCVTIMEO, (PSTR)&ReliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(TCPSocket);
		PrintError(WINSOCK_ERROR, L"Set TCP socket timeout error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Connect to server.
	if (connect(TCPSocket, (PSOCKADDR)&SockAddr, TargetData.AddrLen) == SOCKET_ERROR) //Connection is RESET or other errors when connecting.
	{
		if (!Alternate && WSAGetLastError() == WSAETIMEDOUT)
		{
			closesocket(TCPSocket);
			return WSAETIMEDOUT;
		}
		else {
			closesocket(TCPSocket);
			return EXIT_FAILURE;
		}
	}

//Send request.
	if (send(TCPSocket, SendBuffer.get(), (int)DataLength, NULL) == SOCKET_ERROR) //Connection is RESET or other errors when sending.
	{
		if (!Alternate && WSAGetLastError() == WSAETIMEDOUT)
		{
			closesocket(TCPSocket);
			return WSAETIMEDOUT;
		}
		else {
			closesocket(TCPSocket);
			return EXIT_FAILURE;
		}
	}

	//Receive result.
	RecvLen = recv(TCPSocket, Recv, (int)RecvSize, NULL) - sizeof(uint16_t);
	if (RecvLen <= 0 || (SSIZE_T)htons(((uint16_t *)Recv)[0]) > RecvLen) //Connection is RESET or other errors(including SOCKET_ERROR) when sending or server fin the connection.
	{
		memset(Recv, 0, RecvSize);
		if (!Alternate && RecvLen == RETURN_ERROR && WSAGetLastError() == WSAETIMEDOUT)
		{
			closesocket(TCPSocket);
			return WSAETIMEDOUT;
		}
		else {
			closesocket(TCPSocket);
			return EXIT_FAILURE;
		}
	}
	else if (RecvLen <= sizeof(dns_hdr) + 1U + sizeof(dns_qry) && htons(((uint16_t *)Recv)[0]) > sizeof(dns_hdr) + 1U + sizeof(dns_qry)) //TCP segment of a reassembled PDU
	{
		size_t PDULen = htons(((uint16_t *)Recv)[0]);
		memset(Recv, 0, RecvSize);
		RecvLen = recv(TCPSocket, Recv, (int)RecvSize, NULL) - sizeof(uint16_t);
		if (RecvLen < (SSIZE_T)PDULen) //Connection is RESET, corrupted packet or sother errors(including SOCKET_ERROR) after sending or finished.
		{
			memset(Recv, 0, RecvSize);
			if (!Alternate && RecvLen == RETURN_ERROR && WSAGetLastError() == WSAETIMEDOUT)
			{
				closesocket(TCPSocket);
				return WSAETIMEDOUT;
			}
			else {
				closesocket(TCPSocket);
				return EXIT_FAILURE;
			}
		}

		closesocket(TCPSocket);
		if (PDULen > sizeof(dns_hdr) + 1U + sizeof(dns_qry) && PDULen <= RecvSize)
		{
			memmove(Recv, Recv + sizeof(uint16_t), PDULen);
		//Mark DNS Cache
			if (Parameter.CacheType != 0)
				MarkDNSCache(Recv, PDULen);

			return PDULen;
		}
		else {
			closesocket(TCPSocket);
			memset(Recv, 0, RecvSize);
			return EXIT_FAILURE;
		}
	}
	else if (RecvLen > sizeof(dns_hdr) + 1U + sizeof(dns_qry))
	{
		closesocket(TCPSocket);
		RecvLen = ntohs(((uint16_t *)Recv)[0]);
		if (RecvLen > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry)) && RecvLen <= (SSIZE_T)RecvSize)
		{
			memmove(Recv, Recv + sizeof(uint16_t), RecvLen);
		//Mark DNS Cache
			if (Parameter.CacheType != 0)
				MarkDNSCache(Recv, RecvLen);

			return RecvLen;
		}
		else {
			return EXIT_FAILURE;
		}
	}

	closesocket(TCPSocket);
	memset(Recv, 0, RecvSize);
	return EXIT_FAILURE;
}

//Transmission of UDP protocol
size_t __fastcall UDPRequest(const PSTR Send, const size_t Length, const SOCKET_DATA TargetData, const size_t Index, const bool Local, const bool Alternate)
{
//Initialization
	sockaddr_storage SockAddr = {0};
	SYSTEM_SOCKET UDPSocket = 0;

//Socket initialization
	if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
	{
		if (Alternate)
		{
			if (Local && Parameter.DNSTarget.Alternate_Local_IPv6.sin6_family != NULL)
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.Alternate_Local_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.Alternate_Local_IPv6.sin6_port;
			}
			else if (Parameter.DNSTarget.Alternate_IPv6.sin6_family != NULL)
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.Alternate_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.Alternate_IPv6.sin6_port;
			}
			else {
				return EXIT_FAILURE;
			}
		}
		else {
			if (Local && Parameter.DNSTarget.Local_IPv6.sin6_family != NULL)
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.Local_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.Local_IPv6.sin6_port;
			}
			else if (Parameter.DNSTarget.IPv6.sin6_family != NULL)
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.IPv6.sin6_port;
			}
			else {
				return EXIT_FAILURE;
			}
		}

		UDPSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		SockAddr.ss_family = AF_INET6;
	}
	else { //IPv4
		if (Alternate)
		{
			if (Local && Parameter.DNSTarget.Alternate_Local_IPv4.sin_family != NULL)
			{
				((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.Alternate_Local_IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = Parameter.DNSTarget.Alternate_Local_IPv4.sin_port;
			}
			else if (Parameter.DNSTarget.Alternate_IPv4.sin_family != NULL)
			{
				((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.Alternate_IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = Parameter.DNSTarget.Alternate_IPv4.sin_port;
			}
			else {
				return EXIT_FAILURE;
			}
		}
		else {
			if (Local && Parameter.DNSTarget.Local_IPv4.sin_family != NULL)
			{
				((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.Local_IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = Parameter.DNSTarget.Local_IPv4.sin_port;
			}
			else if (Parameter.DNSTarget.IPv4.sin_family != NULL)
			{
				((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = Parameter.DNSTarget.IPv4.sin_port;
			}
			else {
				return EXIT_FAILURE;
			}
		}

		UDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		SockAddr.ss_family = AF_INET;
	}

//Check socket.
	if (UDPSocket == INVALID_SOCKET)
	{
		PrintError(WINSOCK_ERROR, L"UDP request initialization error", WSAGetLastError(), nullptr, NULL);
		return EXIT_FAILURE;
	}

//Set socket timeout.
	if (setsockopt(UDPSocket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(UDPSocket);
		PrintError(WINSOCK_ERROR, L"Set UDP socket timeout error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Send request.
	if (sendto(UDPSocket, Send, (int)Length, NULL, (PSOCKADDR)&SockAddr, TargetData.AddrLen) == SOCKET_ERROR)
	{
		closesocket(UDPSocket);
		PrintError(WINSOCK_ERROR, L"UDP request error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Mark port to list.
	if (Index < QUEUE_MAXLENGTH * QUEUE_PARTNUM)
	{
		if (getsockname(UDPSocket, (PSOCKADDR)&SockAddr, (PINT)&TargetData.AddrLen) != 0)
		{
			closesocket(UDPSocket);
			return EXIT_FAILURE;
		}

	//Minimum supported system of GetTickCount64() is Windows Vista.
		if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
		{
			PortList.SendData[Index].SockAddr.ss_family = sizeof(sockaddr_in6);
			((PSOCKADDR_IN6)&PortList.SendData[Index].SockAddr)->sin6_port = ((PSOCKADDR_IN6)&SockAddr)->sin6_port;
		}
		else //IPv4
		{
			PortList.SendData[Index].SockAddr.ss_family = sizeof(sockaddr_in);
			((PSOCKADDR_IN)&PortList.SendData[Index].SockAddr)->sin_port = ((PSOCKADDR_IN)&SockAddr)->sin_port;
		}

	//Mark send time
		#ifdef _WIN64
			AlternateSwapList.PcapAlternateTimeout[Index] = GetTickCount64();
		#else //x86
			AlternateSwapList.PcapAlternateTimeout[Index] = GetTickCount();
		#endif
	}

//Block Port Unreachable messages of system.
	Sleep(UNRELIABLE_SOCKET_TIMEOUT * TIME_OUT);
	closesocket(UDPSocket);
	return EXIT_SUCCESS;
}

//Complete transmission of UDP protocol
size_t __fastcall UDPCompleteRequest(const PSTR Send, const size_t SendSize, PSTR Recv, const size_t RecvSize, const SOCKET_DATA TargetData, const bool Local, const bool Alternate)
{
//Initialization
	sockaddr_storage SockAddr = {0};
	SYSTEM_SOCKET UDPSocket = 0;

//Socket initialization
	if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
	{
		if (Alternate)
		{
			if (Local && Parameter.DNSTarget.Alternate_Local_IPv6.sin6_family != NULL)
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.Alternate_Local_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.Alternate_Local_IPv6.sin6_port;
			}
			else if (Parameter.DNSTarget.IPv6.sin6_family != NULL) //Main
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.Alternate_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.Alternate_IPv6.sin6_port;
			}
			else {
				return EXIT_FAILURE;
			}
		}
		else { //Main
			if (Local && Parameter.DNSTarget.Local_IPv6.sin6_family != NULL)
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.Local_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.Local_IPv6.sin6_port;
			}
			else if (Parameter.DNSTarget.IPv6.sin6_family != NULL) //Main
			{
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = Parameter.DNSTarget.IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = Parameter.DNSTarget.IPv6.sin6_port;
			}
			else {
				return EXIT_FAILURE;
			}
		}

		UDPSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		SockAddr.ss_family = AF_INET6;
	}
	else { //IPv4
		if (Alternate)
		{
			if (Parameter.DNSTarget.Alternate_Local_IPv4.sin_family != NULL)
			{
				((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.Alternate_Local_IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = Parameter.DNSTarget.Alternate_Local_IPv4.sin_port;
			}
			else {
				return EXIT_FAILURE;
			}
		}
		else { //Main
			if (Parameter.DNSTarget.Local_IPv4.sin_family != NULL)
			{
				((PSOCKADDR_IN)&SockAddr)->sin_addr = Parameter.DNSTarget.Local_IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = Parameter.DNSTarget.Local_IPv4.sin_port;
			}
			else {
				return EXIT_FAILURE;
			}
		}

		UDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		SockAddr.ss_family = AF_INET;
	}

//Check socket.
	if (UDPSocket == INVALID_SOCKET)
	{
		PrintError(WINSOCK_ERROR, L"Complete UDP request initialization error", WSAGetLastError(), nullptr, NULL);
		return EXIT_FAILURE;
	}

//Set socket timeout.
	if (setsockopt(UDPSocket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR || 
		setsockopt(UDPSocket, SOL_SOCKET, SO_RCVTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(UDPSocket);
		PrintError(WINSOCK_ERROR, L"Set Complete UDP socket timeout error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Send request.
	if (sendto(UDPSocket, Send, (int)SendSize, NULL, (PSOCKADDR)&SockAddr, TargetData.AddrLen) == SOCKET_ERROR)
	{
		closesocket(UDPSocket);
		PrintError(WINSOCK_ERROR, L"Complete UDP request error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Receive result.
	SSIZE_T RecvLen = recvfrom(UDPSocket, Recv, (int)RecvSize, NULL, (PSOCKADDR)&SockAddr, (PINT)&TargetData.AddrLen);
	if (!Alternate && RecvLen == RETURN_ERROR && WSAGetLastError() == WSAETIMEDOUT)
	{
		closesocket(UDPSocket);
		memset(Recv, 0, RecvSize);
		return WSAETIMEDOUT;
	}
	else if (RecvLen > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry)))
	{
		closesocket(UDPSocket);

	//Mark DNS Cache
		if (Parameter.CacheType != 0)
			MarkDNSCache(Recv, RecvLen);

		return RecvLen;
	}
	else {
		closesocket(UDPSocket);
		memset(Recv, 0, RecvSize);
		return EXIT_FAILURE;
	}
}
