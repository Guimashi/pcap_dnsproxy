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
extern DNSCurveConfiguration DNSCurveParameter;

//DNSCurve verify keypair
bool __fastcall VerifyKeypair(const PUINT8 PublicKey, const PUINT8 SecretKey)
{
	std::shared_ptr<uint8_t> Test_PublicKey(new uint8_t[crypto_box_PUBLICKEYBYTES]()), Test_SecretKey(new uint8_t[crypto_box_PUBLICKEYBYTES]()), Validation(new uint8_t[crypto_box_PUBLICKEYBYTES + crypto_box_SECRETKEYBYTES + crypto_box_ZEROBYTES]());

//Keypair, Nonce and validation data
	crypto_box_curve25519xsalsa20poly1305_keypair(Test_PublicKey.get(), Test_SecretKey.get());
	//Test Nonce, 0x00 - 0x23(ASCII)
	uint8_t Nonce[crypto_box_NONCEBYTES] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11, 
											0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23};
	memcpy(Validation.get() + crypto_box_ZEROBYTES, PublicKey, crypto_box_PUBLICKEYBYTES);

//Verify
	crypto_box_curve25519xsalsa20poly1305(Validation.get(), Validation.get(), crypto_box_PUBLICKEYBYTES + crypto_box_ZEROBYTES, Nonce, Test_PublicKey.get(), SecretKey);
	if (crypto_box_curve25519xsalsa20poly1305_open(Validation.get(), Validation.get(), crypto_box_PUBLICKEYBYTES + crypto_box_ZEROBYTES, Nonce, PublicKey, Test_SecretKey.get()) == RETURN_ERROR)
		return false;

	return true;
}

//DNSCurve initialization
size_t __fastcall DNSCurveInit(void)
{
//DNSCurve signature request TCP Mode
	if (DNSCurveParameter.DNSCurveMode == DNSCURVE_REQUEST_TCPMODE)
	{
	//Main
		if (DNSCurveParameter.DNSCurveTarget.IPv6.sin6_family != NULL && 
			(CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey, crypto_box_BEFORENMBYTES) || 
			CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN)))
		{
			std::thread TCPSignatureRequestThread(TCPSignatureRequest, AF_INET6, false);
			TCPSignatureRequestThread.detach();
		}
		
		if (DNSCurveParameter.DNSCurveTarget.IPv4.sin_family != NULL && 
			(CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey, crypto_box_BEFORENMBYTES) || 
			CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN)))
		{
			std::thread TCPSignatureRequestThread(TCPSignatureRequest, AF_INET, false);
			TCPSignatureRequestThread.detach();
		}

	//Alternate
		if (DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_family != NULL && 
			(CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey, crypto_box_BEFORENMBYTES) || 
			CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN)))
		{
			std::thread TCPSignatureRequestThread(TCPSignatureRequest, AF_INET6, true);
			TCPSignatureRequestThread.detach();
		}
		
		if (DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_family != NULL && 
			(CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey, crypto_box_BEFORENMBYTES) || 
			CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN)))
		{
			std::thread TCPSignatureRequestThread(TCPSignatureRequest, AF_INET, true);
			TCPSignatureRequestThread.detach();
		}
	}

//DNSCurve signature request UDP Mode
//Main
	if (DNSCurveParameter.DNSCurveTarget.IPv6.sin6_family != NULL && 
		(CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey, crypto_box_BEFORENMBYTES) || 
		CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN)))
	{
		std::thread UDPSignatureRequestThread(UDPSignatureRequest, AF_INET6, false);
		UDPSignatureRequestThread.detach();
	}
		
	if (DNSCurveParameter.DNSCurveTarget.IPv4.sin_family != NULL && 
		(CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey, crypto_box_BEFORENMBYTES) || 
		CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN)))
	{
		std::thread UDPSignatureRequestThread(UDPSignatureRequest, AF_INET, false);
		UDPSignatureRequestThread.detach();
	}

//Alternate
	if (DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_family != NULL && 
		(CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey, crypto_box_BEFORENMBYTES) || 
		CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN)))
	{
		std::thread UDPSignatureRequestThread(UDPSignatureRequest, AF_INET6, true);
		UDPSignatureRequestThread.detach();
	}
		
	if (DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_family != NULL && 
		(CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey, crypto_box_BEFORENMBYTES) || 
		CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN)))
	{
		std::thread UDPSignatureRequestThread(UDPSignatureRequest, AF_INET, true);
		UDPSignatureRequestThread.detach();
	}

	return EXIT_SUCCESS;
}

//DNSCurve Local Signature Request
inline size_t LocalSignatureRequest(const PSTR Send, const size_t SendSize, PSTR Recv, const size_t RecvSize)
{
//Initialization
	SYSTEM_SOCKET UDPSocket = 0;
	sockaddr_storage SockAddr;
	int AddrLen = 0;

//Socket initialization
	if (Parameter.DNSTarget.IPv6.sin6_family != NULL) //IPv6
	{
		AddrLen = sizeof(sockaddr_in6);
		UDPSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		((sockaddr_in6 *)&SockAddr)->sin6_family = AF_INET6;
		((sockaddr_in6 *)&SockAddr)->sin6_addr = in6addr_loopback;
		((sockaddr_in6 *)&SockAddr)->sin6_port = Parameter.ListenPort;
	}
	else { //IPv4
		AddrLen = sizeof(sockaddr_in);
		UDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		((sockaddr_in *)&SockAddr)->sin_family = AF_INET;
		((sockaddr_in *)&SockAddr)->sin_addr.S_un.S_addr = htonl(INADDR_LOOPBACK);
		((sockaddr_in *)&SockAddr)->sin_port = Parameter.ListenPort;
	}

//Check socket.
	if (UDPSocket == INVALID_SOCKET)
	{
		PrintError(WINSOCK_ERROR, L"DNSCurve Local Signature request initialization error", WSAGetLastError(), nullptr, NULL);
		return EXIT_FAILURE;
	}

//Set socket timeout.
	if (setsockopt(UDPSocket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR || 
		setsockopt(UDPSocket, SOL_SOCKET, SO_RCVTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(UDPSocket);
		PrintError(WINSOCK_ERROR, L"Set DNSCurve Local Signature socket timeout error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Send request.
	if (sendto(UDPSocket, Send, (int)SendSize, NULL, (PSOCKADDR)&SockAddr, AddrLen) == SOCKET_ERROR)
	{
		closesocket(UDPSocket);
		PrintError(WINSOCK_ERROR, L"DNSCurve Local Signature request error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Receive result.
	SSIZE_T RecvLen = recvfrom(UDPSocket, Recv, (int)RecvSize, NULL, (PSOCKADDR)&SockAddr, (PINT)&AddrLen);
	closesocket(UDPSocket);
	if (RecvLen > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry)))
	{
		return RecvLen;
	}
	else {
		memset(Recv, 0, RecvSize);
		return EXIT_FAILURE;
	}
}

//Send TCP request to get Signature Data of server(s)
inline bool __fastcall TCPSignatureRequest(const uint16_t NetworkLayer, const bool Alternate)
{
//Initialization
	std::shared_ptr<char> SendBuffer(new char[PACKET_MAXSIZE]()), RecvBuffer(new char[LARGE_PACKET_MAXSIZE]());
	sockaddr_storage SockAddr = {0};
	SYSTEM_SOCKET TCPSocket = 0;

//Packet
	size_t DataLength = sizeof(tcp_dns_hdr);
	auto ptcp_dns_hdr = (tcp_dns_hdr *)SendBuffer.get();
	ptcp_dns_hdr->ID = Parameter.DomainTestOptions.DomainTestID;
	ptcp_dns_hdr->Flags = htons(DNS_STANDARD);
	ptcp_dns_hdr->Questions = htons(0x0001);
	
	if (NetworkLayer == AF_INET6) //IPv6
	{
		if (Alternate)
			DataLength += CharToDNSQuery(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6_ProviderName, SendBuffer.get() + DataLength);
		else 
			DataLength += CharToDNSQuery(DNSCurveParameter.DNSCurveTarget.IPv6_ProviderName, SendBuffer.get() + DataLength);
	}
	else { //IPv4
		if (Alternate)
			DataLength += CharToDNSQuery(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4_ProviderName, SendBuffer.get() + DataLength);
		else 
			DataLength += CharToDNSQuery(DNSCurveParameter.DNSCurveTarget.IPv4_ProviderName, SendBuffer.get() + DataLength);
	}

	auto Query = (dns_qry *)(SendBuffer.get() + DataLength);
	Query->Type = htons(DNS_TXT_RECORDS);
	Query->Classes = htons(DNS_CLASS_IN);
	DataLength += sizeof(dns_qry);

//EDNS0 Label
	ptcp_dns_hdr->Additional = htons(0x0001);
	auto EDNS0 = (dns_edns0_label *)(SendBuffer.get() + DataLength);
	EDNS0->Type = htons(DNS_EDNS0_RECORDS);
	EDNS0->UDPPayloadSize = htons(EDNS0_MINSIZE);
	DataLength += sizeof(dns_edns0_label);

	ptcp_dns_hdr->Length = htons((uint16_t)(DataLength - sizeof(uint16_t)));
//Socket initialization
	int AddrLen = 0;
	if (NetworkLayer == AF_INET6) //IPv6
	{
		AddrLen = sizeof(sockaddr_in6);
		SockAddr.ss_family = AF_INET6;
		TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

		if (Alternate)
		{
			((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_addr;
			((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_port;
		}
		else { //Main
			((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_addr;
			((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_port;
		}
	}
	else { //IPv4
		AddrLen = sizeof(sockaddr_in);
		SockAddr.ss_family = AF_INET;
		TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if (Alternate)
		{
			((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_addr;
			((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_port;
		}
		else { //Main
			((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.IPv4.sin_addr;
			((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.IPv4.sin_port;
		}
	}

	SSIZE_T RecvLen = 0;
	while (true)
	{
	//Check socket.
		if (TCPSocket == INVALID_SOCKET)
		{
			PrintError(WINSOCK_ERROR, L"DNSCurve socket(s) initialization error", WSAGetLastError(), nullptr, NULL);
			return false;
		}

	//Set socket timeout.
		if (setsockopt(TCPSocket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&ReliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR || 
			setsockopt(TCPSocket, SOL_SOCKET, SO_RCVTIMEO, (PSTR)&ReliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
		{
			closesocket(TCPSocket);
			PrintError(WINSOCK_ERROR, L"Set TCP socket timeout error", WSAGetLastError(), nullptr, NULL);

			return false;
		}

	//Connect to server.
		if (connect(TCPSocket, (PSOCKADDR)&SockAddr, AddrLen) == SOCKET_ERROR) //Connection is RESET or other errors when connecting.
		{
			closesocket(TCPSocket);
			Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
			continue;
		}

	//Send request.
		if (send(TCPSocket, SendBuffer.get(), (int)DataLength, NULL) == SOCKET_ERROR)
		{
			closesocket(TCPSocket);
			PrintError(DNSCURVE_ERROR, L"TCP get signature data request error", WSAGetLastError(), nullptr, NULL);

			if (NetworkLayer == AF_INET6) //IPv6
				TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
			else //IPv4
				TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

			Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
			continue;
		}
		else {
			RecvLen = recv(TCPSocket, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, NULL);
			if (RecvLen <= 0 || (SSIZE_T)htons(((uint16_t *)RecvBuffer.get())[0]) > RecvLen) //Connection is RESET or other errors(including SOCKET_ERROR) when sending or server fin the connection.
			{
				closesocket(TCPSocket);
				if (NetworkLayer == AF_INET6) //IPv6
					TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
				else //IPv4
					TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

				memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
				Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
				continue;
			}
			else if (htons(((uint16_t *)RecvBuffer.get())[0]) <= sizeof(dns_hdr) + 1U + sizeof(dns_qry) + sizeof(dns_txt_record) + DNSCRYPT_TXT_RECORD_LENGTH) //TCP segment of a reassembled PDU
			{
				size_t PDULen = htons(((uint16_t *)RecvBuffer.get())[0]);
				memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
				RecvLen = recv(TCPSocket, RecvBuffer.get(), LARGE_PACKET_MAXSIZE, NULL) - sizeof(uint16_t);
				if (RecvLen <= 0 || RecvLen < (SSIZE_T)PDULen) //Connection is RESET or other errors(including SOCKET_ERROR) after sending or finished, also may be a corrupted packet.
				{
					closesocket(TCPSocket);
					if (NetworkLayer == AF_INET6) //IPv6
						TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
					else //IPv4
						TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

					memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
					Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
					continue;
				}
				else if (PDULen > sizeof(dns_hdr) + 1U + sizeof(dns_qry) + sizeof(dns_txt_record) + DNSCRYPT_TXT_RECORD_LENGTH)
				{
					memmove(RecvBuffer.get(), RecvBuffer.get() + sizeof(uint16_t), PDULen);

				//Check result.
					if (Alternate)
					{
						if (NetworkLayer == AF_INET6) //IPv6
						{
							if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_ALTERNATEIPV6) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
							{
								closesocket(TCPSocket);
								if (NetworkLayer == AF_INET6) //IPv6
									TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
								else //IPv4
									TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

								memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
								Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
								continue;
							}

							memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
						}
						else { //IPv4
							if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_ALTERNATEIPV4) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
							{
								closesocket(TCPSocket);
								if (NetworkLayer == AF_INET6) //IPv6
									TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
								else //IPv4
									TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

								memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
								Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
								continue;
							}

							memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
						}
					}
					else {
						if (NetworkLayer == AF_INET6) //IPv6
						{
							if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_MAINIPV6) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
							{
								closesocket(TCPSocket);
								if (NetworkLayer == AF_INET6) //IPv6
									TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
								else //IPv4
									TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

								memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
								Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
								continue;
							}

							memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
						}
						else { //IPv4
							if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_MAINIPV4) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
							{
								closesocket(TCPSocket);
								if (NetworkLayer == AF_INET6) //IPv6
									TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
								else //IPv4
									TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

								memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
								Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
								continue;
							}

							memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
						}
					}
				}
				else {
					closesocket(TCPSocket);
					if (NetworkLayer == AF_INET6) //IPv6
						TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
					else //IPv4
						TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

					memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
					Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
					continue;
				}
			}
			else if (htons(((uint16_t *)RecvBuffer.get())[0]) > sizeof(dns_hdr) + 1U + sizeof(dns_qry) + sizeof(dns_txt_record) + DNSCRYPT_TXT_RECORD_LENGTH)
			{
				RecvLen = htons(((uint16_t *)RecvBuffer.get())[0]);
				memmove(RecvBuffer.get(), RecvBuffer.get() + sizeof(uint16_t), RecvLen);

			//Check result.
				if (Alternate)
				{
					if (NetworkLayer == AF_INET6) //IPv6
					{
						if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_ALTERNATEIPV6) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
						{
							closesocket(TCPSocket);
							if (NetworkLayer == AF_INET6) //IPv6
								TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
							else //IPv4
								TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

							memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
							Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
							continue;
						}

						memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
					}
					else { //IPv4
						if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_ALTERNATEIPV4) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
						{
							closesocket(TCPSocket);
							if (NetworkLayer == AF_INET6) //IPv6
								TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
							else //IPv4
								TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

							memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
							Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
							continue;
						}

						memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
					}
				}
				else {
					if (NetworkLayer == AF_INET6) //IPv6
					{
						if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_MAINIPV6) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
						{
							closesocket(TCPSocket);
							if (NetworkLayer == AF_INET6) //IPv6
								TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
							else //IPv4
								TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

							memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
							Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
							continue;
						}

						memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
					}
					else { //IPv4
						if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_MAINIPV4) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
						{
							closesocket(TCPSocket);
							if (NetworkLayer == AF_INET6) //IPv6
								TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
							else //IPv4
								TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

							memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
							Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
							continue;
						}

						memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
					}
				}
			}
			else {
				closesocket(TCPSocket);
				if (NetworkLayer == AF_INET6) //IPv6
					TCPSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
				else //IPv4
					TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

				memset(RecvBuffer.get(), 0, LARGE_PACKET_MAXSIZE);
				Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
				continue;
			}
		}

		Sleep((DWORD)DNSCurveParameter.KeyRecheckTime);
	}

	return true;
}

//Send UDP request to get Signature Data of server(s)
inline bool __fastcall UDPSignatureRequest(const uint16_t NetworkLayer, const bool Alternate)
{
//Initialization
	std::shared_ptr<char> SendBuffer(new char[PACKET_MAXSIZE]()), RecvBuffer(new char[PACKET_MAXSIZE]());
	sockaddr_storage SockAddr = {0};
	SYSTEM_SOCKET UDPSocket = 0;

//Packet
	size_t DataLength = sizeof(dns_hdr);
	dns_hdr *pdns_hdr = (dns_hdr *)SendBuffer.get();
	pdns_hdr->ID = Parameter.DomainTestOptions.DomainTestID;
	pdns_hdr->Flags = htons(DNS_STANDARD);
	pdns_hdr->Questions = htons(0x0001);

	if (NetworkLayer == AF_INET6) //IPv6
	{
		if (Alternate)
			DataLength += CharToDNSQuery(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6_ProviderName, SendBuffer.get() + DataLength);
		else 
			DataLength += CharToDNSQuery(DNSCurveParameter.DNSCurveTarget.IPv6_ProviderName, SendBuffer.get() + DataLength);
	}
	else { //IPv4
		if (Alternate)
			DataLength += CharToDNSQuery(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4_ProviderName, SendBuffer.get() + DataLength);
		else 
			DataLength += CharToDNSQuery(DNSCurveParameter.DNSCurveTarget.IPv4_ProviderName, SendBuffer.get() + DataLength);
	}

	auto Query = (dns_qry *)(SendBuffer.get() + DataLength);
	Query->Type = htons(DNS_TXT_RECORDS);
	Query->Classes = htons(DNS_CLASS_IN);
	DataLength += sizeof(dns_qry);

//EDNS0 Label
	pdns_hdr->Additional = htons(0x0001);
	auto EDNS0 = (dns_edns0_label *)(SendBuffer.get() + DataLength);
	EDNS0->Type = htons(DNS_EDNS0_RECORDS);
	EDNS0->UDPPayloadSize = htons(EDNS0_MINSIZE);
	DataLength += sizeof(dns_edns0_label);

//Socket initialization
	int AddrLen = 0;
	if (NetworkLayer == AF_INET6) //IPv6
	{
		SockAddr.ss_family = AF_INET6;
		AddrLen = sizeof(sockaddr_in6);
		UDPSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);

		if (Alternate)
		{
			((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_addr;
			((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_port;
		}
		else { //Main
			((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_addr;
			((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_port;
		}
	}
	else { //IPv4
		SockAddr.ss_family = AF_INET;
		AddrLen = sizeof(sockaddr_in);
		UDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		if (Alternate)
		{
			((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_addr;
			((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_port;
		}
		else { //Main
			((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.IPv4.sin_addr;
			((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.IPv4.sin_port;
		}
	}

//Check socket.
	if (UDPSocket == INVALID_SOCKET)
	{
		PrintError(WINSOCK_ERROR, L"DNSCurve socket(s) initialization error", WSAGetLastError(), nullptr, NULL);
		return false;
	}

//Set socket timeout.
	if (setsockopt(UDPSocket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR || 
		setsockopt(UDPSocket, SOL_SOCKET, SO_RCVTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(UDPSocket);
		PrintError(WINSOCK_ERROR, L"Set UDP socket timeout error", WSAGetLastError(), nullptr, NULL);

		return false;
	}

//Send request.
	SSIZE_T RecvLen = 0;
	while (true)
	{
		if (sendto(UDPSocket, SendBuffer.get(), (int)DataLength, NULL, (PSOCKADDR)&SockAddr, AddrLen) == SOCKET_ERROR)
		{
			closesocket(UDPSocket);
			PrintError(DNSCURVE_ERROR, L"UDP get signature data request error", WSAGetLastError(), nullptr, NULL);

			if (NetworkLayer == AF_INET6) //IPv6
				UDPSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
			else //IPv4
				UDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

		//Check socket.
			if (UDPSocket == INVALID_SOCKET)
			{
				PrintError(WINSOCK_ERROR, L"DNSCurve socket(s) initialization error", WSAGetLastError(), nullptr, NULL);
				return false;
			}

		//Set socket timeout.
			if (setsockopt(UDPSocket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR || 
				setsockopt(UDPSocket, SOL_SOCKET, SO_RCVTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
			{
				closesocket(UDPSocket);
				PrintError(WINSOCK_ERROR, L"Set UDP socket timeout error", WSAGetLastError(), nullptr, NULL);

				return false;
			}

			Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
			continue;
		}
		else {
			RecvLen = recvfrom(UDPSocket, RecvBuffer.get(), PACKET_MAXSIZE, NULL, (PSOCKADDR)&SockAddr, &AddrLen);
			if (RecvLen > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry) + sizeof(dns_txt_record) + DNSCRYPT_TXT_RECORD_LENGTH))
			{
			//Check result.
				if (Alternate)
				{
					if (NetworkLayer == AF_INET6) //IPv6
					{
						if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_ALTERNATEIPV6) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
						{
							memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
							Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
							continue;
						}

						memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
					}
					else { //IPv4
						if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_ALTERNATEIPV4) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
						{
							memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
							Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
							continue;
						}

						memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
					}
				}
				else {
					if (NetworkLayer == AF_INET6) //IPv6
					{
						if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_MAINIPV6) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
						{
							memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
							Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
							continue;
						}

						memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
					}
					else { //IPv4
						if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_MAINIPV4) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
							CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
						{
							memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
							Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
							continue;
						}

						memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
					}
				}
			}
			else {
				memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
				if (LocalSignatureRequest(SendBuffer.get(), (int)DataLength, RecvBuffer.get(), PACKET_MAXSIZE) > sizeof(dns_hdr) + 1U + sizeof(dns_qry) + sizeof(dns_txt_record) + DNSCRYPT_TXT_RECORD_LENGTH)
				{
				//Check result.
					if (Alternate)
					{
						if (NetworkLayer == AF_INET6) //IPv6
						{
							if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_ALTERNATEIPV6) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
							{
								memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
								Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
								continue;
							}

							memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
						}
						else { //IPv4
							if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_ALTERNATEIPV4) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
							{
								memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
								Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
								continue;
							}

							memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
						}
					}
					else {
						if (NetworkLayer == AF_INET6) //IPv6
						{
							if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_MAINIPV6) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
							{
								memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
								Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
								continue;
							}

							memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
						}
						else { //IPv4
							if (!GetSignatureData(RecvBuffer.get() + sizeof(dns_hdr) + strlen(RecvBuffer.get() + sizeof(dns_hdr)) + 1U + sizeof(dns_qry), DNSCURVE_MAINIPV4) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint, crypto_box_PUBLICKEYBYTES) || 
								CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
							{
								memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
								Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
								continue;
							}

							memset(RecvBuffer.get(), 0, PACKET_MAXSIZE);
						}
					}
				}
				else {
					Sleep(SENDING_INTERVAL_TIME * TIME_OUT);
					continue;
				}
			}
		}

		Sleep((DWORD)DNSCurveParameter.KeyRecheckTime);
	}

	return true;
}

//Get Signature Data of server form packets
bool __fastcall GetSignatureData(const PSTR Buffer, const size_t ServerType)
{
	std::shared_ptr<char> DeBuffer(new char[PACKET_MAXSIZE]());

//Get Signature Data.
	auto TXTRecords = (dns_txt_record *)Buffer;
	if (TXTRecords->Name == htons(DNS_QUERY_PTR) && 
		TXTRecords->Length == htons(TXTRecords->TXTLength + 1U) && TXTRecords->TXTLength == DNSCRYPT_TXT_RECORD_LENGTH)
	{
		auto TXTHeader = (dnscurve_txt_hdr *)(Buffer + sizeof(dns_txt_record));
		if (memcmp(&TXTHeader->CertMagicNumber, DNSCRYPT_CERT_MAGIC, sizeof(uint16_t)) == 0 && 
			TXTHeader->MajorVersion == htons(0x0001) && TXTHeader->MinorVersion == 0)
		{
			ULONGLONG SignatureLength = 0;
			dnscurve_txt_signature *Signature = nullptr;

			if (ServerType == DNSCURVE_MAINIPV6)
			{
			//Check Signature.
				if (crypto_sign_ed25519_open((PUINT8)DeBuffer.get(), &SignatureLength, (PUINT8)(Buffer + sizeof(dns_txt_record) + sizeof(dnscurve_txt_hdr)), TXTRecords->TXTLength - sizeof(dnscurve_txt_hdr), DNSCurveParameter.DNSCurveKey.IPv6_ServerPublicKey) == RETURN_ERROR)
				{
					PrintError(DNSCURVE_ERROR, L"IPv6 Main Server Fingerprint signature validation error", NULL, nullptr, NULL);
					return false;
				}

				Signature = (dnscurve_txt_signature *)DeBuffer.get();
			//Check available(time) Signature.
				if (time(NULL) >= (time_t)ntohl(Signature->Cert_Time_Begin) && time(NULL) <= (time_t)ntohl(Signature->Cert_Time_End))
				{
					memcpy(DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, Signature->MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
					memcpy(DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint, Signature->PublicKey, crypto_box_PUBLICKEYBYTES);
					crypto_box_curve25519xsalsa20poly1305_beforenm(DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey, DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint, DNSCurveParameter.DNSCurveKey.Client_SecretKey);
					
					return true;
				}
				else {
					PrintError(DNSCURVE_ERROR, L"IPv6 Main Server Fingerprint signature is not available", NULL, nullptr, NULL);
				}
			}
			else if (ServerType == DNSCURVE_MAINIPV4)
			{
			//Check Signature.
				if (crypto_sign_ed25519_open((PUINT8)DeBuffer.get(), &SignatureLength, (PUINT8)(Buffer + sizeof(dns_txt_record) + sizeof(dnscurve_txt_hdr)), TXTRecords->TXTLength - sizeof(dnscurve_txt_hdr), DNSCurveParameter.DNSCurveKey.IPv4_ServerPublicKey) == RETURN_ERROR)
				{
					PrintError(DNSCURVE_ERROR, L"IPv4 Main Server Fingerprint signature validation error", NULL, nullptr, NULL);
					return false;
				}

				Signature = (dnscurve_txt_signature *)DeBuffer.get();
			//Check available(time) Signature.
				if (time(NULL) >= (time_t)ntohl(Signature->Cert_Time_Begin) && time(NULL) <= (time_t)ntohl(Signature->Cert_Time_End))
				{
					memcpy(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, Signature->MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
					memcpy(DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint, Signature->PublicKey, crypto_box_PUBLICKEYBYTES);
					crypto_box_curve25519xsalsa20poly1305_beforenm(DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey, DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint, DNSCurveParameter.DNSCurveKey.Client_SecretKey);

					return true;
				}
				else {
					PrintError(DNSCURVE_ERROR, L"IPv4 Main Server Fingerprint signature is not available", NULL, nullptr, NULL);
				}
			}
			else if (ServerType == DNSCURVE_ALTERNATEIPV6)
			{
			//Check Signature.
				if (crypto_sign_ed25519_open((PUINT8)DeBuffer.get(), &SignatureLength, (PUINT8)(Buffer + sizeof(dns_txt_record) + sizeof(dnscurve_txt_hdr)), TXTRecords->TXTLength - sizeof(dnscurve_txt_hdr), DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerPublicKey) == RETURN_ERROR)
				{
					PrintError(DNSCURVE_ERROR, L"IPv6 Alternate Server Fingerprint signature validation error", NULL, nullptr, NULL);
					return false;
				}

				Signature = (dnscurve_txt_signature *)DeBuffer.get();
			//Check available(time) Signature.
				if (time(NULL) >= (time_t)ntohl(Signature->Cert_Time_Begin) && time(NULL) <= (time_t)ntohl(Signature->Cert_Time_End))
				{
					memcpy(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, Signature->MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
					memcpy(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint, Signature->PublicKey, crypto_box_PUBLICKEYBYTES);
					crypto_box_curve25519xsalsa20poly1305_beforenm(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey, DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint, DNSCurveParameter.DNSCurveKey.Client_SecretKey);

					return true;
				}
				else {
					PrintError(DNSCURVE_ERROR, L"IPv6 Alternate Server Fingerprint signature is not available", NULL, nullptr, NULL);
				}
			}
			else if (ServerType == DNSCURVE_ALTERNATEIPV4)
			{
			//Check Signature.
				if (crypto_sign_ed25519_open((PUINT8)DeBuffer.get(), &SignatureLength, (PUINT8)(Buffer + sizeof(dns_txt_record) + sizeof(dnscurve_txt_hdr)), TXTRecords->TXTLength - sizeof(dnscurve_txt_hdr), DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerPublicKey) == RETURN_ERROR)
				{
					PrintError(DNSCURVE_ERROR, L"IPv4 Alternate Server Fingerprint signature validation error", NULL, nullptr, NULL);
					return false;
				}

				Signature = (dnscurve_txt_signature *)DeBuffer.get();
			//Check available(time) Signature.
				if (time(NULL) >= (time_t)ntohl(Signature->Cert_Time_Begin) && time(NULL) <= (time_t)ntohl(Signature->Cert_Time_End))
				{
					memcpy(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, Signature->MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
					memcpy(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint, Signature->PublicKey, crypto_box_PUBLICKEYBYTES);
					crypto_box_curve25519xsalsa20poly1305_beforenm(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey, DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint, DNSCurveParameter.DNSCurveKey.Client_SecretKey);
					
					return true;
				}
				else {
					PrintError(DNSCURVE_ERROR, L"IPv4 Alternate Server Fingerprint signature is not available", NULL, nullptr, NULL);
				}
			}
		}
	}

	return false;
}

//Transmission of DNSCurve TCP protocol
size_t __fastcall TCPDNSCurveRequest(const PSTR Send, const size_t SendSize, PSTR Recv, const size_t RecvSize, const SOCKET_DATA TargetData, const bool Alternate, const bool Encryption)
{
//Initialization
/*	if (Encryption && (SendSize > DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES) || 
		RecvSize < DNSCurveParameter.DNSCurvePayloadSize || RecvSize < crypto_box_ZEROBYTES + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES + SendSize))
			return EXIT_FAILURE;
*/
	sockaddr_storage SockAddr = {0};
	SYSTEM_SOCKET TCPSocket = 0;

//Socket initialization
	if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
	{
		SockAddr.ss_family = AF_INET6;
		TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

		if (Alternate && DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_family != NULL)
		{
			if (Encryption)
			{
				if (!CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey, crypto_box_BEFORENMBYTES) &&
					!CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_addr;
					((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_port;
				}
				else {
					return EXIT_FAILURE;
				}
			}
			else {
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_port;
			}
		}
		else if (DNSCurveParameter.DNSCurveTarget.IPv6.sin6_family != NULL) //Main
		{
			if (Encryption)
			{
				if (!CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey, crypto_box_BEFORENMBYTES) &&
					!CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_addr;
					((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_port;
				}
				else {
					return EXIT_FAILURE;
				}
			}
			else {
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_port;
			}
		}
		else {
			return EXIT_FAILURE;
		}
	}
	else { //IPv4
		if (Alternate && DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_family != NULL)
		{
		//Encryption mode
			if (Encryption)
			{
				if (!CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey, crypto_box_BEFORENMBYTES) &&
					!CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_addr;
					((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_port;
				}
				else {
					return EXIT_FAILURE;
				}
			}
		//Normal mode
			else {
				((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_port;
			}
		}
		else if (DNSCurveParameter.DNSCurveTarget.IPv4.sin_family != NULL) //Main
		{
		//Encryption mode
			if (Encryption)
			{
				if (!CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey, crypto_box_BEFORENMBYTES) &&
					!CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.IPv4.sin_addr;
					((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.IPv4.sin_port;
				}
				else {
					return EXIT_FAILURE;
				}
			}
		//Normal mode
			else {
				((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.IPv4.sin_port;
			}
		}
		else {
			return EXIT_FAILURE;
		}

		SockAddr.ss_family = AF_INET;
		TCPSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}

//Check socket.
	if (TCPSocket == INVALID_SOCKET)
	{
		PrintError(WINSOCK_ERROR, L"DNSCurve socket(s) initialization error", WSAGetLastError(), nullptr, NULL);
		return EXIT_FAILURE;
	}

//Set socket timeout.
	if (setsockopt(TCPSocket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&ReliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR || 
		setsockopt(TCPSocket, SOL_SOCKET, SO_RCVTIMEO, (PSTR)&ReliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(TCPSocket);
		PrintError(WINSOCK_ERROR, L"Set UDP socket timeout error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Encryption mode
	std::shared_ptr<uint8_t> WholeNonce;
	if (Encryption)
	{
		std::shared_ptr<uint8_t> TempBuffer(new uint8_t[crypto_box_NONCEBYTES]());
		WholeNonce.swap(TempBuffer);
		TempBuffer.reset();

	//Make nonce.
		randombytes_stir();
		*(uint32_t *)WholeNonce.get() = randombytes_random();
		*(uint32_t *)(WholeNonce.get() + sizeof(uint32_t)) = randombytes_random();
		*(uint32_t *)(WholeNonce.get() + sizeof(uint32_t) * 2U) = randombytes_random();
		memset(WholeNonce.get() + crypto_box_HALF_NONCEBYTES, 0, crypto_box_HALF_NONCEBYTES);
		if (randombytes_close() != 0)
		{
			closesocket(TCPSocket);
			PrintError(DNSCURVE_ERROR, L"Ramdom module close error", NULL, nullptr, NULL);

			return EXIT_FAILURE;
		}

	//Make a crypto box.
		std::shared_ptr<char> Buffer(new char[DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES)]());
		memcpy(Buffer.get() + crypto_box_ZEROBYTES, Send, SendSize);
		Buffer.get()[crypto_box_ZEROBYTES + SendSize] = '\x80';

		if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
		{
			if (Alternate)
			{
				if (crypto_box_curve25519xsalsa20poly1305_afternm(
					(PUCHAR)Recv + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES - crypto_box_BOXZEROBYTES, 
					(PUCHAR)Buffer.get(), DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES), 
					WholeNonce.get(), DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey) != 0)
				{
					closesocket(TCPSocket);
					memset(Recv, 0, RecvSize);

					return EXIT_FAILURE;
				}

			//Packet(A part)
				memcpy(Recv + sizeof(uint16_t), DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
			}
			else { //Main
				if (crypto_box_curve25519xsalsa20poly1305_afternm(
					(PUCHAR)Recv + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES - crypto_box_BOXZEROBYTES, 
					(PUCHAR)Buffer.get(), DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES), 
					WholeNonce.get(), DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey) != 0)
				{
					closesocket(TCPSocket);
					memset(Recv, 0, RecvSize);

					return EXIT_FAILURE;
				}

			//Packet(A part)
				memcpy(Recv + sizeof(uint16_t), DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
			}
		}
		else { //IPv4
			if (Alternate)
			{
				if (crypto_box_curve25519xsalsa20poly1305_afternm(
					(PUCHAR)Recv + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES - crypto_box_BOXZEROBYTES, 
					(PUCHAR)Buffer.get(), DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES), 
					WholeNonce.get(), DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey) != 0)
				{
					closesocket(TCPSocket);
					memset(Recv, 0, RecvSize);

					return EXIT_FAILURE;
				}

			//Packet(A part)
				memcpy(Recv + sizeof(uint16_t), DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
			}
			else { //Main
				if (crypto_box_curve25519xsalsa20poly1305_afternm(
					(PUCHAR)Recv + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES - crypto_box_BOXZEROBYTES, 
					(PUCHAR)Buffer.get(), DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES),
					WholeNonce.get(), DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey) != 0)
				{
					closesocket(TCPSocket);
					memset(Recv, 0, RecvSize);

					return EXIT_FAILURE;
				}

			//Packet(A part)
				memcpy(Recv + sizeof(uint16_t), DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
			}
		}

		//Packet(B part)
		memcpy(Recv + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN, DNSCurveParameter.DNSCurveKey.Client_PublicKey, crypto_box_PUBLICKEYBYTES);
		memcpy(Recv + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES, WholeNonce.get(), crypto_box_HALF_NONCEBYTES);
		auto ptcp_dns_hdr = (tcp_dns_hdr *)Recv;
		ptcp_dns_hdr->Length = htons((uint16_t)(DNSCurveParameter.DNSCurvePayloadSize - sizeof(uint16_t)));
		Buffer.reset();
		memset(WholeNonce.get(), 0, crypto_box_NONCEBYTES);

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
		if (send(TCPSocket, Recv, (int)DNSCurveParameter.DNSCurvePayloadSize, NULL) == SOCKET_ERROR)
		{
			closesocket(TCPSocket);
			PrintError(WINSOCK_ERROR, L"DNSCurve TCP request error", WSAGetLastError(), nullptr, NULL);

			if (!Alternate && WSAGetLastError() == WSAETIMEDOUT)
				return WSAETIMEDOUT;
			else 
				return EXIT_FAILURE;
		}

		memset(Recv, 0, RecvSize);
	}
//Normal mode
	else {
		std::shared_ptr<char> Buffer(new char[sizeof(uint16_t) + SendSize]());
		memcpy(Buffer.get() + sizeof(uint16_t), Send, SendSize);
		auto BufferLength = (uint16_t *)Buffer.get();
		*BufferLength = htons((uint16_t)SendSize);

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
		if (send(TCPSocket, Buffer.get(), (int)(sizeof(uint16_t) + SendSize), NULL) == SOCKET_ERROR)
		{
			PrintError(WINSOCK_ERROR, L"DNSCurve TCP request error", WSAGetLastError(), nullptr, NULL);
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
	}

//Receive result.
	SSIZE_T RecvLen = recv(TCPSocket, Recv, (int)RecvSize, NULL);
	if (!Alternate && WSAGetLastError() == WSAETIMEDOUT)
	{
		closesocket(TCPSocket);
		return WSAETIMEDOUT;
	}

	//Encryption mode
	if (Encryption)
	{
		if (RecvLen > (SSIZE_T)(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES + sizeof(dns_hdr) + 1U + sizeof(dns_qry)))
		{
			if (RecvLen >= (SSIZE_T)ntohs(((uint16_t *)Recv)[0]))
			{
				RecvLen = ntohs(((uint16_t *)Recv)[0]);
				memmove(Recv, Recv + sizeof(uint16_t), RecvLen);
				memset(Recv + RecvLen, 0, RecvSize - RecvLen);
			}
			else {
				closesocket(TCPSocket);
				memset(Recv, 0, RecvSize);

				return EXIT_FAILURE;
			}

		//Check receive magic number.
			if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
			{
				if (Alternate)
				{
					if (memcmp(Recv, DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN) != 0)
					{
						closesocket(TCPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}

				//Copy whole nonce.
					memcpy(WholeNonce.get(), Recv + DNSCURVE_MAGIC_QUERY_LEN, crypto_box_NONCEBYTES);

				//Open crypto box.
					memset(Recv, 0, DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);
					memmove(Recv + crypto_box_BOXZEROBYTES, Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					if (crypto_box_curve25519xsalsa20poly1305_open_afternm((PUCHAR)Recv, (PUCHAR)Recv, 
						RecvLen + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES), 
						WholeNonce.get(), DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey) != 0)
					{
						closesocket(TCPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}
					memmove(Recv, Recv + crypto_box_ZEROBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					for (SSIZE_T Index = RecvLen - (SSIZE_T)(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);Index > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry));Index--)
					{
						if ((UCHAR)Recv[Index] == 0x80)
						{
							RecvLen = Index;
							break;
						}
					}

				//Mark DNS Cache
				if (Parameter.CacheType != 0)
					MarkDNSCache(Recv, RecvLen);

				//Return.
					closesocket(TCPSocket);
					memset(Recv + RecvLen, 0, RecvSize - RecvLen);
					return RecvLen;
				}
				else { //Main
					if (memcmp(Recv, DNSCurveParameter.DNSCurveMagicNumber.IPv6_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN) != 0)
					{
						closesocket(TCPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}

				//Copy whole nonce.
					memcpy(WholeNonce.get(), Recv + DNSCURVE_MAGIC_QUERY_LEN, crypto_box_NONCEBYTES);

				//Open crypto box.
					memset(Recv, 0, DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);
					memmove(Recv + crypto_box_BOXZEROBYTES, Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					if (crypto_box_curve25519xsalsa20poly1305_open_afternm((PUCHAR)Recv, (PUCHAR)Recv, 
						RecvLen + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES), 
						WholeNonce.get(), DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey) != 0)
					{
						closesocket(TCPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}
					memmove(Recv, Recv + crypto_box_ZEROBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					for (SSIZE_T Index = RecvLen - (SSIZE_T)(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);Index > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry));Index--)
					{
						if ((UCHAR)Recv[Index] == 0x80)
						{
							RecvLen = Index;
							break;
						}
					}

				//Mark DNS Cache
				if (Parameter.CacheType != 0)
					MarkDNSCache(Recv, RecvLen);

				//Return.
					closesocket(TCPSocket);
					memset(Recv + RecvLen, 0, RecvSize - RecvLen);

					return RecvLen;
				}
			}
			else { //IPv4
				if (Alternate)
				{
					if (memcmp(Recv, DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN) != 0)
					{
						closesocket(TCPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}

				//Copy whole nonce.
					memcpy(WholeNonce.get(), Recv + DNSCURVE_MAGIC_QUERY_LEN, crypto_box_NONCEBYTES);

				//Open crypto box.
					memset(Recv, 0, DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);
					memmove(Recv + crypto_box_BOXZEROBYTES, Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					if (crypto_box_curve25519xsalsa20poly1305_open_afternm((PUCHAR)Recv, (PUCHAR)Recv, 
						RecvLen + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES), 
						WholeNonce.get(), DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey) != 0)
					{
						closesocket(TCPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}
					memmove(Recv, Recv + crypto_box_ZEROBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					for (SSIZE_T Index = RecvLen - (SSIZE_T)(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);Index > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry));Index--)
					{
						if ((UCHAR)Recv[Index] == 0x80)
						{
							RecvLen = Index;
							break;
						}
					}

				//Mark DNS Cache
				if (Parameter.CacheType != 0)
					MarkDNSCache(Recv, RecvLen);

				//Return.
					closesocket(TCPSocket);
					memset(Recv + RecvLen, 0, RecvSize - RecvLen);

					return RecvLen;
				}
				else { //Main
					if (memcmp(Recv, DNSCurveParameter.DNSCurveMagicNumber.IPv4_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN) != 0)
					{
						closesocket(TCPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}

				//Copy whole nonce.
					memcpy(WholeNonce.get(), Recv + DNSCURVE_MAGIC_QUERY_LEN, crypto_box_NONCEBYTES);

				//Open crypto box.
					memset(Recv, 0, DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);
					memmove(Recv + crypto_box_BOXZEROBYTES, Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					if (crypto_box_curve25519xsalsa20poly1305_open_afternm((PUCHAR)Recv, (PUCHAR)Recv, 
						RecvLen + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES), 
						WholeNonce.get(), DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey) != 0)
					{
						closesocket(TCPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}
					memmove(Recv, Recv + crypto_box_ZEROBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					for (SSIZE_T Index = RecvLen - (SSIZE_T)(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);Index > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry));Index--)
					{
						if ((UCHAR)Recv[Index] == 0x80)
						{
							RecvLen = Index;
							break;
						}
					}

				//Mark DNS Cache
				if (Parameter.CacheType != 0)
					MarkDNSCache(Recv, RecvLen);

				//Return.
					closesocket(TCPSocket);
					memset(Recv + RecvLen, 0, RecvSize - RecvLen);

					return RecvLen;
				}
			}
		}
	}
//Normal mode
	else {
		if (RecvLen > 0 && (SSIZE_T)htons(((uint16_t *)Recv)[0]) <= RecvLen)
		{
			if (htons(((uint16_t *)Recv)[0]) > sizeof(dns_hdr) + 1U + sizeof(dns_qry))
			{
				RecvLen = htons(((uint16_t *)Recv)[0]);
				memmove(Recv, Recv + sizeof(uint16_t), RecvLen);

			//Mark DNS Cache
				if (Parameter.CacheType != 0)
					MarkDNSCache(Recv, RecvLen);

			//Return.
				closesocket(TCPSocket);
				return RecvLen;
			}
			else { //TCP segment of a reassembled PDU or incorrect packets
				size_t PDULen = htons(((uint16_t *)Recv)[0]);
				memset(Recv, 0, RecvSize);
				RecvLen = recv(TCPSocket, Recv, (int)RecvSize, NULL);
				if (PDULen > sizeof(dns_hdr) + 1U + sizeof(dns_qry) && PDULen <= RecvSize)
				{
				//Mark DNS Cache
					if (Parameter.CacheType != 0)
						MarkDNSCache(Recv, RecvLen);

				//Return.
					closesocket(TCPSocket);
					return RecvLen;
				}
			}
		}
	}

	closesocket(TCPSocket);
	memset(Recv, 0, RecvSize);
	return EXIT_FAILURE;
}

//Transmission of DNSCurve UDP protocol
size_t __fastcall UDPDNSCurveRequest(const PSTR Send, const size_t SendSize, PSTR Recv, const size_t RecvSize, const SOCKET_DATA TargetData, const bool Alternate, const bool Encryption)
{
//Initialization
/*	if (Encryption && (SendSize > DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES) || 
		RecvSize < DNSCurveParameter.DNSCurvePayloadSize || RecvSize < crypto_box_ZEROBYTES + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES + SendSize))
			return EXIT_FAILURE;
*/
	sockaddr_storage SockAddr = {0};
	SYSTEM_SOCKET UDPSocket = 0;

//Socket initialization
	if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
	{
		if (Alternate && DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_family != NULL)
		{
			if (Encryption)
			{
				if (!CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey, crypto_box_BEFORENMBYTES) &&
					!CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_addr;
					((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_port;
				}
				else {
					return EXIT_FAILURE;
				}
			}
			else {
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_port;
			}
		}
		else if (DNSCurveParameter.DNSCurveTarget.IPv6.sin6_family != NULL) //Main
		{
			if (Encryption)
			{
				if (!CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey, crypto_box_BEFORENMBYTES) &&
					!CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_addr;
					((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_port;
				}
				else {
					return EXIT_FAILURE;
				}
			}
			else {
				((PSOCKADDR_IN6)&SockAddr)->sin6_addr = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_addr;
				((PSOCKADDR_IN6)&SockAddr)->sin6_port = DNSCurveParameter.DNSCurveTarget.IPv6.sin6_port;
			}
		}
		else {
			return EXIT_FAILURE;
		}

		SockAddr.ss_family = AF_INET6;
		UDPSocket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
	}
	else { //IPv4
		if (Alternate && DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_family != NULL)
		{
		//Encryption mode
			if (Encryption)
			{
				if (!CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey, crypto_box_BEFORENMBYTES) &&
					!CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_addr;
					((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_port;
				}
				else {
					return EXIT_FAILURE;
				}
			}
		//Normal mode
			else {
				((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_port;
			}
		}
		else if (DNSCurveParameter.DNSCurveTarget.IPv4.sin_family != NULL) //Main
		{
		//Encryption mode
			if (Encryption)
			{
				if (!CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey, crypto_box_BEFORENMBYTES) &&
					!CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.IPv4.sin_addr;
					((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.IPv4.sin_port;
				}
				else {
					return EXIT_FAILURE;
				}
			}
		//Normal mode
			else {
				((PSOCKADDR_IN)&SockAddr)->sin_addr = DNSCurveParameter.DNSCurveTarget.IPv4.sin_addr;
				((PSOCKADDR_IN)&SockAddr)->sin_port = DNSCurveParameter.DNSCurveTarget.IPv4.sin_port;
			}
		}
		else {
			return EXIT_FAILURE;
		}

		SockAddr.ss_family = AF_INET;
		UDPSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	}

//Check socket.
	if (UDPSocket == INVALID_SOCKET)
	{
		PrintError(WINSOCK_ERROR, L"DNSCurve socket(s) initialization error", WSAGetLastError(), nullptr, NULL);
		return EXIT_FAILURE;
	}

//Set socket timeout.
	if (setsockopt(UDPSocket, SOL_SOCKET, SO_SNDTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR || 
		setsockopt(UDPSocket, SOL_SOCKET, SO_RCVTIMEO, (PSTR)&UnreliableSocketTimeout, sizeof(timeval)) == SOCKET_ERROR)
	{
		closesocket(UDPSocket);
		PrintError(WINSOCK_ERROR, L"Set UDP socket timeout error", WSAGetLastError(), nullptr, NULL);

		return EXIT_FAILURE;
	}

//Encryption mode
	std::shared_ptr<uint8_t> WholeNonce;
	if (Encryption)
	{
		std::shared_ptr<uint8_t> TempBuffer(new uint8_t[crypto_box_NONCEBYTES]());
		WholeNonce.swap(TempBuffer);
		TempBuffer.reset();

	//Make nonce.
		randombytes_stir();
		*(uint32_t *)WholeNonce.get() = randombytes_random();
		*(uint32_t *)(WholeNonce.get() + sizeof(uint32_t)) = randombytes_random();
		*(uint32_t *)(WholeNonce.get() + sizeof(uint32_t) * 2U) = randombytes_random();
		memset(WholeNonce.get() + crypto_box_HALF_NONCEBYTES, 0, crypto_box_HALF_NONCEBYTES);
		if (randombytes_close() != 0)
		{
			closesocket(UDPSocket);
			PrintError(DNSCURVE_ERROR, L"Ramdom module close error", NULL, nullptr, NULL);

			return EXIT_FAILURE;
		}

	//Make a crypto box.
		std::shared_ptr<char> Buffer(new char[DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES)]());
		memcpy(Buffer.get() + crypto_box_ZEROBYTES, Send, SendSize);

		if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
		{
			if (Alternate)
			{
				if (crypto_box_curve25519xsalsa20poly1305_afternm(
					(PUCHAR)Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES - crypto_box_BOXZEROBYTES, 
					(PUCHAR)Buffer.get(), DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES), 
					WholeNonce.get(), DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey) != 0)
				{
					closesocket(UDPSocket);
					memset(Recv, 0, RecvSize);

					return EXIT_FAILURE;
				}

			//Packet(A part)
				memcpy(Recv, DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
			}
			else { //Main
				if (crypto_box_curve25519xsalsa20poly1305_afternm(
					(PUCHAR)Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES - crypto_box_BOXZEROBYTES, 
					(PUCHAR)Buffer.get(), DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES), 
					WholeNonce.get(), DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey) != 0)
				{
					closesocket(UDPSocket);
					memset(Recv, 0, RecvSize);

					return EXIT_FAILURE;
				}

			//Packet(A part)
				memcpy(Recv, DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
			}
		}
		else { //IPv4
			if (Alternate)
			{
				if (crypto_box_curve25519xsalsa20poly1305_afternm(
					(PUCHAR)Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES - crypto_box_BOXZEROBYTES, 
					(PUCHAR)Buffer.get(), DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES), 
					WholeNonce.get(), DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey) != 0)
				{
					closesocket(UDPSocket);
					memset(Recv, 0, RecvSize);

					return EXIT_FAILURE;
				}

			//Packet(A part)
				memcpy(Recv, DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
			}
			else { //Main
				if (crypto_box_curve25519xsalsa20poly1305_afternm(
					(PUCHAR)Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES - crypto_box_BOXZEROBYTES, 
					(PUCHAR)Buffer.get(), DNSCurveParameter.DNSCurvePayloadSize + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES),
					WholeNonce.get(), DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey) != 0)
				{
					closesocket(UDPSocket);
					memset(Recv, 0, RecvSize);

					return EXIT_FAILURE;
				}

			//Packet(A part)
				memcpy(Recv, DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN);
			}
		}

		//Packet(B part)
		memcpy(Recv + DNSCURVE_MAGIC_QUERY_LEN, DNSCurveParameter.DNSCurveKey.Client_PublicKey, crypto_box_PUBLICKEYBYTES);
		memcpy(Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES, WholeNonce.get(), crypto_box_HALF_NONCEBYTES);
		Buffer.reset();
		memset(WholeNonce.get(), 0, crypto_box_NONCEBYTES);

//Send request.
		if (sendto(UDPSocket, Recv, (int)DNSCurveParameter.DNSCurvePayloadSize, NULL, (PSOCKADDR)&SockAddr, TargetData.AddrLen) == SOCKET_ERROR)
		{
			closesocket(UDPSocket);
			PrintError(WINSOCK_ERROR, L"DNSCurve UDP request error", WSAGetLastError(), nullptr, NULL);

			return EXIT_FAILURE;
		}

		memset(Recv, 0, RecvSize);
	}
//Normal mode
	else {
		WholeNonce.reset();
		if (sendto(UDPSocket, Send, (int)SendSize, NULL, (PSOCKADDR)&SockAddr, TargetData.AddrLen) == SOCKET_ERROR)
		{
			closesocket(UDPSocket);
			PrintError(WINSOCK_ERROR, L"DNSCurve UDP request error", WSAGetLastError(), nullptr, NULL);

			return EXIT_FAILURE;
		}
	}

//Receive result.
	SSIZE_T RecvLen = recvfrom(UDPSocket, Recv, (int)RecvSize, NULL, (PSOCKADDR)&SockAddr, (PINT)&TargetData.AddrLen);
	//Encryption mode
	if (Encryption)
	{
		if (RecvLen > (SSIZE_T)(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES + sizeof(dns_hdr) + 1U + sizeof(dns_qry)))
		{
		//Check receive magic number.
			if (TargetData.AddrLen == sizeof(sockaddr_in6)) //IPv6
			{
				if (Alternate)
				{
					if (memcmp(Recv, DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN) != 0)
					{
						closesocket(UDPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}

				//Copy whole nonce.
					memcpy(WholeNonce.get(), Recv + DNSCURVE_MAGIC_QUERY_LEN, crypto_box_NONCEBYTES);

				//Open crypto box.
					memset(Recv, 0, DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);
					memmove(Recv + crypto_box_BOXZEROBYTES, Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					if (crypto_box_curve25519xsalsa20poly1305_open_afternm((PUCHAR)Recv, (PUCHAR)Recv, 
						RecvLen + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES), 
						WholeNonce.get(), DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey) != 0)
					{
						closesocket(UDPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}
					memmove(Recv, Recv + crypto_box_ZEROBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					for (SSIZE_T Index = RecvLen - (SSIZE_T)(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);Index > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry));Index--)
					{
						if ((UCHAR)Recv[Index] == 0x80)
						{
							RecvLen = Index;
							break;
						}
					}

				//Mark DNS Cache
				if (Parameter.CacheType != 0)
					MarkDNSCache(Recv, RecvLen);

				//Return.
					closesocket(UDPSocket);
					memset(Recv + RecvLen, 0, RecvSize - RecvLen);
					return RecvLen;
				}
				else { //Main
					if (memcmp(Recv, DNSCurveParameter.DNSCurveMagicNumber.IPv6_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN) != 0)
					{
						closesocket(UDPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}

				//Copy whole nonce.
					memcpy(WholeNonce.get(), Recv + DNSCURVE_MAGIC_QUERY_LEN, crypto_box_NONCEBYTES);

				//Open crypto box.
					memset(Recv, 0, DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);
					memmove(Recv + crypto_box_BOXZEROBYTES, Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					if (crypto_box_curve25519xsalsa20poly1305_open_afternm((PUCHAR)Recv, (PUCHAR)Recv, 
						RecvLen + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES), 
						WholeNonce.get(), DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey) != 0)
					{
						closesocket(UDPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}
					memmove(Recv, Recv + crypto_box_ZEROBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					for (SSIZE_T Index = RecvLen - (SSIZE_T)(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);Index > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry));Index--)
					{
						if ((UCHAR)Recv[Index] == 0x80)
						{
							RecvLen = Index;
							break;
						}
					}

				//Mark DNS Cache
				if (Parameter.CacheType != 0)
					MarkDNSCache(Recv, RecvLen);

				//Return.
					closesocket(UDPSocket);
					memset(Recv + RecvLen, 0, RecvSize - RecvLen);
					return RecvLen;
				}
			}
			else { //IPv4
				if (Alternate)
				{
					if (memcmp(Recv, DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN) != 0)
					{
						closesocket(UDPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}

				//Copy whole nonce.
					memcpy(WholeNonce.get(), Recv + DNSCURVE_MAGIC_QUERY_LEN, crypto_box_NONCEBYTES);

				//Open crypto box.
					memset(Recv, 0, DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);
					memmove(Recv + crypto_box_BOXZEROBYTES, Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					if (crypto_box_curve25519xsalsa20poly1305_open_afternm((PUCHAR)Recv, (PUCHAR)Recv, 
						RecvLen + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES), 
						WholeNonce.get(), DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey) != 0)
					{
						closesocket(UDPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}
					memmove(Recv, Recv + crypto_box_ZEROBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					for (SSIZE_T Index = RecvLen - (SSIZE_T)(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);Index > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry));Index--)
					{
						if ((UCHAR)Recv[Index] == 0x80)
						{
							RecvLen = Index;
							break;
						}
					}

				//Mark DNS Cache
				if (Parameter.CacheType != 0)
					MarkDNSCache(Recv, RecvLen);

				//Return.
					closesocket(UDPSocket);
					memset(Recv + RecvLen, 0, RecvSize - RecvLen);
					return RecvLen;
				}
				else { //Main
					if (memcmp(Recv, DNSCurveParameter.DNSCurveMagicNumber.IPv4_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN) != 0)
					{
						closesocket(UDPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}

				//Copy whole nonce.
					memcpy(WholeNonce.get(), Recv + DNSCURVE_MAGIC_QUERY_LEN, crypto_box_NONCEBYTES);

				//Open crypto box.
					memset(Recv, 0, DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);
					memmove(Recv + crypto_box_BOXZEROBYTES, Recv + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					if (crypto_box_curve25519xsalsa20poly1305_open_afternm((PUCHAR)Recv, (PUCHAR)Recv, 
						RecvLen + crypto_box_BOXZEROBYTES - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES), 
						WholeNonce.get(), DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey) != 0)
					{
						closesocket(UDPSocket);
						memset(Recv, 0, RecvSize);

						return EXIT_FAILURE;
					}
					memmove(Recv, Recv + crypto_box_ZEROBYTES, RecvLen - (DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES));
					for (SSIZE_T Index = RecvLen - (SSIZE_T)(DNSCURVE_MAGIC_QUERY_LEN + crypto_box_NONCEBYTES);Index > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry));Index--)
					{
						if ((UCHAR)Recv[Index] == 0x80)
						{
							RecvLen = Index;
							break;
						}
					}

				//Mark DNS Cache
				if (Parameter.CacheType != 0)
					MarkDNSCache(Recv, RecvLen);

				//Return.
					closesocket(UDPSocket);
					memset(Recv + RecvLen, 0, RecvSize - RecvLen);
					return RecvLen;
				}
			}
		}
	}
	//Normal mode
	else {
		if (RecvLen > (SSIZE_T)(sizeof(dns_hdr) + 1U + sizeof(dns_qry)))
		{
		//Mark DNS Cache
			if (Parameter.CacheType != 0)
				MarkDNSCache(Recv, RecvLen);

		//Return.
			closesocket(UDPSocket);
			return RecvLen;
		}
	}

	closesocket(UDPSocket);
	memset(Recv, 0, RecvSize);
	return EXIT_FAILURE;
}
