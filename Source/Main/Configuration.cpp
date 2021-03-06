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

//Version define
#define INI_VERSION       0.4         //Current version of configuration file
#define HOSTS_VERSION     0.4         //Current version of hosts file

//Codepage define
#define ANSI              1U          //ANSI Codepage
#define UTF_8             65001U      //Microsoft Windows Codepage of UTF-8
#define UTF_16_LE         1200U       //Microsoft Windows Codepage of UTF-16 Little Endian/LE
#define UTF_16_BE         1201U       //Microsoft Windows Codepage of UTF-16 Big Endian/BE
#define UTF_32_LE         12000U      //Microsoft Windows Codepage of UTF-32 Little Endian/LE
#define UTF_32_BE         12001U      //Microsoft Windows Codepage of UTF-32 Big Endian/BE

//Read Text input types defines
//#define READTEXT_PARAMETER     0
#define READTEXT_HOSTS         1U
#define READTEXT_IPFILTER      2U

//Next line type define
//CR/Carriage Return is 0x0D and LF/Line Feed is 0x0A.
#define NEXTLINETYPE_CRLF 1U
#define NEXTLINETYPE_LF   2U
#define NEXTLINETYPE_CR   3U

//Compare addresses defines
#define COMPARE_LESS      1U
#define COMPARE_EQUAL     2U
#define COMPARE_GREATER   3U

extern std::wstring Path;
extern Configuration Parameter;
extern std::vector<HostsTable> *HostsListUsing, *HostsListModificating;
extern std::vector<AddressRange> *AddressRangeUsing, *AddressRangeModificating;
extern const char *DomainTable;
extern DNSCurveConfiguration DNSCurveParameter;
extern std::mutex HostsListLock, AddressRangeLock;

size_t TestIndex = 0;

//Read texts
inline bool Configuration::ReadText(const FILE *Input, const size_t InputType, const PWSTR FileName)
{
//Initialization
	std::shared_ptr<char> Buffer(new char[FILE_BUFFER_SIZE]()), Addition(new char[FILE_BUFFER_SIZE]());
	size_t FileLocation = 0, Encoding = 0, NextLineType = 0, ReadLength = 0, AdditionLength = 0, Line = 1U, Index = 0, Start = 0;
	auto Comments = false, Local = false;

//Read data
	while (!feof((FILE *)Input))
	{
	//Reset Local
		if (InputType == READTEXT_HOSTS) //ReadHosts
			Local = false;

		memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
		ReadLength = fread_s(Buffer.get(), FILE_BUFFER_SIZE, sizeof(char), FILE_BUFFER_SIZE, (FILE *)Input);
		if (Encoding == NULL)
		{
			ReadEncoding(Buffer.get(), ReadLength, Encoding, NextLineType); //Read encoding
			ReadEncoding(Buffer.get(), ReadLength, Encoding, NextLineType); //Read next line type
			if (Encoding == UTF_8)
			{
				memcpy(Addition.get(), Buffer.get() + 3U, FILE_BUFFER_SIZE - 3U);
				memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
				memcpy(Buffer.get(), Addition.get(), FILE_BUFFER_SIZE - 3U);
				memset(Addition.get(), 0, FILE_BUFFER_SIZE);
				ReadLength -= 3U;
			}
			else if (Encoding == UTF_16_LE || Encoding == UTF_16_BE)
			{
				memcpy(Addition.get(), Buffer.get() + 2U, FILE_BUFFER_SIZE - 2U);
				memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
				memcpy(Buffer.get(), Addition.get(), FILE_BUFFER_SIZE - 2U);
				memset(Addition.get(), 0, FILE_BUFFER_SIZE);
				ReadLength -= 2U;
			}
			else if (Encoding == UTF_32_LE || Encoding == UTF_32_BE)
			{
				memcpy(Addition.get(), Buffer.get() + 4U, FILE_BUFFER_SIZE - 4U);
				memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
				memcpy(Buffer.get(), Addition.get(), FILE_BUFFER_SIZE - 4U);
				memset(Addition.get(), 0, FILE_BUFFER_SIZE);
				ReadLength -= 4U;
			}
		}

		for (Index = 0, Start = 0;Index < ReadLength;Index++)
		{
		//CR/Carriage Return and LF/Line Feed
			if ((Encoding == UTF_8 || Encoding == ANSI || 
				(Encoding == UTF_16_LE || Encoding == UTF_32_LE) && Buffer.get()[Index + 1U] == 0 || 
				(Encoding == UTF_16_BE || Encoding == UTF_32_BE) && Buffer.get()[Index - 1U] == 0) && 
				(Buffer.get()[Index] == 0x0D || Buffer.get()[Index] == 0x0A))
			{
				if (InputType == READTEXT_HOSTS) //ReadHosts
				{
					if (Index - Start > 2U) //Minimum length of IPv6 addresses and regular expression
					{
						if (Parameter.ReadHostsData(Addition.get(), FileName, Line, Local, Comments) == EXIT_FAILURE)
						{
							Line++;
							memset(Addition.get(), 0, FILE_BUFFER_SIZE);
							continue;
						}
					}
				}
				else if (InputType == READTEXT_IPFILTER) //ReadIPFilter
				{
					if (Index - Start > 2U) //Minimum length of comment
					{
						if (Parameter.ReadIPFilterData(Addition.get(), FileName, Line, Comments) == EXIT_FAILURE)
						{
							Line++;
							memset(Addition.get(), 0, FILE_BUFFER_SIZE);
							continue;
						}
					}
				}
				else { //ReadParameter
					if (Index - Start > 2U) //Minimum length of rules
					{
						if (Parameter.ReadParameterData(Addition.get(), FileName, Line, Comments) == EXIT_FAILURE)
							return false;
					}
				}

				memset(Addition.get(), 0, FILE_BUFFER_SIZE);
				AdditionLength = 0;
				Start = Index;
			//Mark lines.
				if (NextLineType == NEXTLINETYPE_CRLF || NextLineType == NEXTLINETYPE_CR)
				{
					if (Buffer.get()[Index] == 0x0D)
						Line++;
				}
				else {
					if (Buffer.get()[Index] == 0x0A)
						Line++;
				}

				continue;
			}
		//Last line
			else if (Index == ReadLength - 1U && ReadLength < FILE_BUFFER_SIZE - 4U) //BOM of UTF
			{
				Addition.get()[strlen(Addition.get())] = Buffer.get()[Index];
				if (InputType == READTEXT_HOSTS) //ReadHosts
				{
					if (Parameter.ReadHostsData(Addition.get(), FileName, Line, Local, Comments) == EXIT_FAILURE)
					{
						memset(Addition.get(), 0, FILE_BUFFER_SIZE);
						AdditionLength = 0;
						break;
					}
				}
				else if (InputType == READTEXT_IPFILTER) //ReadIPFilter
				{
					if (Parameter.ReadIPFilterData(Addition.get(), FileName, Line, Comments) == EXIT_FAILURE)
					{
						memset(Addition.get(), 0, FILE_BUFFER_SIZE);
						AdditionLength = 0;
						break;
					}
				}
				else { //ReadParameter
					if (Parameter.ReadParameterData(Addition.get(), FileName, Line, Comments) == EXIT_FAILURE)
						return false;
				}
			}
		//ASCII data
			else if (Buffer.get()[Index] != 0)
			{
				if (AdditionLength < FILE_BUFFER_SIZE)
				{
					Addition.get()[AdditionLength] = Buffer.get()[Index];
					AdditionLength++;
				}
				else {
					if (InputType == READTEXT_HOSTS) //ReadHosts
					{
						::PrintError(HOSTS_ERROR, L"Data of a line is too long", NULL, FileName, Line);

						memset(Addition.get(), 0, FILE_BUFFER_SIZE);
						AdditionLength = 0;
						continue;
					}
					else if (InputType == READTEXT_IPFILTER) //ReadIPFilter
					{
						::PrintError(IPFILTER_ERROR, L"Data of a line is too long", NULL, FileName, Line);

						memset(Addition.get(), 0, FILE_BUFFER_SIZE);
						AdditionLength = 0;
						continue;
					}
					else { //ReadParameter
						::PrintError(PARAMETER_ERROR, L"Data of a line is too long", NULL, FileName, Line);
						return false;
					}
				}
			}
		}
	}

	return true;
}

//Read parameter from configuration file
size_t Configuration::ReadParameter(void)
{
//Initialization
	FILE *Input = nullptr;
	size_t Index = 0;

//Open file.
	std::wstring ConfigPath[] = {Path, Path, Path};
	ConfigPath[0].append(L"Config.ini");
	ConfigPath[1U].append(L"Config.conf");
	ConfigPath[2U].append(L"Config");
	for (Index = 0;Index < sizeof(ConfigPath) / sizeof(std::wstring);Index++)
	{
		if (_wfopen_s(&Input, ConfigPath[Index].c_str(), L"rb") == 0)
		{
			if (Input != nullptr)
				break;
		}
		
	//Check all configuration files.
		if (Index == sizeof(ConfigPath) / sizeof(std::wstring) - 1U)
		{
			::PrintError(PARAMETER_ERROR, L"Cannot open any configuration files", NULL, nullptr, NULL);
			return EXIT_FAILURE;
		}
	}

//Read data
	if (!ReadText(Input, 0/* READTEXT_PARAMETER */, (PWSTR)ConfigPath[Index].c_str()))
		return EXIT_FAILURE;
	fclose(Input);

//Check parameters.
	if (this->Version > INI_VERSION) //Version check
	{
		::PrintError(PARAMETER_ERROR, L"Configuration file version error", NULL, nullptr, NULL);
		return EXIT_FAILURE;
	}
	else if (this->Version < INI_VERSION)
	{
		::PrintError(PARAMETER_ERROR, L"Configuration file is not the latest version", NULL, nullptr, NULL);
	}

	//DNS Cache check
	if (this->CacheType != 0 && this->CacheParameter == 0)
	{
		::PrintError(PARAMETER_ERROR, L"DNS Cache error", NULL, nullptr, NULL);
		return EXIT_FAILURE;
	}

	//DNS and Alternate Targets check
	if (this->DNSTarget.IPv4.sin_family == NULL && this->DNSTarget.IPv6.sin6_family == NULL || 
		this->DNSTarget.IPv4.sin_family == NULL && this->DNSTarget.Alternate_IPv4.sin_family != NULL || this->DNSTarget.IPv6.sin6_family == NULL && this->DNSTarget.Alternate_IPv6.sin6_family != NULL || 
		this->DNSTarget.Local_IPv4.sin_family == NULL && this->DNSTarget.Alternate_Local_IPv4.sin_family != NULL || this->DNSTarget.Local_IPv6.sin6_family == NULL && this->DNSTarget.Alternate_Local_IPv6.sin6_family != NULL || 
		DNSCurveParameter.DNSCurveTarget.IPv6.sin6_family == NULL && DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_family != NULL || DNSCurveParameter.DNSCurveTarget.IPv4.sin_family == NULL && DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_family != NULL)
	{
		::PrintError(PARAMETER_ERROR, L"DNS Targets error", NULL, nullptr, NULL);
		return EXIT_FAILURE;
	}

	if (this->HopLimitOptions.HopLimitFluctuation > 0) //Hop Limit or TTL Fluctuations check
	{
	//IPv4
		if (this->HopLimitOptions.IPv4TTL > 0 && 
			((size_t)this->HopLimitOptions.IPv4TTL + (size_t)this->HopLimitOptions.HopLimitFluctuation > U8_MAXNUM || 
			(SSIZE_T)this->HopLimitOptions.IPv4TTL < (SSIZE_T)this->HopLimitOptions.HopLimitFluctuation + 1) || 
			this->HopLimitOptions.Alternate_IPv4TTL > 0 && 
			((size_t)this->HopLimitOptions.Alternate_IPv4TTL + (size_t)this->HopLimitOptions.HopLimitFluctuation > U8_MAXNUM || 
			(SSIZE_T)this->HopLimitOptions.Alternate_IPv4TTL < (SSIZE_T)this->HopLimitOptions.HopLimitFluctuation + 1) || 
	//IPv6
			this->HopLimitOptions.IPv6HopLimit > 0 && 
			((size_t)this->HopLimitOptions.IPv6HopLimit + (size_t)this->HopLimitOptions.HopLimitFluctuation > U8_MAXNUM || 
			(SSIZE_T)this->HopLimitOptions.IPv6HopLimit < (SSIZE_T)this->HopLimitOptions.HopLimitFluctuation + 1) || 
			this->HopLimitOptions.Alternate_IPv6HopLimit > 0 && 
			((size_t)this->HopLimitOptions.Alternate_IPv6HopLimit + (size_t)this->HopLimitOptions.HopLimitFluctuation > U8_MAXNUM || 
			(SSIZE_T)this->HopLimitOptions.Alternate_IPv6HopLimit < (SSIZE_T)this->HopLimitOptions.HopLimitFluctuation + 1))
		{
			::PrintError(PARAMETER_ERROR, L"Hop Limit or TTL Fluctuations error", NULL, nullptr, NULL); //Hop Limit and TTL must between 1 and 255.
			return EXIT_FAILURE;
		}
	}

	if (this->EDNS0PayloadSize < OLD_DNS_MAXSIZE)
	{
		if (this->EDNS0PayloadSize > 0)
			::PrintError(PARAMETER_ERROR, L"EDNS0 PayloadSize must longer than 512 bytes(Old DNS packets minimum supported size)", NULL, nullptr, NULL);
		this->EDNS0PayloadSize = EDNS0_MINSIZE; //Default UDP maximum payload size.
	}
	else if (this->EDNS0PayloadSize >= PACKET_MAXSIZE - sizeof(ipv6_hdr) - sizeof(udp_hdr))
	{
		::PrintError(PARAMETER_ERROR, L"EDNS0 PayloadSize may be too long", NULL, nullptr, NULL);
		this->EDNS0PayloadSize = EDNS0_MINSIZE;
	}
	if (this->RquestMode != REQUEST_TCPMODE) //TCP Mode options check
		this->TCPDataCheck = false;
	if (this->DNSTarget.IPv4.sin_family == NULL) //IPv4 options check
		this->IPv4DataCheck = false;

	if (this->DNSCurve) //DNSCurve options check
	{
	//Libsodium initialization
		if (sodium_init() != 0)
		{
			::PrintError(DNSCURVE_ERROR, L"Libsodium initialization error", NULL, nullptr, NULL);
			return EXIT_FAILURE;
		}

	//Client keys check
		if (!CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Client_PublicKey, crypto_box_PUBLICKEYBYTES) && !CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Client_SecretKey, crypto_box_SECRETKEYBYTES) && 
			!VerifyKeypair(DNSCurveParameter.DNSCurveKey.Client_PublicKey, DNSCurveParameter.DNSCurveKey.Client_SecretKey))
		{
			::PrintError(DNSCURVE_ERROR, L"Client keypair(public key and secret key) error", NULL, nullptr, NULL);
			return EXIT_FAILURE;
		}
		else if (DNSCurveParameter.Encryption)
		{
			memset(DNSCurveParameter.DNSCurveKey.Client_PublicKey, 0, crypto_box_PUBLICKEYBYTES);
			memset(DNSCurveParameter.DNSCurveKey.Client_SecretKey, 0, crypto_box_SECRETKEYBYTES);
			crypto_box_curve25519xsalsa20poly1305_keypair(DNSCurveParameter.DNSCurveKey.Client_PublicKey, DNSCurveParameter.DNSCurveKey.Client_SecretKey);
		}
		else {
			delete[] DNSCurveParameter.DNSCurveKey.Client_PublicKey;
			delete[] DNSCurveParameter.DNSCurveKey.Client_SecretKey;
			DNSCurveParameter.DNSCurveKey.Client_PublicKey = nullptr;
			DNSCurveParameter.DNSCurveKey.Client_SecretKey = nullptr;
		}

	//DNSCurve target(s) check
		if (DNSCurveParameter.DNSCurveTarget.IPv4.sin_family == NULL && DNSCurveParameter.DNSCurveTarget.IPv6.sin6_family == NULL || 
			DNSCurveParameter.DNSCurveTarget.IPv4.sin_family == NULL && DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_family != NULL || 
			DNSCurveParameter.DNSCurveTarget.IPv6.sin6_family == NULL && DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_family != NULL) 
		{
			::PrintError(PARAMETER_ERROR, L"DNSCurve Targets error", NULL, nullptr, NULL);
			return EXIT_FAILURE;
		}

	//Eencryption options check
		if (DNSCurveParameter.EncryptionOnly && !DNSCurveParameter.Encryption)
		{
			DNSCurveParameter.Encryption = true;
			::PrintError(PARAMETER_ERROR, L"DNSCurve encryption options error", NULL, nullptr, NULL);
		}

	//Main(IPv6)
		if (DNSCurveParameter.Encryption && DNSCurveParameter.DNSCurveTarget.IPv6.sin6_family != NULL)
		{
		//Empty Server Fingerprint
			if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint, crypto_box_PUBLICKEYBYTES))
			{
			//Encryption Only mode check
				if (DNSCurveParameter.EncryptionOnly && 
					CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve Encryption Only mode error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}

			//Empty Provider Name
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv6_ProviderName, DOMAIN_MAXSIZE))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve empty Provider Name error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}

			//Empty Public Key
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv6_ServerPublicKey, crypto_box_PUBLICKEYBYTES))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve empty Public Key error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}
			}
			else {
				crypto_box_curve25519xsalsa20poly1305_beforenm(DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey, DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint, DNSCurveParameter.DNSCurveKey.Client_SecretKey);
			}
		}
		else {
			delete[] DNSCurveParameter.DNSCurveTarget.IPv6_ProviderName;
			delete[] DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey;
			delete[] DNSCurveParameter.DNSCurveKey.IPv6_ServerPublicKey;
			delete[] DNSCurveParameter.DNSCurveMagicNumber.IPv6_ReceiveMagicNumber;
			delete[] DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber;

			DNSCurveParameter.DNSCurveTarget.IPv6_ProviderName = nullptr;
			DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey = nullptr;
			DNSCurveParameter.DNSCurveKey.IPv6_ServerPublicKey = nullptr;
			DNSCurveParameter.DNSCurveMagicNumber.IPv6_ReceiveMagicNumber = nullptr;
			DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber = nullptr;
		}

	//Main(IPv4)
		if (DNSCurveParameter.Encryption && DNSCurveParameter.DNSCurveTarget.IPv4.sin_family != NULL)
		{
		//Empty Server Fingerprint
			if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint, crypto_box_PUBLICKEYBYTES))
			{
			//Encryption Only mode check
				if (DNSCurveParameter.EncryptionOnly && 
					CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve Encryption Only mode error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}

			//Empty Provider Name
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.IPv4_ProviderName, DOMAIN_MAXSIZE))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve empty Provider Name error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}

			//Empty Public Key
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.IPv4_ServerPublicKey, crypto_box_PUBLICKEYBYTES))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve empty Public Key error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}
			}
			else {
				crypto_box_curve25519xsalsa20poly1305_beforenm(DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey, DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint, DNSCurveParameter.DNSCurveKey.Client_SecretKey);
			}
		}
		else {
			delete[] DNSCurveParameter.DNSCurveTarget.IPv4_ProviderName;
			delete[] DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey;
			delete[] DNSCurveParameter.DNSCurveKey.IPv4_ServerPublicKey;
			delete[] DNSCurveParameter.DNSCurveMagicNumber.IPv4_ReceiveMagicNumber;
			delete[] DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber;

			DNSCurveParameter.DNSCurveTarget.IPv4_ProviderName = nullptr;
			DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey = nullptr;
			DNSCurveParameter.DNSCurveKey.IPv4_ServerPublicKey = nullptr;
			DNSCurveParameter.DNSCurveMagicNumber.IPv4_ReceiveMagicNumber = nullptr;
			DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber = nullptr;
		}

	//Alternate(IPv6)
		if (DNSCurveParameter.Encryption && DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_family != NULL)
		{
		//Empty Server Fingerprint
			if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint, crypto_box_PUBLICKEYBYTES))
			{
			//Encryption Only mode check
				if (DNSCurveParameter.EncryptionOnly && 
					CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve Encryption Only mode error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}

			//Empty Provider Name
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6_ProviderName, DOMAIN_MAXSIZE))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve empty Provider Name error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}

			//Empty Public Key
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerPublicKey, crypto_box_PUBLICKEYBYTES))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve empty Public Key error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}
			}
			else {
				crypto_box_curve25519xsalsa20poly1305_beforenm(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey, DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint, DNSCurveParameter.DNSCurveKey.Client_SecretKey);
			}
		}
		else {
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6_ProviderName;
			delete[] DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey;
			delete[] DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerPublicKey;
			delete[] DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber;
			delete[] DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber;

			DNSCurveParameter.DNSCurveTarget.Alternate_IPv6_ProviderName = nullptr;
			DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey = nullptr;
			DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerPublicKey = nullptr;
			DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber = nullptr;
			DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber = nullptr;
		}

	//Alternate(IPv4)
		if (DNSCurveParameter.Encryption && DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_family != NULL)
		{
		//Empty Server Fingerprint
			if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint, crypto_box_PUBLICKEYBYTES))
			{
			//Encryption Only mode check
				if (DNSCurveParameter.EncryptionOnly && 
					CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve Encryption Only mode error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}

			//Empty Provider Name
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4_ProviderName, DOMAIN_MAXSIZE))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve empty Provider Name error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}

			//Empty Public Key
				if (CheckEmptyBuffer(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerPublicKey, crypto_box_PUBLICKEYBYTES))
				{
					::PrintError(PARAMETER_ERROR, L"DNSCurve empty Public Key error", NULL, nullptr, NULL);
					return EXIT_FAILURE;
				}
			}
			else {
				crypto_box_curve25519xsalsa20poly1305_beforenm(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey, DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint, DNSCurveParameter.DNSCurveKey.Client_SecretKey);
			}
		}
		else {
			delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4_ProviderName;
			delete[] DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey;
			delete[] DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerPublicKey;
			delete[] DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber;
			delete[] DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber;

			DNSCurveParameter.DNSCurveTarget.Alternate_IPv4_ProviderName = nullptr;
			DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey = nullptr;
			DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerPublicKey = nullptr;
			DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber = nullptr;
			DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber = nullptr;
		}
	}
	else {
		delete[] DNSCurveParameter.DNSCurveTarget.IPv4_ProviderName;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv4_ProviderName;
		delete[] DNSCurveParameter.DNSCurveTarget.IPv6_ProviderName;
		delete[] DNSCurveParameter.DNSCurveTarget.Alternate_IPv6_ProviderName;
	//DNSCurve Keys
		delete[] DNSCurveParameter.DNSCurveKey.Client_PublicKey;
		delete[] DNSCurveParameter.DNSCurveKey.Client_SecretKey;
		delete[] DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey;
		delete[] DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey;
		delete[] DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey;
		delete[] DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey;
		delete[] DNSCurveParameter.DNSCurveKey.IPv4_ServerPublicKey;
		delete[] DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerPublicKey;
		delete[] DNSCurveParameter.DNSCurveKey.IPv6_ServerPublicKey;
		delete[] DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerPublicKey;
		delete[] DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint;
		delete[] DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint;
		delete[] DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint;
		delete[] DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint;
	//DNSCurve Magic Numbers
		delete[] DNSCurveParameter.DNSCurveMagicNumber.IPv4_ReceiveMagicNumber;
		delete[] DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber;
		delete[] DNSCurveParameter.DNSCurveMagicNumber.IPv6_ReceiveMagicNumber;
		delete[] DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber;
		delete[] DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber;
		delete[] DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber;
		delete[] DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber;
		delete[] DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber;

		DNSCurveParameter.DNSCurveTarget.IPv4_ProviderName = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv4_ProviderName = nullptr, DNSCurveParameter.DNSCurveTarget.IPv6_ProviderName = nullptr, DNSCurveParameter.DNSCurveTarget.Alternate_IPv6_ProviderName = nullptr;
		DNSCurveParameter.DNSCurveKey.Client_PublicKey = nullptr, DNSCurveParameter.DNSCurveKey.Client_SecretKey = nullptr;
		DNSCurveParameter.DNSCurveKey.IPv4_PrecomputationKey = nullptr, DNSCurveParameter.DNSCurveKey.Alternate_IPv4_PrecomputationKey = nullptr, DNSCurveParameter.DNSCurveKey.IPv6_PrecomputationKey = nullptr, DNSCurveParameter.DNSCurveKey.Alternate_IPv6_PrecomputationKey = nullptr;
		DNSCurveParameter.DNSCurveKey.IPv4_ServerPublicKey = nullptr, DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerPublicKey = nullptr, DNSCurveParameter.DNSCurveKey.IPv6_ServerPublicKey = nullptr, DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerPublicKey = nullptr;
		DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint = nullptr, DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint = nullptr, DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint = nullptr, DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint = nullptr;
		DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber = nullptr, DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber = nullptr, DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber = nullptr, DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber = nullptr;
		DNSCurveParameter.DNSCurveMagicNumber.IPv4_ReceiveMagicNumber = nullptr, DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber = nullptr, DNSCurveParameter.DNSCurveMagicNumber.IPv6_ReceiveMagicNumber = nullptr, DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber = nullptr;
	}

//Default settings
	if (this->ListenPort == 0)
		this->ListenPort = htons(DNS_PORT);
	if (!this->EDNS0Label)
	{
		if (this->DNSSECRequest)
		{
			::PrintError(PARAMETER_ERROR, L"EDNS0 Label must trun ON when request DNSSEC", NULL, nullptr, NULL);
			this->EDNS0Label = true;
		}

		if (this->DNSCurve)
		{
			::PrintError(PARAMETER_ERROR, L"EDNS0 Label must trun ON when request DNSCurve", NULL, nullptr, NULL);
			this->EDNS0Label = true;
		}
	}

	if (ICMPOptions.ICMPSpeed > 0)
	{
		if (ntohs(this->ICMPOptions.ICMPID) <= 0)
			this->ICMPOptions.ICMPID = htons((uint16_t)GetCurrentProcessId()); //Default DNS ID is current process ID.
		if (ntohs(this->ICMPOptions.ICMPSequence) <= 0)
			this->ICMPOptions.ICMPSequence = htons(0x0001); //Default DNS Sequence is 0x0001.
	}

	if (ntohs(this->DomainTestOptions.DomainTestID) <= 0)
		this->DomainTestOptions.DomainTestID = htons(0x0001); //Default Domain Test DNS ID is 0x0001.
	if (this->DomainTestOptions.DomainTestSpeed <= 5000U) //5s is least time between Domain Tests.
		this->DomainTestOptions.DomainTestSpeed = 900000U; //Default Domain Test request every 15 minutes.

	if (this->PaddingDataOptions.PaddingDataLength <= 0)
	{
		this->PaddingDataOptions.PaddingDataLength = strlen(LOCAL_PADDINGDATA) + 1U;
		memcpy(this->PaddingDataOptions.PaddingData, LOCAL_PADDINGDATA, this->PaddingDataOptions.PaddingDataLength - 1U); //Load default padding data from Microsoft Windows Ping.
	}

	if (this->LocalhostServerOptions.LocalhostServerLength <= 0)
		this->LocalhostServerOptions.LocalhostServerLength = CharToDNSQuery(LOCAL_SERVERNAME, this->LocalhostServerOptions.LocalhostServer); //Default Localhost DNS server name
	this->HostsDefaultTTL = DEFAULT_HOSTS_TTL;

	if (this->DNSCurve && DNSCurveParameter.Encryption) //DNSCurve default settings
	{
	//DNSCurve PayloadSize check
		if (DNSCurveParameter.DNSCurvePayloadSize < OLD_DNS_MAXSIZE)
		{
			if (DNSCurveParameter.DNSCurvePayloadSize > 0)
				::PrintError(PARAMETER_ERROR, L"DNSCurve PayloadSize must longer than 512 bytes(Old DNS packets minimum supported size)", NULL, nullptr, NULL);
			DNSCurveParameter.DNSCurvePayloadSize = OLD_DNS_MAXSIZE; //Default DNSCurve UDP maximum payload size.
		}
		else if (DNSCurveParameter.DNSCurvePayloadSize >= PACKET_MAXSIZE - sizeof(ipv6_hdr) - sizeof(udp_hdr))
		{
			::PrintError(PARAMETER_ERROR, L"DNSCurve PayloadSize may be too long", NULL, nullptr, NULL);
			DNSCurveParameter.DNSCurvePayloadSize = EDNS0_MINSIZE;
		}

	//Main(IPv6)
		if (DNSCurveParameter.DNSCurveTarget.IPv6.sin6_family != NULL && CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv6_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.IPv6_ReceiveMagicNumber, DNSCRYPT_RECEIVE_MAGIC, DNSCURVE_MAGIC_QUERY_LEN);

	//Main(IPv4)
		if (DNSCurveParameter.DNSCurveTarget.IPv4.sin_family != NULL && CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.IPv4_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.IPv4_ReceiveMagicNumber, DNSCRYPT_RECEIVE_MAGIC, DNSCURVE_MAGIC_QUERY_LEN);

	//Alternate(IPv6)
		if (DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_family != NULL && CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber, DNSCRYPT_RECEIVE_MAGIC, DNSCURVE_MAGIC_QUERY_LEN);

	//Alternate(IPv4)
		if (DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_family != NULL && CheckEmptyBuffer(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber, DNSCURVE_MAGIC_QUERY_LEN))
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber, DNSCRYPT_RECEIVE_MAGIC, DNSCURVE_MAGIC_QUERY_LEN);

	//DNSCurve key(s) recheck time
		if (DNSCurveParameter.KeyRecheckTime == 0)
			DNSCurveParameter.KeyRecheckTime = DEFAULT_DNSCURVE_RECHECK_TIME * TIME_OUT;
	}

	return EXIT_SUCCESS;
}

//Read parameter data from configuration file(s)
size_t Configuration::ReadParameterData(const PSTR Buffer, const PWSTR FileName, const size_t Line, bool &Comments)
{
	std::string Data(Buffer);
//Multi-line comments check
	if (Comments)
	{
		if (Data.find("*/") != std::string::npos)
		{
			Data = Buffer + Data.find("*/") + 2U;
			Comments = false;
		}
		else {
			return FALSE;
		}
	}
	while (Data.find("/*") != std::string::npos)
	{
		if (Data.find("*/") == std::string::npos)
		{
			Data.erase(Data.find("/*"), Data.length() - Data.find("/*"));
			Comments = true;
			break;
		}
		else {
			Data.erase(Data.find("/*"), Data.find("*/") - Data.find("/*") + 2U);
		}
	}

	SSIZE_T Result = 0;
//Parameter version <= 0.3 compatible support.
	if (this->Version < INI_VERSION && Data.find("Hop Limits/TTL Fluctuation = ") == 0)
	{
		if (Data.length() == strlen("Hop Limits/TTL Fluctuation = "))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("Hop Limits/TTL Fluctuation = ") + 4U)
		{
			Result = atol(Data.c_str() + strlen("Hop Limits/TTL Fluctuation = "));
			if (Result > 0 && Result < U8_MAXNUM)
				this->HopLimitOptions.HopLimitFluctuation = (uint8_t)Result;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}

//Delete delete spaces, horizontal tab/HT, check comments(Number Sign/NS and double slashs) and check minimum length of ipfilter items.
//Delete comments(Number Sign/NS and double slashs) and check minimum length of configuration items.
	if (Data.find(35) == 0 || Data.find(47) == 0)
		return EXIT_SUCCESS;
	while (Data.find(9) != std::string::npos)
		Data.erase(Data.find(9), 1U);
	while (Data.find(32) != std::string::npos)
		Data.erase(Data.find(32), 1U);
	if (Data.find(35) != std::string::npos)
		Data.erase(Data.find(35));
	else if (Data.find(47) != std::string::npos)
		Data.erase(Data.find(47));
	if (Data.length() < 8U)
		return FALSE;

	size_t Symbol = 0;
//inet_ntop() and inet_pton() was only support in Windows Vista and newer system. [Roy Tam]
#ifdef _WIN64
#else //x86
	sockaddr_storage SockAddr = {0};
	int SockLength = 0;
#endif

//[Base] block
	if (Data.find("Version=") == 0)
	{
		if (Data.length() > strlen("Version=") && Data.length() < strlen("Version=") + 8U)
		{
			this->Version = atof(Data.c_str() + strlen("Version="));
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
//Parameter version <= 0.3 compatible support.
	else if (this->Version < INI_VERSION)
	{
	//[Base] block
		if (Data.find("Hosts=") == 0)
		{
			if (Data.length() == strlen("Hosts="))
			{
				return EXIT_SUCCESS;
			}
			else if (Data.length() < strlen("Hosts=") + 6U)
			{
				Result = atol(Data.c_str() + strlen("Hosts="));
				if (Result >= DEFAULT_FILEREFRESH_TIME)
					this->FileRefreshTime = Result * TIME_OUT;
				else if (Result > 0 && Result < DEFAULT_FILEREFRESH_TIME)
					this->FileRefreshTime = DEFAULT_FILEREFRESH_TIME * TIME_OUT;
//				else 
//					this->FileRefreshTime = 0; //Read file again OFF.
			}
			else {
				::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else if (Data.find("IPv4DNSAddress=") == 0)
		{
			if (Data.length() == strlen("IPv4DNSAddress="))
			{
				return EXIT_SUCCESS;
			}
			else if (Data.length() > strlen("IPv4DNSAddress=") + 6U && Data.length() < strlen("IPv4DNSAddress=") + 20U)
			{
				if (Data.find(46) == std::string::npos) //IPv4 Address(".")
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address or Port format error", NULL, FileName, Line);
					return EXIT_FAILURE;
				}

			//IPv4 Address and Port check
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				memcpy(Target.get(), Data.c_str() + strlen("IPv4DNSAddress="), Data.length() - strlen("IPv4DNSAddress="));
				this->DNSTarget.IPv4.sin_port = htons(DNS_PORT);
			//inet_ntop() and inet_pton() was only support in Windows Vista and newer system. [Roy Tam]
			#ifdef _WIN64
				Result = inet_pton(AF_INET, Target.get(), &this->DNSTarget.IPv4.sin_addr);
				if (Result == FALSE)
			#else //x86
				memset(&SockAddr, 0, sizeof(sockaddr_storage));
				SockLength = sizeof(sockaddr_in);
				if (WSAStringToAddressA(Target.get(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
			#endif
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
					return EXIT_FAILURE;
				}
			#ifdef _WIN64
				else if (Result == RETURN_ERROR)
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
					return EXIT_FAILURE;
				}
			#else //x86
				this->DNSTarget.IPv4.sin_addr = ((PSOCKADDR_IN)&SockAddr)->sin_addr;
			#endif
				this->DNSTarget.IPv4.sin_family = AF_INET;
			}
			else {
				::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else if (Data.find("IPv4LocalDNSAddress=") == 0)
		{
			if (Data.length() == strlen("IPv4LocalDNSAddress="))
			{
				return EXIT_SUCCESS;
			}
			else if (Data.length() > strlen("IPv4LocalDNSAddress=") + 6U && Data.length() < strlen("IPv4LocalDNSAddress=") + 20U)
			{
				if (Data.find(46) == std::string::npos) //IPv4 Address(".")
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address or Port format error", NULL, FileName, Line);
					return EXIT_FAILURE;
				}

			//IPv4 Address and Port check
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				memcpy(Target.get(), Data.c_str() + strlen("IPv4LocalDNSAddress="), Data.length() - strlen("IPv4LocalDNSAddress="));
				this->DNSTarget.Local_IPv4.sin_port = htons(DNS_PORT);
			//inet_ntop() and inet_pton() was only support in Windows Vista and newer system. [Roy Tam]
			#ifdef _WIN64
				Result = inet_pton(AF_INET, Target.get(), &this->DNSTarget.Local_IPv4.sin_addr);
				if (Result == FALSE)
			#else //x86
				memset(&SockAddr, 0, sizeof(sockaddr_storage));
				SockLength = sizeof(sockaddr_in);
				if (WSAStringToAddressA(Target.get(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
			#endif
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
					return EXIT_FAILURE;
				}
			#ifdef _WIN64
				else if (Result == RETURN_ERROR)
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
					return EXIT_FAILURE;
				}
			#else //x86
				this->DNSTarget.Local_IPv4.sin_addr = ((PSOCKADDR_IN)&SockAddr)->sin_addr;
			#endif
				this->DNSTarget.Local_IPv4.sin_family = AF_INET;
			}
		}
		else if (Data.find("IPv6DNSAddress=") == 0)
		{
			if (Data.length() == strlen("IPv6DNSAddress="))
			{
				return EXIT_SUCCESS;
			}
			else if (Data.length() > strlen("IPv6DNSAddress=") + 1U && Data.length() < strlen("IPv6DNSAddress=") + 40U)
			{
				if (Data.find(58) == std::string::npos) //IPv6 Address(":")
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address or Port format error", NULL, FileName, Line);
					return EXIT_FAILURE;
				}

			//IPv6 Address check
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				memcpy(Target.get(), Data.c_str() + strlen("IPv6DNSAddress="), Data.length() - strlen("IPv6DNSAddress="));
				this->DNSTarget.IPv6.sin6_port = htons(DNS_PORT);
			//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
			#ifdef _WIN64
				Result = inet_pton(AF_INET6, Target.get(), &this->DNSTarget.IPv6.sin6_addr);
				if (Result == FALSE)
			#else //x86
				memset(&SockAddr, 0, sizeof(sockaddr_storage));
				SockLength = sizeof(sockaddr_in6);
				if (WSAStringToAddressA(Target.get(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
			#endif
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", NULL, FileName, Line);
					return EXIT_FAILURE;
				}
			#ifdef _WIN64
				else if (Result == RETURN_ERROR)
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", WSAGetLastError(), FileName, Line);
					return EXIT_FAILURE;
				}
			#else //x86
				this->DNSTarget.IPv6.sin6_addr = ((PSOCKADDR_IN6)&SockAddr)->sin6_addr;
			#endif
				this->DNSTarget.IPv6.sin6_family = AF_INET6;
			}
			else {
				::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else if (Data.find("IPv6LocalDNSAddress=") == 0)
		{
			if (Data.length() == strlen("IPv6LocalDNSAddress="))
			{
				return EXIT_SUCCESS;
			}
			else if (Data.length() > strlen("IPv6LocalDNSAddress=") + 1U && Data.length() < strlen("IPv6LocalDNSAddress=") + 40U)
			{
				if (Data.find(58) == std::string::npos) //IPv6 Address(":")
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address or Port format error", NULL, FileName, Line);
					return EXIT_FAILURE;
				}

			//IPv6 Address check
				std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
				memcpy(Target.get(), Data.c_str() + strlen("IPv6LocalDNSAddress="), Data.length() - strlen("IPv6LocalDNSAddress="));
				this->DNSTarget.Local_IPv6.sin6_port = htons(DNS_PORT);
			//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
			#ifdef _WIN64
				Result = inet_pton(AF_INET6, Target.get(), &this->DNSTarget.Local_IPv6.sin6_addr);
				if (Result == FALSE)
			#else //x86
				memset(&SockAddr, 0, sizeof(sockaddr_storage));
				SockLength = sizeof(sockaddr_in6);
				if (WSAStringToAddressA(Target.get(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
			#endif
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", NULL, FileName, Line);
					return EXIT_FAILURE;
				}
			#ifdef _WIN64
				else if (Result == RETURN_ERROR)
				{
					::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", WSAGetLastError(), FileName, Line);
					return EXIT_FAILURE;
				}
			#else //x86
				this->DNSTarget.Local_IPv6.sin6_addr = ((PSOCKADDR_IN6)&SockAddr)->sin6_addr;
			#endif
				this->DNSTarget.Local_IPv6.sin6_family = AF_INET6;
			}
			else {
				::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}

	//[Extend Test] block
		else if (Data.find("IPv4OptionsFilter=1") == 0)
		{
			this->IPv4DataCheck = true;
		}
		else if (Data.find("TCPOptionsFilter=1") == 0)
		{
			this->TCPDataCheck = true;
		}
		else if (Data.find("DNSOptionsFilter=1") == 0)
		{
			this->DNSDataCheck = true;
		}

	//[Data] block
		else if (Data.find("DomainTestSpeed=") == 0)
		{
			if (Data.length() == strlen("DomainTestSpeed="))
			{
				return EXIT_SUCCESS;
			}
			else if (Data.length() < strlen("DomainTestSpeed=") + 6U)
			{
				Result = atol(Data.c_str() + strlen("DomainTestSpeed="));
				if (Result > 0)
					this->DomainTestOptions.DomainTestSpeed = Result * TIME_OUT;
			}
			else {
				::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
	}
//	else if (Data.find("PrintError=") == 0)
	else if (Data.find("PrintError=0") == 0)
	{
		this->PrintError = false;
	}
	else if (Data.find("FileRefreshTime=") == 0)
	{
		if (Data.length() == strlen("FileRefreshTime="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("FileRefreshTime=") + 6U)
		{
			Result = atol(Data.c_str() + strlen("FileRefreshTime="));
			if (Result >= DEFAULT_FILEREFRESH_TIME)
				this->FileRefreshTime = Result * TIME_OUT;
			else if (Result > 0 && Result < DEFAULT_FILEREFRESH_TIME)
				this->FileRefreshTime = DEFAULT_FILEREFRESH_TIME * TIME_OUT;
//			else 
//				this->FileRefreshTime = 0; //Read file again OFF.
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("FileHash=1") == 0)
	{
		this->FileHash = true;
	}

//[DNS] block
//	else if (Data.find("Protocol=") == 0)
	else if (Data.find("Protocol=TCP") == 0 || Data.find("Protocol=Tcp") == 0 || Data.find("Protocol=tcp") == 0)
	{
//		if (Data.find("Protocol=TCP") == 0 || Data.find("Protocol=Tcp") == 0 || Data.find("Protocol=tcp") == 0)
			this->RquestMode = REQUEST_TCPMODE;
//		else 
//			this->RquestMode = REQUEST_UDPMODE;
	}
	else if (Data.find("HostsOnly=1") == 0)
	{
		this->HostsOnly = true;
	}
	else if (Data.find("CacheType=") == 0 && Data.length() > strlen(("CacheType=")))
	{
		if (Data.find("CacheType=Timer") == 0)
			this->CacheType = CACHE_TIMER;
		else if (Data.find("CacheType=Queue") == 0)
			this->CacheType = CACHE_QUEUE;
	}
	else if (this->CacheType != 0 && Data.find("CacheParameter=") == 0 && Data.length() > strlen("CacheParameter="))
	{
		Result = atol(Data.c_str() + strlen("CacheParameter="));
		if (Result > 0)
		{
			if (this->CacheType == CACHE_TIMER)
				this->CacheParameter = Result * TIME_OUT;
			else //CACHE_QUEUE
				this->CacheParameter = Result;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}

//[Listen] block
	else if (Data.find("OperationMode=") == 0)
	{
		if (Data.find("OperationMode=Private") == 0 || Data.find("OperationMode=private") == 0)
			this->OperationMode = LISTEN_PRIVATEMODE;
		else if (Data.find("OperationMode=Server") == 0 || Data.find("OperationMode=server") == 0)
			this->OperationMode = LISTEN_SERVERMODE;
		else if (Data.find("Operation Mode=Custom") == 0 || Data.find("OperationMode=custom") == 0)
			this->OperationMode = LISTEN_CUSTOMMODE;
//		else 
//			this->OperationMode = LISTEN_PROXYMODE;
	}
	else if (Data.find("ListenPort=") == 0)
	{
		if (Data.length() == strlen("ListenPort="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("ListenPort=") + 6U)
		{
			Result = atol(Data.c_str() + strlen("ListenPort="));
			if (Result > 0 && Result <= U16_MAXNUM)
			{
				this->ListenPort = htons((uint16_t)Result);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"Localhost server listening Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
//	else if (Data.find("IPFilterType=") == 0)
	else if (Data.find("IPFilterType=Permit") == 0 || Data.find("IPFilterType=permit") == 0)
	{
		this->IPFilterOptions.Type = true;
	}
	else if (Data.find("IPFilterLevel<") == 0)
	{
		if (Data.length() == strlen("IPFilterLevel<"))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("IPFilterLevel<") + 4U)
		{
			Result = atol(Data.c_str() + strlen("IPFilterLevel<"));
			if (Result > 0 && Result <= U16_MAXNUM)
			{
				this->IPFilterOptions.IPFilterLevel = (size_t)Result;
			}
			else {
				::PrintError(PARAMETER_ERROR, L"IPFilter Level error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}

//[Addresses] block
	else if (Data.find("IPv4DNSAddress=") == 0)
	{
		if (Data.length() == strlen("IPv4DNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv4DNSAddress=") + 8U && Data.length() < strlen("IPv4DNSAddress=") + 22U)
		{
			Symbol = Data.find(58);
			if (Data.find(46) == std::string::npos || Symbol == std::string::npos) //IPv4 Address(".") and Port(":")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv4 Address and Port check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv4DNSAddress="), Symbol - strlen("IPv4DNSAddress="));
			Result = atol(Data.c_str() + Symbol + 1U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				this->DNSTarget.IPv4.sin_port = htons((uint16_t)Result);
			}
		//inet_ntop() and inet_pton() was only support in Windows Vista and newer system. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET, Target.get(), &this->DNSTarget.IPv4.sin_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in);
			if (WSAStringToAddressA(Target.get(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			this->DNSTarget.IPv4.sin_addr = ((PSOCKADDR_IN)&SockAddr)->sin_addr;
		#endif
			this->DNSTarget.IPv4.sin_family = AF_INET;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateDNSAddress=") == 0)
	{
		if (Data.length() == strlen("IPv4AlternateDNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv4AlternateDNSAddress=") + 8U && Data.length() < strlen("IPv4AlternateDNSAddress=") + 22U)
		{
			Symbol = Data.find(58);
			if (Data.find(46) == std::string::npos || Symbol == std::string::npos) //IPv4 Address(".") and Port(":")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv4 Address and Port check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv4AlternateDNSAddress="), Symbol - strlen("IPv4AlternateDNSAddress="));
			Result = atol(Data.c_str() + Symbol + 1U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				this->DNSTarget.Alternate_IPv4.sin_port = htons((uint16_t)Result);
			}
		//inet_ntop() and inet_pton() was only support in Windows Vista and newer system. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET, Target.get(), &this->DNSTarget.Alternate_IPv4.sin_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in);
			if (WSAStringToAddressA(Target.get(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			this->DNSTarget.Alternate_IPv4.sin_addr = ((PSOCKADDR_IN)&SockAddr)->sin_addr;
		#endif
			this->DNSTarget.Alternate_IPv4.sin_family = AF_INET;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4LocalDNSAddress=") == 0)
	{
		if (Data.length() == strlen("IPv4LocalDNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv4LocalDNSAddress=") + 8U && Data.length() < strlen("IPv4LocalDNSAddress=") + 22U)
		{
			Symbol = Data.find(58);
			if (Data.find(46) == std::string::npos || Symbol == std::string::npos) //IPv4 Address(".") and Port(":")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv4 Address and Port check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv4LocalDNSAddress="), Symbol - strlen("IPv4LocalDNSAddress="));
			Result = atol(Data.c_str() + Symbol + 1U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				this->DNSTarget.Local_IPv4.sin_port = htons((uint16_t)Result);
			}
		//inet_ntop() and inet_pton() was only support in Windows Vista and newer system. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET, Target.get(), &this->DNSTarget.Local_IPv4.sin_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in);
			if (WSAStringToAddressA(Target.get(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			this->DNSTarget.Local_IPv4.sin_addr = ((PSOCKADDR_IN)&SockAddr)->sin_addr;
		#endif
			this->DNSTarget.Local_IPv4.sin_family = AF_INET;
		}
	}
	else if (Data.find("IPv4LocalAlternateDNSAddress=") == 0)
	{
		if (Data.length() == strlen("IPv4LocalAlternateDNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv4LocalAlternateDNSAddress=") + 8U && Data.length() < strlen("IPv4LocalAlternateDNSAddress=") + 22U)
		{
			Symbol = Data.find(58);
			if (Data.find(46) == std::string::npos || Symbol == std::string::npos) //IPv4 Address(".") and Port(":")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv4 Address and Port check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv4LocalAlternateDNSAddress="), Symbol - strlen("IPv4LocalAlternateDNSAddress="));
			Result = atol(Data.c_str() + Symbol + 1U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				this->DNSTarget.Alternate_Local_IPv4.sin_port = htons((uint16_t)Result);
			}
		//inet_ntop() and inet_pton() was only support in Windows Vista and newer system. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET, Target.get(), &this->DNSTarget.Alternate_Local_IPv4.sin_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in);
			if (WSAStringToAddressA(Target.get(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			this->DNSTarget.Alternate_Local_IPv4.sin_addr = ((PSOCKADDR_IN)&SockAddr)->sin_addr;
		#endif
			this->DNSTarget.Alternate_Local_IPv4.sin_family = AF_INET;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6DNSAddress=") == 0)
	{
		if (Data.length() == strlen("IPv6DNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv6DNSAddress=") + 6U && Data.length() < strlen("IPv6DNSAddress=") + 48U)
		{
			Symbol = Data.find(93);
			if (Data.find(58) == std::string::npos || Data.find(91) == std::string::npos || Symbol == std::string::npos) //IPv6 Address(":", "[" and "]") and Port("]:")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv6 Address check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv6DNSAddress=") + 1U, Symbol - strlen("IPv6DNSAddress=") - 1U);
			Result = atol(Data.c_str() + Symbol + 2U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				this->DNSTarget.IPv6.sin6_port = htons((uint16_t)Result);
			}
		//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET6, Target.get(), &this->DNSTarget.IPv6.sin6_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in6);
			if (WSAStringToAddressA(Target.get(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			this->DNSTarget.IPv6.sin6_addr = ((PSOCKADDR_IN6)&SockAddr)->sin6_addr;
		#endif
			this->DNSTarget.IPv6.sin6_family = AF_INET6;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateDNSAddress=") == 0)
	{
		if (Data.length() == strlen("IPv6AlternateDNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv6AlternateDNSAddress=") + 6U && Data.length() < strlen("IPv6AlternateDNSAddress=") + 48U)
		{
			Symbol = Data.find(93);
			if (Data.find(58) == std::string::npos || Data.find(91) == std::string::npos || Symbol == std::string::npos) //IPv6 Address(":", "[" and "]") and Port("]:")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv6 Address check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv6AlternateDNSAddress=") + 1U, Symbol - strlen("IPv6AlternateDNSAddress=") - 1U);
			Result = atol(Data.c_str() + Symbol + 2U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				this->DNSTarget.Alternate_IPv6.sin6_port = htons((uint16_t)Result);
			}
		//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET6, Target.get(), &this->DNSTarget.Alternate_IPv6.sin6_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in6);
			if (WSAStringToAddressA(Target.get(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			this->DNSTarget.Alternate_IPv6.sin6_addr = ((PSOCKADDR_IN6)&SockAddr)->sin6_addr;
		#endif
			this->DNSTarget.Alternate_IPv6.sin6_family = AF_INET6;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6LocalDNSAddress=") == 0)
	{
		if (Data.length() == strlen("IPv6LocalDNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv6LocalDNSAddress=") + 6U && Data.length() < strlen("IPv6LocalDNSAddress=") + 48U)
		{
			Symbol = Data.find(93);
			if (Data.find(58) == std::string::npos || Data.find(91) == std::string::npos || Symbol == std::string::npos) //IPv6 Address(":", "[" and "]") and Port("]:")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv6 Address check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv6LocalDNSAddress=") + 1U, Symbol - strlen("IPv6LocalDNSAddress=") - 1U);
			Result = atol(Data.c_str() + Symbol + 2U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				this->DNSTarget.Local_IPv6.sin6_port = htons((uint16_t)Result);
			}
		//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET6, Target.get(), &this->DNSTarget.Local_IPv6.sin6_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in6);
			if (WSAStringToAddressA(Target.get(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			this->DNSTarget.Local_IPv6.sin6_addr = ((PSOCKADDR_IN6)&SockAddr)->sin6_addr;
		#endif
			this->DNSTarget.Local_IPv6.sin6_family = AF_INET6;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6LocalAlternateDNSAddress=") == 0)
	{
		if (Data.length() == strlen("IPv6LocalAlternateDNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv6LocalAlternateDNSAddress=") + 6U && Data.length() < strlen("IPv6LocalAlternateDNSAddress=") + 48U)
		{
			Symbol = Data.find(93);
			if (Data.find(58) == std::string::npos || Data.find(91) == std::string::npos || Symbol == std::string::npos) //IPv6 Address(":", "[" and "]") and Port("]:")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv6 Address check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("IPv6LocalAlternateDNSAddress=") + 1U, Symbol - strlen("IPv6LocalAlternateDNSAddress=") - 1U);
			Result = atol(Data.c_str() + Symbol + 2U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				this->DNSTarget.Alternate_Local_IPv6.sin6_port = htons((uint16_t)Result);
			}
		//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET6, Target.get(), &this->DNSTarget.Alternate_Local_IPv6.sin6_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in6);
			if (WSAStringToAddressA(Target.get(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			this->DNSTarget.Alternate_Local_IPv6.sin6_addr = ((PSOCKADDR_IN6)&SockAddr)->sin6_addr;
		#endif
			this->DNSTarget.Alternate_Local_IPv6.sin6_family = AF_INET6;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}

//[Values] block
	else if (Data.find("EDNS0PayloadSize=") == 0)
	{
		if (Data.length() == strlen("EDNS0PayloadSize="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("EDNS0PayloadSize=") + 6U)
		{
			Result = atol(Data.c_str() + strlen("EDNS0PayloadSize="));
			if (Result >= 0)
				this->EDNS0PayloadSize = Result;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4TTL=") == 0)
	{
		if (Data.length() == strlen("IPv4TTL="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("IPv4TTL=") + 4U)
		{
			Result = atol(Data.c_str() + strlen("IPv4TTL="));
			if (Result > 0 && Result < U8_MAXNUM)
				this->HopLimitOptions.IPv4TTL = (uint8_t)Result;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6HopLimits=") == 0)
	{
		if (Data.length() == strlen("IPv6HopLimits="))
		{
			return EXIT_SUCCESS;
		}
		if (Data.length() < strlen("IPv6HopLimits=") + 4U)
		{
			Result = atol(Data.c_str() + strlen("IPv6HopLimits="));
			if (Result > 0 && Result < U8_MAXNUM)
				this->HopLimitOptions.IPv6HopLimit = (uint8_t)Result;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateTTL=") == 0)
	{
		if (Data.length() == strlen("IPv4AlternateTTL="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("IPv4AlternateTTL=") + 4U)
		{
			Result = atol(Data.c_str() + strlen("IPv4AlternateTTL="));
			if (Result > 0 && Result < U8_MAXNUM)
				this->HopLimitOptions.Alternate_IPv4TTL = (uint8_t)Result;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateHopLimits=") == 0)
	{
		if (Data.length() == strlen("IPv6AlternateHopLimits="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("IPv6AlternateHopLimits=") + 4U)
		{
			Result = atol(Data.c_str() + strlen("IPv6AlternateHopLimits="));
			if (Result > 0 && Result < U8_MAXNUM)
				this->HopLimitOptions.Alternate_IPv6HopLimit = (uint8_t)Result;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("HopLimitsFluctuation=") == 0)
	{
		if (Data.length() == strlen("HopLimitsFluctuation="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("HopLimitsFluctuation=") + 4U)
		{
			Result = atol(Data.c_str() + strlen("HopLimitsFluctuation="));
			if (Result > 0 && Result < U8_MAXNUM)
				this->HopLimitOptions.HopLimitFluctuation = (uint8_t)Result;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("AlternateTimes=") == 0)
	{
		if (Data.length() == strlen("AlternateTimes="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("AlternateTimes=") + 6U)
		{
			Result = atol(Data.c_str() + strlen("AlternateTimes="));
			if (Result >= DEFAULT_ALTERNATE_TIMES)
				this->AlternateOptions.AlternateTimes = Result;
			else 
				this->AlternateOptions.AlternateTimes = DEFAULT_ALTERNATE_TIMES;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("AlternateTimeRange=") == 0)
	{
		if (Data.length() == strlen("AlternateTimeRange="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("AlternateTimeRange=") + 6U)
		{
			Result = atol(Data.c_str() + strlen("AlternateTimeRange="));
			if (Result >= DEFAULT_ALTERNATE_RANGE)
				this->AlternateOptions.AlternateTimeRange = Result * TIME_OUT;
			else 
				this->AlternateOptions.AlternateTimeRange = DEFAULT_ALTERNATE_RANGE * TIME_OUT;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("AlternateResetTime=") == 0)
	{
		if (Data.length() == strlen("AlternateResetTime="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("AlternateResetTime=") + 6U)
		{
			Result = atol(Data.c_str() + strlen("AlternateResetTime="));
			if (Result >= DEFAULT_ALTERNATERESET_TIME)
				this->AlternateOptions.AlternateResetTime = Result * TIME_OUT;
			else 
				this->AlternateOptions.AlternateResetTime = DEFAULT_ALTERNATERESET_TIME * TIME_OUT;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("ICMPTest=") == 0)
	{
		if (Data.length() == strlen("ICMPTest="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("ICMPTest=") + 6U)
		{
			Result = atol(Data.c_str() + strlen("ICMPTest="));
			if (Result >= 5)
				this->ICMPOptions.ICMPSpeed = Result * TIME_OUT;
			else if (Result > 0 && Result < DEFAULT_ICMPTEST_TIME)
				this->ICMPOptions.ICMPSpeed = DEFAULT_ICMPTEST_TIME * TIME_OUT;
			else 
				this->ICMPOptions.ICMPSpeed = 0; //ICMP Test OFF.
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DomainTest=") == 0)
	{
		if (Data.length() == strlen("DomainTest="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("DomainTest=") + 6U)
		{
			Result = atol(Data.c_str() + strlen("DomainTest="));
			if (Result > 0)
				this->DomainTestOptions.DomainTestSpeed = Result * TIME_OUT;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);

			return EXIT_FAILURE;
		}
	}

//[Switches] block
//	else if (Data.find("EDNS0Label=") == 0)
	else if (Data.find("EDNS0Label=1") == 0)
	{
		this->EDNS0Label = true;
	}
//	else if (Data.find("DNSSECRequest=") == 0)
	else if (Data.find("DNSSECRequest=1") == 0)
	{
		this->DNSSECRequest = true;
	}
//	else if (Data.find("IPv4DataFilter=") == 0)
	else if (Data.find("IPv4DataFilter=1") == 0)
	{
		this->IPv4DataCheck = true;
	}
//	else if (Data.find("TCPDataFilter=") == 0)
	else if (Data.find("TCPDataFilter=1") == 0)
	{
		this->TCPDataCheck = true;
	}
//	else if (Data.find("DNSDataFilter=") == 0)
	else if (Data.find("DNSDataFilter=1") == 0)
	{
		this->DNSDataCheck = true;
	}
//	else if (Data.find("BlacklistFilter=") == 0)
	else if (Data.find("BlacklistFilter=1") == 0)
	{
		this->Blacklist = true;
	}

//[Data] block
	else if (Data.find("ICMPID=") == 0)
	{
		if (Data.length() == strlen("ICMPID="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("ICMPID=") + 7U)
		{
			Result = strtol(Data.c_str() + strlen("ICMPID="), NULL, 16);
			if (Result > 0)
				this->ICMPOptions.ICMPID = htons((uint16_t)Result);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("ICMPSequence=") == 0)
	{
		if (Data.length() == strlen("ICMPSequence="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("ICMPSequence=") + 7U)
		{
			Result = strtol(Data.c_str() + strlen("ICMPSequence="), NULL, 16);
			if (Result > 0)
				this->ICMPOptions.ICMPSequence = htons((uint16_t)Result);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DomainTestData=") == 0)
	{
		if (Data.length() == strlen("DomainTestData="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("DomainTestData=") + 2U && Data.length() < strlen("DomainTestData=") + 253U) //Maximum length of whole level domain is 253 bytes(Section 2.3.1 in RFC 1035).
		{
			memcpy(this->DomainTestOptions.DomainTest, Data.c_str() + strlen("DomainTestData="), Data.length() - strlen("DomainTestData="));
			this->DomainTestOptions.DomainTestCheck = true;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DomainTestID=") == 0)
	{
		if (Data.length() == strlen("DomainTestID="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("DomainTestID=") + 7U)
		{
			Result = strtol(Data.c_str() + strlen("DomainTestID="), NULL, 16);
			if (Result > 0)
				this->DomainTestOptions.DomainTestID = htons((uint16_t)Result);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("ICMPPaddingData=") == 0)
	{
		if (Data.length() == strlen("ICMPPaddingData="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("ICMPPaddingData=") + 17U && Data.length() < strlen("ICMPPaddingData=") + ICMP_PADDING_MAXSIZE - 1U) //Length of ICMP padding data must between 18 bytes and 1464 bytes(Ethernet MTU - IPv4 Standard Header - ICMP Header).
		{
			this->PaddingDataOptions.PaddingDataLength = Data.length() - strlen("ICMPPaddingData=") - 1U;
			memcpy(this->PaddingDataOptions.PaddingData, Data.c_str() + strlen("ICMPPaddingData="), Data.length() - strlen("ICMPPaddingData="));
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("LocalhostServerName=") == 0)
	{
		if (Data.length() == strlen("LocalhostServerName="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("LocalhostServerName=") + 2U && Data.length() < strlen("LocalhostServerName=") + 253U) //Maximum length of whole level domain is 253 bytes(Section 2.3.1 in RFC 1035).
		{
/* Old version
			std::shared_ptr<int> Point(new int[DOMAIN_MAXSIZE]());
			std::shared_ptr<char> LocalhostServer(new char[DOMAIN_MAXSIZE]());
			size_t Index = 0;
			this->LocalhostServerOptions.LocalhostServerLength = Data.length() - strlen("LocalhostServerName=");

		//Convert from char to DNS query.
			LocalhostServer.get()[0] = 46;
			memcpy(LocalhostServer.get() + 1U, Data.c_str() + strlen("LocalhostServerName="), this->LocalhostServerOptions.LocalhostServerLength);
			for (Index = 0;Index < Data.length() - strlen("LocalhostServerName=") - 1U;Index++)
			{
			//Preferred name syntax(Section 2.3.1 in RFC 1035)
				if (LocalhostServer.get()[Index] == 45 || LocalhostServer.get()[Index] == 46 || 
					LocalhostServer.get()[Index] > 47 && LocalhostServer.get()[Index] < 58 || 
					LocalhostServer.get()[Index] > 64 && LocalhostServer.get()[Index] < 91 || 
					LocalhostServer.get()[Index] > 96 && LocalhostServer.get()[Index] < 123)
				{
					if (LocalhostServer.get()[Index] == 46)
					{
						Point.get()[Result] = (int)Index;
						Result++;
					}

					continue;
				}
				else {
					::PrintError(PARAMETER_ERROR, L"Localhost server name format error", NULL, FileName, Line);
					this->LocalhostServerOptions.LocalhostServerLength = 0;
					break;
				}
			}

			if (this->LocalhostServerOptions.LocalhostServerLength > 2U)
			{
				std::shared_ptr<char> LocalhostServerName(new char[DOMAIN_MAXSIZE]());
				for (Index = 0;Index < (size_t)Result;Index++)
				{
					if (Index + 1 == Result)
					{
						LocalhostServerName.get()[Point.get()[Index]] = (int)(this->LocalhostServerOptions.LocalhostServerLength - Point.get()[Index]);
						memcpy(LocalhostServerName.get() + Point.get()[Index] + 1U, LocalhostServer.get() + Point.get()[Index] + 1U, this->LocalhostServerOptions.LocalhostServerLength - Point.get()[Index]);
					}
					else {
						LocalhostServerName.get()[Point.get()[Index]] = Point.get()[Index + 1U] - Point.get()[Index] - 1U;
						memcpy(LocalhostServerName.get() + Point.get()[Index] + 1U, LocalhostServer.get() + Point.get()[Index] + 1U, Point.get()[Index + 1U] - Point.get()[Index]);
					}
				}

				memcpy(this->LocalhostServerOptions.LocalhostServer, LocalhostServerName.get(), this->LocalhostServerOptions.LocalhostServerLength + 1U);

			}
*/
			std::shared_ptr<char> LocalhostServer(new char[DOMAIN_MAXSIZE]());
			this->LocalhostServerOptions.LocalhostServerLength = Data.length() - strlen("LocalhostServerName=");
			memcpy(LocalhostServer.get(), Data.c_str() + strlen("LocalhostServerName="), this->LocalhostServerOptions.LocalhostServerLength);
			Result = CharToDNSQuery(LocalhostServer.get(), this->LocalhostServerOptions.LocalhostServer);
			if (Result > 3U) //Domain length is between 3 and 63(Labels must be 63 characters/bytes or less, Section 2.3.1 in RFC 1035).
			{
				this->LocalhostServerOptions.LocalhostServerLength = Result;
			}
			else {
				this->LocalhostServerOptions.LocalhostServerLength = 0;
				memset(this->LocalhostServerOptions.LocalhostServer, 0, DOMAIN_MAXSIZE);
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}

//[DNSCurve] block
	else if (Data.find("DNSCurve=1") == 0)
	{
		this->DNSCurve = true;
	}
	else if (Data.find("DNSCurveProtocol=TCP") == 0 || Data.find("DNSCurveProtocol=Tcp") == 0 || Data.find("DNSCurveProtocol=tcp") == 0)
	{
		DNSCurveParameter.DNSCurveMode = DNSCURVE_REQUEST_TCPMODE;
	}
	else if (Data.find("DNSCurvePayloadSize=") == 0)
	{
		if (Data.length() == strlen("DNSCurvePayloadSize="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("DNSCurvePayloadSize=") + 2U)
		{
			Result = atol(Data.c_str() + strlen("DNSCurvePayloadSize="));
			if (Result > sizeof(eth_hdr) + sizeof(ipv4_hdr) + sizeof(udp_hdr) + sizeof(uint16_t) + DNSCURVE_MAGIC_QUERY_LEN + crypto_box_PUBLICKEYBYTES + crypto_box_HALF_NONCEBYTES)
				DNSCurveParameter.DNSCurvePayloadSize = Result;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("Encryption=1") == 0)
	{
		DNSCurveParameter.Encryption = true;
	}
	else if (Data.find("EncryptionOnly=1") == 0)
	{
		DNSCurveParameter.EncryptionOnly = true;
	}
	else if (Data.find("KeyRecheckTime=") == 0)
	{
		if (Data.length() == strlen("KeyRecheckTime="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() < strlen("KeyRecheckTime=") + 6U)
		{
			Result = atol(Data.c_str() + strlen("KeyRecheckTime="));
			if (Result >= SHORTEST_DNSCURVE_RECHECK_TIME)
				DNSCurveParameter.KeyRecheckTime = Result * TIME_OUT;
			else if (Result > 0 && Result < DEFAULT_DNSCURVE_RECHECK_TIME)
				DNSCurveParameter.KeyRecheckTime = DEFAULT_DNSCURVE_RECHECK_TIME * TIME_OUT;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}

//[DNSCurve Addresses] block
	else if (Data.find("DNSCurveIPv4DNSAddress=") == 0)
	{
		if (Data.length() == strlen("DNSCurveIPv4DNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("DNSCurveIPv4DNSAddress=") + 8U && Data.length() < strlen("DNSCurveIPv4DNSAddress=") + 22U)
		{
			Symbol = Data.find(58);
			if (Data.find(46) == std::string::npos || Symbol == std::string::npos) //IPv4 Address(".") and Port(":")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv4 Address and Port check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("DNSCurveIPv4DNSAddress="), Symbol - strlen("DNSCurveIPv4DNSAddress="));
			Result = atol(Data.c_str() + Symbol + 1U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				DNSCurveParameter.DNSCurveTarget.IPv4.sin_port = htons((uint16_t)Result);
			}
		//inet_ntop() and inet_pton() was only support in Windows Vista and newer system. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET, Target.get(), &DNSCurveParameter.DNSCurveTarget.IPv4.sin_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in);
			if (WSAStringToAddressA(Target.get(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			DNSCurveParameter.DNSCurveTarget.IPv4.sin_addr = ((PSOCKADDR_IN)&SockAddr)->sin_addr;
		#endif
			DNSCurveParameter.DNSCurveTarget.IPv4.sin_family = AF_INET;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv4AlternateDNSAddress=") == 0)
	{
		if (Data.length() == strlen("DNSCurveIPv4AlternateDNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("DNSCurveIPv4AlternateDNSAddress=") + 8U && Data.length() < strlen("DNSCurveIPv4AlternateDNSAddress=") + 22U)
		{
			Symbol = Data.find(58);
			if (Data.find(46) == std::string::npos || Symbol == std::string::npos) //IPv4 Address(".") and Port(":")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv4 Address and Port check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("DNSCurveIPv4AlternateDNSAddress="), Symbol - strlen("DNSCurveIPv4AlternateDNSAddress="));
			Result = atol(Data.c_str() + Symbol + 1U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_port = htons((uint16_t)Result);
			}
		//inet_ntop() and inet_pton() was only support in Windows Vista and newer system. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET, Target.get(), &DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in);
			if (WSAStringToAddressA(Target.get(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv4 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_addr = ((PSOCKADDR_IN)&SockAddr)->sin_addr;
		#endif
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv4.sin_family = AF_INET;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv6DNSAddress=") == 0)
	{
		if (Data.length() == strlen("DNSCurveIPv6DNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("DNSCurveIPv6DNSAddress=") + 6U && Data.length() < strlen("DNSCurveIPv6DNSAddress=") + 48U)
		{
			Symbol = Data.find(93);
			if (Data.find(58) == std::string::npos || Data.find(91) == std::string::npos || Symbol == std::string::npos) //IPv6 Address(":", "[" and "]") and Port("]:")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv6 Address check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("DNSCurveIPv6DNSAddress=") + 1U, Symbol - strlen("DNSCurveIPv6DNSAddress=") - 1U);
			Result = atol(Data.c_str() + Symbol + 2U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				DNSCurveParameter.DNSCurveTarget.IPv6.sin6_port = htons((uint16_t)Result);
			}
		//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET6, Target.get(), &DNSCurveParameter.DNSCurveTarget.IPv6.sin6_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in6);
			if (WSAStringToAddressA(Target.get(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			DNSCurveParameter.DNSCurveTarget.IPv6.sin6_addr = ((PSOCKADDR_IN6)&SockAddr)->sin6_addr;
		#endif
			DNSCurveParameter.DNSCurveTarget.IPv6.sin6_family = AF_INET6;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv6AlternateDNSAddress=") == 0)
	{
		if (Data.length() == strlen("DNSCurveIPv6AlternateDNSAddress="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("DNSCurveIPv6AlternateDNSAddress=") + 6U && Data.length() < strlen("DNSCurveIPv6AlternateDNSAddress=") + 48U)
		{
			Symbol = Data.find(93);
			if (Data.find(58) == std::string::npos || Data.find(91) == std::string::npos || Symbol == std::string::npos) //IPv6 Address(":", "[" and "]") and Port("]:")
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address or Port format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}

		//IPv6 Address check
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			memcpy(Target.get(), Data.c_str() + strlen("DNSCurveIPv6AlternateDNSAddress=") + 1U, Symbol - strlen("DNSCurveIPv6AlternateDNSAddress=") - 1U);
			Result = atol(Data.c_str() + Symbol + 2U);
			if (Result <= 0 || Result > U16_MAXNUM)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Port error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
			else {
				DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_port = htons((uint16_t)Result);
			}
		//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
		#ifdef _WIN64
			Result = inet_pton(AF_INET6, Target.get(), &DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_addr);
			if (Result == FALSE)
		#else //x86
			memset(&SockAddr, 0, sizeof(sockaddr_storage));
			SockLength = sizeof(sockaddr_in6);
			if (WSAStringToAddressA(Target.get(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
		#endif
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		#ifdef _WIN64
			else if (Result == RETURN_ERROR)
			{
				::PrintError(PARAMETER_ERROR, L"DNS server IPv6 Address format error", WSAGetLastError(), FileName, Line);
				return EXIT_FAILURE;
			}
		#else //x86
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_addr = ((PSOCKADDR_IN6)&SockAddr)->sin6_addr;
		#endif
			DNSCurveParameter.DNSCurveTarget.Alternate_IPv6.sin6_family = AF_INET6;
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}

	else if (Data.find("DNSCurveIPv4ProviderName=") == 0)
	{
		if (Data.length() == strlen("DNSCurveIPv4ProviderName="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("DNSCurveIPv4ProviderName=") + 2U && Data.length() < strlen("DNSCurveIPv4ProviderName=") + 253U) //Maximum length of whole level domain is 253 bytes(Section 2.3.1 in RFC 1035).
		{
			for (Result = strlen("DNSCurveIPv4ProviderName=");Result < (SSIZE_T)(Data.length() - strlen("DNSCurveIPv4ProviderName="));Result++)
			{
				for (Symbol = 0;Symbol < strlen(DomainTable);Symbol++)
				{
					if (Symbol == strlen(DomainTable) - 1U && Data[Result] != DomainTable[Symbol])
					{
						::PrintError(PARAMETER_ERROR, L"DNSCurve Provider Name(s) error", NULL, FileName, Line);
						return EXIT_FAILURE;
					}
					if (Data[Result] == DomainTable[Symbol])
						break;
				}
			}

			memcpy(DNSCurveParameter.DNSCurveTarget.IPv4_ProviderName, Data.c_str() + strlen("DNSCurveIPv4ProviderName="), Data.length() - strlen("DNSCurveIPv4ProviderName="));
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv4AlternateProviderName=") == 0)
	{
		if (Data.length() == strlen("DNSCurveIPv4AlternateProviderName="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("DNSCurveIPv4AlternateProviderName=") + 2U && Data.length() < strlen("DNSCurveIPv4AlternateProviderName=") + 253U) //Maximum length of whole level domain is 253 bytes(Section 2.3.1 in RFC 1035).
		{
			for (Result = strlen("DNSCurveIPv4AlternateProviderName=");Result < (SSIZE_T)(Data.length() - strlen("DNSCurveIPv4AlternateProviderName="));Result++)
			{
				for (Symbol = 0;Symbol < strlen(DomainTable);Symbol++)
				{
					if (Symbol == strlen(DomainTable) - 1U && Data[Result] != DomainTable[Symbol])
					{
						::PrintError(PARAMETER_ERROR, L"DNSCurve Provider Name(s) error", NULL, FileName, Line);
						return EXIT_FAILURE;
					}
					if (Data[Result] == DomainTable[Symbol])
						break;
				}
			}

			memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv4_ProviderName, Data.c_str() + strlen("DNSCurveIPv4AlternateProviderName="), Data.length() - strlen("DNSCurveIPv4AlternateProviderName="));
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv6ProviderName=") == 0)
	{
		if (Data.length() == strlen("DNSCurveIPv6ProviderName="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("DNSCurveIPv6ProviderName=") + 2U && Data.length() < strlen("DNSCurveIPv6ProviderName=") + 253U) //Maximum length of whole level domain is 253 bytes(Section 2.3.1 in RFC 1035).
		{
			for (Result = strlen("DNSCurveIPv6ProviderName=");Result < (SSIZE_T)(Data.length() - strlen("DNSCurveIPv6ProviderName="));Result++)
			{
				for (Symbol = 0;Symbol < strlen(DomainTable);Symbol++)
				{
					if (Symbol == strlen(DomainTable) - 1U && Data[Result] != DomainTable[Symbol])
					{
						::PrintError(PARAMETER_ERROR, L"DNSCurve Provider Name(s) error", NULL, FileName, Line);
						return EXIT_FAILURE;
					}
					if (Data[Result] == DomainTable[Symbol])
						break;
				}
			}

			memcpy(DNSCurveParameter.DNSCurveTarget.IPv6_ProviderName, Data.c_str() + strlen("DNSCurveIPv6ProviderName="), Data.length() - strlen("DNSCurveIPv6ProviderName="));
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("DNSCurveIPv6AlternateProviderName=") == 0)
	{
		if (Data.length() == strlen("DNSCurveIPv6AlternateProviderName="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("DNSCurveIPv6AlternateProviderName=") + 2U && Data.length() < strlen("DNSCurveIPv6AlternateProviderName=") + 253U) //Maximum length of whole level domain is 253 bytes(Section 2.3.1 in RFC 1035).
		{
			for (Result = strlen("DNSCurveIPv6AlternateProviderName=");Result < (SSIZE_T)(Data.length() - strlen("DNSCurveIPv6AlternateProviderName="));Result++)
			{
				for (Symbol = 0;Symbol < strlen(DomainTable);Symbol++)
				{
					if (Symbol == strlen(DomainTable) - 1U && Data[Result] != DomainTable[Symbol])
					{
						::PrintError(PARAMETER_ERROR, L"DNSCurve Provider Name(s) error", NULL, FileName, Line);
						return EXIT_FAILURE;
					}
					if (Data[Result] == DomainTable[Symbol])
						break;
				}
			}

			memcpy(DNSCurveParameter.DNSCurveTarget.Alternate_IPv6_ProviderName, Data.c_str() + strlen("DNSCurveIPv6AlternateProviderName="), Data.length() - strlen("DNSCurveIPv6AlternateProviderName="));
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}

//[DNSCurve Keys] block
	else if (Data.find("ClientPublicKey=") == 0)
	{
		if (Data.length() == strlen("ClientPublicKey="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("ClientPublicKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("ClientPublicKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			Result = HexToBinary((PUINT8)Target.get(), (const PSTR)(Data.c_str() + strlen("ClientPublicKey=")), Data.length() - strlen("ClientPublicKey="));
			if (Result == crypto_box_PUBLICKEYBYTES)
			{
				memcpy(DNSCurveParameter.DNSCurveKey.Client_PublicKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"DNSCurve Key(s) error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("ClientSecretKey=") == 0)
	{
		if (Data.length() == strlen("ClientSecretKey="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("ClientSecretKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("ClientSecretKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			Result = HexToBinary((PUINT8)Target.get(), (const PSTR)(Data.c_str() + strlen("ClientSecretKey=")), Data.length() - strlen("ClientSecretKey="));
			if (Result == crypto_box_PUBLICKEYBYTES)
			{
				memcpy(DNSCurveParameter.DNSCurveKey.Client_SecretKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"DNSCurve Key(s) error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4DNSPublicKey=") == 0)
	{
		if (Data.length() == strlen("IPv4DNSPublicKey="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv4DNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv4DNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			Result = HexToBinary((PUINT8)Target.get(), (const PSTR)(Data.c_str() + strlen("IPv4DNSPublicKey=")), Data.length() - strlen("IPv4DNSPublicKey="));
			if (Result == crypto_box_PUBLICKEYBYTES)
			{
				memcpy(DNSCurveParameter.DNSCurveKey.IPv4_ServerPublicKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"DNSCurve Key(s) error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateDNSPublicKey=") == 0)
	{
		if (Data.length() == strlen("IPv4AlternateDNSPublicKey="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv4AlternateDNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv4AlternateDNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			Result = HexToBinary((PUINT8)Target.get(), (const PSTR)(Data.c_str() + strlen("IPv4AlternateDNSPublicKey=")), Data.length() - strlen("IPv4AlternateDNSPublicKey="));
			if (Result == crypto_box_PUBLICKEYBYTES)
			{
				memcpy(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerPublicKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"DNSCurve Key(s) error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6DNSPublicKey=") == 0)
	{
		if (Data.length() == strlen("IPv6DNSPublicKey="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv6DNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv6DNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			Result = HexToBinary((PUINT8)Target.get(), (const PSTR)(Data.c_str() + strlen("IPv6DNSPublicKey=")), Data.length() - strlen("IPv6DNSPublicKey="));
			if (Result == crypto_box_PUBLICKEYBYTES)
			{
				memcpy(DNSCurveParameter.DNSCurveKey.IPv6_ServerPublicKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"DNSCurve Key(s) error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateDNSPublicKey=") == 0)
	{
		if (Data.length() == strlen("IPv6AlternateDNSPublicKey="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv6AlternateDNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv6AlternateDNSPublicKey=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			Result = HexToBinary((PUINT8)Target.get(), (const PSTR)(Data.c_str() + strlen("IPv6AlternateDNSPublicKey=")), Data.length() - strlen("IPv6AlternateDNSPublicKey="));
			if (Result == crypto_box_PUBLICKEYBYTES)
			{
				memcpy(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerPublicKey, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"DNSCurve Key(s) error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4DNSFingerprint=") == 0)
	{
		if (Data.length() == strlen("IPv4DNSFingerprint="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv4DNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv4DNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			Result = HexToBinary((PUINT8)Target.get(), (const PSTR)(Data.c_str() + strlen("IPv4DNSFingerprint=")), Data.length() - strlen("IPv4DNSFingerprint="));
			if (Result == crypto_box_PUBLICKEYBYTES)
			{
				memcpy(DNSCurveParameter.DNSCurveKey.IPv4_ServerFingerprint, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"DNSCurve Key(s) error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateDNSFingerprint=") == 0)
	{
		if (Data.length() == strlen("IPv4AlternateDNSFingerprint="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv4AlternateDNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv4AlternateDNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			Result = HexToBinary((PUINT8)Target.get(), (const PSTR)(Data.c_str() + strlen("IPv4AlternateDNSFingerprint=")), Data.length() - strlen("IPv4AlternateDNSFingerprint="));
			if (Result == crypto_box_PUBLICKEYBYTES)
			{
				memcpy(DNSCurveParameter.DNSCurveKey.Alternate_IPv4_ServerFingerprint, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"DNSCurve Key(s) error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6DNSFingerprint=") == 0)
	{
		if (Data.length() == strlen("IPv6DNSFingerprint="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv6DNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv6DNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			Result = HexToBinary((PUINT8)Target.get(), (const PSTR)(Data.c_str() + strlen("IPv6DNSFingerprint=")), Data.length() - strlen("IPv6DNSFingerprint="));
			if (Result == crypto_box_PUBLICKEYBYTES)
			{
				memcpy(DNSCurveParameter.DNSCurveKey.IPv6_ServerFingerprint, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"DNSCurve Key(s) error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateDNSFingerprint=") == 0)
	{
		if (Data.length() == strlen("IPv6AlternateDNSFingerprint="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() > strlen("IPv6AlternateDNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 2U && Data.length() < strlen("IPv6AlternateDNSFingerprint=") + crypto_box_PUBLICKEYBYTES * 3U)
		{
			std::shared_ptr<char> Target(new char[ADDR_STRING_MAXSIZE]());
			Result = HexToBinary((PUINT8)Target.get(), (const PSTR)(Data.c_str() + strlen("IPv6AlternateDNSFingerprint=")), Data.length() - strlen("IPv6AlternateDNSFingerprint="));
			if (Result == crypto_box_PUBLICKEYBYTES)
			{
				memcpy(DNSCurveParameter.DNSCurveKey.Alternate_IPv6_ServerFingerprint, Target.get(), crypto_box_PUBLICKEYBYTES);
			}
			else {
				::PrintError(PARAMETER_ERROR, L"DNSCurve Key(s) error", NULL, FileName, Line);
				return EXIT_FAILURE;
			}
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}

//[DNSCurve Magic Number] block
	else if (Data.find("IPv4ReceiveMagicNumber=") == 0)
	{
		if (Data.length() == strlen("IPv4ReceiveMagicNumber="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() == strlen("IPv4ReceiveMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.IPv4_ReceiveMagicNumber, Data.c_str() + strlen("IPv4ReceiveMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateReceiveMagicNumber=") == 0)
	{
		if (Data.length() == strlen("IPv4AlternateReceiveMagicNumber="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() == strlen("IPv4AlternateReceiveMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_ReceiveMagicNumber, Data.c_str() + strlen("IPv4AlternateReceiveMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6ReceiveMagicNumber = ") == 0)
	{
		if (Data.length() == strlen("IPv6ReceiveMagicNumber="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() == strlen("IPv6ReceiveMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.IPv6_ReceiveMagicNumber, Data.c_str() + strlen("IPv6ReceiveMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateReceiveMagicNumber=") == 0)
	{
		if (Data.length() == strlen("IPv6AlternateReceiveMagicNumber="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() == strlen("IPv6AlternateReceiveMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_ReceiveMagicNumber, Data.c_str() + strlen("IPv6AlternateReceiveMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4DNSMagicNumber=") == 0)
	{
		if (Data.length() == strlen("IPv4DNSMagicNumber="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() == strlen("IPv4DNSMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.IPv4_MagicNumber, Data.c_str() + strlen("IPv4DNSMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv4AlternateDNSMagicNumber=") == 0)
	{
		if (Data.length() == strlen("IPv4AlternateDNSMagicNumber="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() == strlen("IPv4AlternateDNSMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv4_MagicNumber, Data.c_str() + strlen("IPv4AlternateDNSMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6DNSMagicNumber=") == 0)
	{
		if (Data.length() == strlen("IPv6DNSMagicNumber="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() == strlen("IPv6DNSMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.IPv6_MagicNumber, Data.c_str() + strlen("IPv6DNSMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}
	else if (Data.find("IPv6AlternateDNSMagicNumber=") == 0)
	{
		if (Data.length() == strlen("IPv6AlternateDNSMagicNumber="))
		{
			return EXIT_SUCCESS;
		}
		else if (Data.length() == strlen("IPv6AlternateDNSMagicNumber=") + DNSCURVE_MAGIC_QUERY_LEN)
		{
			memcpy(DNSCurveParameter.DNSCurveMagicNumber.Alternate_IPv6_MagicNumber, Data.c_str() + strlen("IPv6AlternateDNSMagicNumber="), DNSCURVE_MAGIC_QUERY_LEN);
		}
		else {
			::PrintError(PARAMETER_ERROR, L"Item length error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}
	}

	return EXIT_SUCCESS;
}

//Read ipfilter from ipfilter file
size_t Configuration::ReadIPFilter(void)
{
	SSIZE_T Index = 0;

//Initialization(Available when file hash check is ON.)
	std::shared_ptr<char> Buffer;
	FileData Temp[7U];
	if (this->FileHash)
	{
		std::shared_ptr<char> TempBuffer(new char[FILE_BUFFER_SIZE]());
		Buffer.swap(TempBuffer);
		TempBuffer.reset();

		for (Index = 0;Index < sizeof(Temp) / sizeof(FileData);Index++)
		{
			std::shared_ptr<BitSequence> TempBuffer_SHA3(new BitSequence[SHA3_512_SIZE]());
			Temp[Index].Result.swap(TempBuffer_SHA3);
		}
	}

//Open file.
	FILE *Input = nullptr;
	std::vector<FileData> FileList;
	for (Index = 0;Index < sizeof(Temp) / sizeof(FileData);Index++)
		Temp[Index].FileName = Path;
	Temp[0].FileName.append(L"IPFilter.ini");
	Temp[1U].FileName.append(L"IPFilter.conf");
	Temp[2U].FileName.append(L"IPFilter.dat");
	Temp[3U].FileName.append(L"IPFilter.csv");
	Temp[4U].FileName.append(L"IPFilter");
	Temp[5U].FileName.append(L"Guarding.P2P");
	Temp[6U].FileName.append(L"Guarding");
	for (Index = 0;Index < sizeof(Temp) / sizeof(FileData);Index++)
		FileList.push_back(Temp[Index]);

//File(s) monitor
	size_t ReadLength = 0;;
	auto HashChanged = false;
	std::vector<FileData>::iterator FileListIter;
	Keccak_HashInstance HashInstance = {0};
	std::vector<AddressRange>::iterator AddressRangeIter[2U];

	in6_addr IPv6NextAddress = {0};
	in_addr IPv4NextAddress = {0};
	while (true)
	{
		HashChanged = false;
		for (FileListIter = FileList.begin();FileListIter != FileList.end();FileListIter++)
		{
			if (_wfopen_s(&Input, FileListIter->FileName.c_str(), L"rb") != 0)
			{
			//Cleanup hash(s)
				if (this->FileHash)
				{
					FileListIter->Available = false;
					memset(FileListIter->Result.get(), 0, SHA3_512_SIZE);
				}

				continue;
			}
			else {
				if (Input == nullptr)
					continue;

			//Mark or check file(s) hash.
				if (this->FileHash)
				{
					memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
					memset(&HashInstance, 0, sizeof(Keccak_HashInstance));
					Keccak_HashInitialize_SHA3_512(&HashInstance);
					while (!feof(Input))
					{
						ReadLength = fread_s(Buffer.get(), FILE_BUFFER_SIZE, sizeof(char), FILE_BUFFER_SIZE, Input);
						Keccak_HashUpdate(&HashInstance, (BitSequence *)Buffer.get(), ReadLength * 8U);
					}
					memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
					Keccak_HashFinal(&HashInstance, (BitSequence *)Buffer.get());

				//Set file pointer(s) to the beginning of file.
					if (_fseeki64(Input, 0, SEEK_SET) != 0)
					{
						::PrintError(HOSTS_ERROR, L"Read file(s) error", NULL, (PWSTR)FileListIter->FileName.c_str(), NULL);

						fclose(Input);
						Input = nullptr;
						continue;
					}
					else {
						if (FileListIter->Available)
						{
							if (memcmp(FileListIter->Result.get(), Buffer.get(), SHA3_512_SIZE) == 0)
							{
								fclose(Input);
								Input = nullptr;
								continue;
							}
							else {
								memcpy(FileListIter->Result.get(), Buffer.get(), SHA3_512_SIZE);
							}
						}
						else {
							FileListIter->Available = true;
							memcpy(FileListIter->Result.get(), Buffer.get(), SHA3_512_SIZE);
						}
					}
				}
			}

			HashChanged = true;
		//Read data.
			ReadText(Input, READTEXT_IPFILTER, (PWSTR)FileListIter->FileName.c_str());
			fclose(Input);
			Input = nullptr;
		}

	//Update AddressRange list.
		if (!HashChanged)
		{
		//Auto-refresh
			Sleep((DWORD)this->FileRefreshTime);
			continue;
		}
		else if (!AddressRangeModificating->empty())
		{
		//Check repeating ranges.
			for (AddressRangeIter[0] = AddressRangeModificating->begin();AddressRangeIter[0] != AddressRangeModificating->end();AddressRangeIter[0]++)
			{
				for (AddressRangeIter[1U] = AddressRangeIter[0] + 1U;AddressRangeIter[1U] != AddressRangeModificating->end();AddressRangeIter[1U]++)
				{
				//IPv6
					if (AddressRangeIter[0]->Protocol == AF_INET6 && AddressRangeIter[1U]->Protocol == AF_INET6)
					{
					//A-Range is not same as B-Range.
						if (CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->Begin, AF_INET6) == COMPARE_LESS)
						{
							IPv6NextAddress = AddressRangeIter[0]->End.IPv6;
						//Check next address.
							for (Index = sizeof(in6_addr) / sizeof(uint16_t) - 1U;Index >= 0;Index--)
							{
								if (IPv6NextAddress.u.Word[Index] == U16_MAXNUM)
								{
									if (Index == 0)
										break;

									IPv6NextAddress.u.Word[Index] = 0;
									continue;
								}
								else {
									IPv6NextAddress.u.Word[Index] = htons(ntohs(IPv6NextAddress.u.Word[Index]) + 1U);
									break;
								}
							}
							if (memcmp(&IPv6NextAddress, &AddressRangeIter[1U]->Begin, sizeof(in6_addr)) == 0)
							{
								AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
								AddressRangeModificating->erase(AddressRangeIter[1U]);
								AddressRangeIter[0]--;
							}

							break;
						}
					//B-Range is not same as A-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->End, AF_INET6) == COMPARE_GREATER)
						{
							IPv6NextAddress = AddressRangeIter[1U]->End.IPv6;
						//Check next address.
							for (Index = sizeof(in6_addr) / sizeof(uint16_t) - 1U;Index >= 0;Index--)
							{
								if (IPv6NextAddress.u.Word[Index] == U16_MAXNUM)
								{
									if (Index == 0)
										break;

									IPv6NextAddress.u.Word[Index] = 0;
									continue;
								}
								else {
									IPv6NextAddress.u.Word[Index] = htons(ntohs(IPv6NextAddress.u.Word[Index]) + 1U);
									break;
								}
							}
							if (memcmp(&IPv6NextAddress, &AddressRangeIter[0]->Begin, sizeof(in6_addr)) == 0)
							{
								AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
								AddressRangeModificating->erase(AddressRangeIter[1U]);
								AddressRangeIter[0]--;
							}

							break;
						}
					//A-Range is same as B-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->Begin, AF_INET6) == COMPARE_EQUAL && 
							CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->End, AF_INET6) == COMPARE_EQUAL)
						{
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A-Range connect B-Range or B-Range connect A-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->Begin, AF_INET6) == COMPARE_EQUAL)
						{
							AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&AddressRangeIter[1U]->End, &AddressRangeIter[0]->Begin, AF_INET6) == COMPARE_EQUAL)
						{
							AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A-Range include B-Range or B-Range include A-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->Begin, AF_INET6) == COMPARE_LESS && 
							CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->End, AF_INET6) == COMPARE_GREATER)
						{
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->Begin, AF_INET6) == COMPARE_GREATER && 
							CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->End, AF_INET6) == COMPARE_LESS)
						{
							*AddressRangeIter[0] = *AddressRangeIter[1U];
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A part of A-Range or B-Range is same as a part of B-Range or A-Range, also begin or end of A-Range or B-Range is same as begin or end of A-Range or B-Range.
						if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->Begin, AF_INET6) == COMPARE_EQUAL)
						{
							if (CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->End, AF_INET6) == COMPARE_LESS)
								*AddressRangeIter[0] = *AddressRangeIter[1U];

							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->End, AF_INET6) == COMPARE_EQUAL)
						{
							if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->Begin, AF_INET6) != COMPARE_LESS)
								*AddressRangeIter[0] = *AddressRangeIter[1U];

							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A part of A-Range or B-Range is same as a part of B-Range or A-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->Begin, AF_INET6) == COMPARE_GREATER)
						{
							AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&AddressRangeIter[1U]->End, &AddressRangeIter[0]->Begin, AF_INET6) == COMPARE_GREATER)
						{
							AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					}
				//IPv4
					else if (AddressRangeIter[0]->Protocol == AF_INET && AddressRangeIter[1U]->Protocol == AF_INET)
					{
					//A-Range is not same as B-Range.
						if (CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->Begin, AF_INET) == COMPARE_LESS)
						{
							IPv4NextAddress = AddressRangeIter[0]->End.IPv4;
						//Check next address.
							IPv4NextAddress.S_un.S_addr = htonl(ntohl(IPv4NextAddress.S_un.S_addr) + 1U);
							if (IPv4NextAddress.S_un.S_addr == AddressRangeIter[1U]->Begin.IPv4.S_un.S_addr)
							{
								AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
								AddressRangeModificating->erase(AddressRangeIter[1U]);
								AddressRangeIter[0]--;
							}

							break;
						}
					//B-Range is not same as A-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->End, AF_INET) == COMPARE_GREATER)
						{
							IPv4NextAddress = AddressRangeIter[1U]->End.IPv4;
						//Check next address.
							IPv4NextAddress.S_un.S_addr = htonl(ntohl(IPv4NextAddress.S_un.S_addr) + 1U);
							if (IPv4NextAddress.S_un.S_addr == AddressRangeIter[0]->Begin.IPv4.S_un.S_addr)
							{
								AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
								AddressRangeModificating->erase(AddressRangeIter[1U]);
								AddressRangeIter[0]--;
							}

							break;
						}
					//A-Range is same as B-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->Begin, AF_INET) == COMPARE_EQUAL && 
							CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->End, AF_INET) == COMPARE_EQUAL)
						{
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A-Range connect B-Range or B-Range connect A-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->Begin, AF_INET) == COMPARE_EQUAL)
						{
							AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&AddressRangeIter[1U]->End, &AddressRangeIter[0]->Begin, AF_INET) == COMPARE_EQUAL)
						{
							AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A-Range include B-Range or B-Range include A-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->Begin, AF_INET) == COMPARE_LESS && 
							CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->End, AF_INET) == COMPARE_GREATER)
						{
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->Begin, AF_INET) == COMPARE_GREATER && 
							CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->End, AF_INET) == COMPARE_LESS)
						{
							*AddressRangeIter[0] = *AddressRangeIter[1U];
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A part of A-Range or B-Range is same as a part of B-Range or A-Range, also begin or end of A-Range or B-Range is same as begin or end of A-Range or B-Range.
						if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->Begin, AF_INET) == COMPARE_EQUAL)
						{
							if (CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->End, AF_INET) == COMPARE_LESS)
								*AddressRangeIter[0] = *AddressRangeIter[1U];

							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->End, AF_INET) == COMPARE_EQUAL)
						{
							if (CompareAddresses(&AddressRangeIter[0]->Begin, &AddressRangeIter[1U]->Begin, AF_INET) != COMPARE_LESS)
								*AddressRangeIter[0] = *AddressRangeIter[1U];

							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					//A part of A-Range or B-Range is same as a part of B-Range or A-Range.
						else if (CompareAddresses(&AddressRangeIter[0]->End, &AddressRangeIter[1U]->Begin, AF_INET) == COMPARE_GREATER)
						{
							AddressRangeIter[0]->End = AddressRangeIter[1U]->End;
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
						else if (CompareAddresses(&AddressRangeIter[1U]->End, &AddressRangeIter[0]->Begin, AF_INET) == COMPARE_GREATER)
						{
							AddressRangeIter[0]->Begin = AddressRangeIter[1U]->Begin;
							AddressRangeModificating->erase(AddressRangeIter[1U]);
							AddressRangeIter[0]--;
							break;
						}
					}
				}
			}

		//Swap(or cleanup) using list.
			AddressRangeModificating->shrink_to_fit();
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			AddressRangeUsing->swap(*AddressRangeModificating);
			AddressRangeMutex.unlock();
			AddressRangeModificating->clear();
			AddressRangeModificating->shrink_to_fit();
		}
		else { //AddressRange Table is empty.
			std::unique_lock<std::mutex> AddressRangeMutex(AddressRangeLock);
			AddressRangeUsing->clear();
			AddressRangeUsing->shrink_to_fit();
			AddressRangeMutex.unlock();
			AddressRangeModificating->clear();
			AddressRangeModificating->shrink_to_fit();
		}
		
	//Auto-refresh
		Sleep((DWORD)this->FileRefreshTime);
	}

	return EXIT_SUCCESS;
}

//Read ipfilter data from ipfilter file(s)
size_t Configuration::ReadIPFilterData(const PSTR Buffer, const PWSTR FileName, const size_t Line, bool &Comments)
{
	std::string Data(Buffer);
//Multi-line comments check
	if (Comments)
	{
		if (Data.find("*/") != std::string::npos)
		{
			Data = Buffer + Data.find("*/") + 2U;
			Comments = false;
		}
		else {
			return FALSE;
		}
	}
	while (Data.find("/*") != std::string::npos)
	{
		if (Data.find("*/") == std::string::npos)
		{
			Data.erase(Data.find("/*"), Data.length() - Data.find("/*"));
			Comments = true;
			break;
		}
		else {
			Data.erase(Data.find("/*"), Data.find("*/") - Data.find("/*") + 2U);
		}
	}

//Delete spaces, horizontal tab/HT, check comments(Number Sign/NS and double slashs) and check minimum length of ipfilter items.
	if (Data.find(35) == 0 || Data.find(47) == 0 || 
		Data.find("[IPFilter]") == 0 && Data.length() == strlen("[IPFilter]"))
	{
		return EXIT_SUCCESS;
	}
	else if (Data.find(45) == std::string::npos)
	{
		::PrintError(IPFILTER_ERROR, L"Item format error", NULL, FileName, Line);
		return FALSE;
	}
	else if (Data.rfind(" //") != std::string::npos)
		Data.erase(Data.rfind(" //"), Data.length() - Data.rfind(" //"));
	else if (Data.rfind("	//") != std::string::npos)
		Data.erase(Data.rfind("	//"), Data.length() - Data.rfind("	//"));
	else if (Data.rfind(" #") != std::string::npos)
		Data.erase(Data.rfind(" #"), Data.length() - Data.rfind(" #"));
	else if (Data.rfind("	#") != std::string::npos)
		Data.erase(Data.rfind("	#"), Data.length() - Data.rfind("	#"));
	while (Data.find(9) != std::string::npos)
		Data.erase(Data.find(9), 1U);
	while (Data.find(32) != std::string::npos)
		Data.erase(Data.find(32), 1U);
	if (Data.length() < 5U)
		return EXIT_SUCCESS;

//Delete space(s), horizontal tab/HT before data.
	while (!Data.empty() && (Data.at(0) == 9 || Data.at(0) == 32))
		Data.erase(0, 1U);

//Check format of item(s).
	AddressRange Temp;
	SSIZE_T Result = 0;
	size_t Index = 0;
	if (Data.find(44) != std::string::npos && Data.find(44) > Data.find(45)) //IPFilter.dat
	{
	//IPv4 spacial delete
		if (Data.find(46) != std::string::npos)
		{
		//Delete all zero(s) before data.
			for (Index = 0;Index < Data.find(45);Index++)
			{
				if (Data[Index] == 48)
				{
					Data.erase(Index, 1U);
					Index--;
				}
				else {
					break;
				}
			}

		//Delete all zero(s) before minus or after comma(s) in addresses range.
			while (Data.find(".0") != std::string::npos)
				Data.replace((size_t)Data.find(".0"), strlen(".0"), ("."));
			while (Data.find("-0") != std::string::npos)
				Data.replace((size_t)Data.find("-0"), strlen("-0"), ("-"));
			while (Data.find("..") != std::string::npos)
				Data.replace((size_t)Data.find(".."), strlen(".."), (".0."));
			if (Data.find(".-") != std::string::npos)
				Data.replace((size_t)Data.find(".-"), strlen(".-"), (".0-"));
			if (Data.find("-.") != std::string::npos)
				Data.replace((size_t)Data.find("-."), strlen("-."), ("-0."));
			if (Data[0] == 46)
				Data.replace(0, 1U, ("0."));
		}

	//Delete all zero(s) before minus or after comma(s) in ipfilter level.
		while (Data.find(",000,") != std::string::npos)
			Data.replace((size_t)Data.find(",000,"), strlen(",000,"), (",0,"));
		while (Data.find(",00,") != std::string::npos)
			Data.replace((size_t)Data.find(",00,"), strlen(",00,"), (",0,"));
		while (Data.find(",00") != std::string::npos)
			Data.replace((size_t)Data.find(",00"), strlen(",00"), (","));
		if (Data.find(",0") != std::string::npos && Data[Data.find(",0") + 2U] != 44)
			Data.replace((size_t)Data.find(",0"), strlen(",0"), (","));

	//Mark ipfilter level.
		std::string Level;
		Level.append(Data, Data.find(44) + 1U, Data.find(44, Data.find(44) + 1U) - Data.find(44) - 1U);
		Result = atol(Level.c_str());
		if (Result >= 0 && Result <= U16_MAXNUM)
		{
			Temp.Level = (size_t)Result;
		}
		else {
			::PrintError(IPFILTER_ERROR, L"Level error", NULL, FileName, Line);
			return FALSE;
		}

	//Delete all data except addresses range.
		Data.erase(Data.find(44));
		if (Data[Data.length() - 1U] == 46)
			Data.append("0");
	}
	else { //PeerGuardian Text Lists(P2P) Format(Guarding.P2P), also a little part of IPFilter.dat without level.
	//IPv4 IPFilter.dat data without level
		if (Data.find(58) == std::string::npos)
		{
		//Delete all zero(s) before data
			for (Index = 0;Index < Data.find(45);Index++)
			{
				if (Data[Index] == 48)
				{
					Data.erase(Index, 1U);
					Index--;
				}
				else {
					break;
				}
			}

		//Delete all zero(s) before minus or after comma(s) in addresses range.
			while (Data.find(".0") != std::string::npos)
				Data.replace((size_t)Data.find(".0"), strlen(".0"), ("."));
			while (Data.find("-0") != std::string::npos)
				Data.replace((size_t)Data.find("-0"), strlen("-0"), ("-"));
			while (Data.find("..") != std::string::npos)
				Data.replace((size_t)Data.find(".."), strlen(".."), (".0."));
			if (Data.find(".-") != std::string::npos)
				Data.replace((size_t)Data.find(".-"), strlen(".-"), (".0-"));
			if (Data.find("-.") != std::string::npos)
				Data.replace((size_t)Data.find("-."), strlen("-."), ("-0."));
			if (Data[0] == 46)
				Data.replace(0, 1U, ("0."));
			if (Data[Data.length() - 1U] == 46)
				Data.append("0");
		}
		else {
		//PeerGuardian Text Lists(P2P) Format(Guarding.P2P)
			if (Data.find(58) == Data.rfind(58))
			{
				Data.erase(0, Data.find(58) + 1U);

			//Delete all zero(s) before data.
				for (Index = 0;Index < Data.find(45);Index++)
				{
					if (Data[Index] == 48)
					{
						Data.erase(Index, 1U);
						Index--;
					}
					else {
						break;
					}
				}

			//Delete all zero(s) before minus or after comma(s) in addresses range.
				while (Data.find(".0") != std::string::npos)
					Data.replace((size_t)Data.find(".0"), strlen(".0"), ("."));
				while (Data.find("-0") != std::string::npos)
					Data.replace((size_t)Data.find("-0"), strlen("-0"), ("-"));
				while (Data.find("..") != std::string::npos)
					Data.replace((size_t)Data.find(".."), strlen(".."), (".0."));
				if (Data.find(".-") != std::string::npos)
					Data.replace((size_t)Data.find(".-"), strlen(".-"), (".0-"));
				if (Data.find("-.") != std::string::npos)
					Data.replace((size_t)Data.find("-."), strlen("-."), ("-0."));
				if (Data[0] == 46)
					Data.replace(0, 1U, ("0."));
				if (Data[Data.length() - 1U] == 46)
					Data.append("0");
			}
//			else { 
//				//IPv6 IPFilter.dat data without level
//			}
		}
	}

//Read data
	std::string Address;
	//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
	#ifdef _WIN64
	#else //x86
		sockaddr_storage SockAddr = {0};
		int SockLength = 0;
	#endif

	if (Data.find(58) != std::string::npos) //IPv6
	{
		Temp.Protocol = AF_INET6;

	//Begin address
		Address.append(Data, 0, Data.find(45));
	#ifdef _WIN64
		Result = inet_pton(AF_INET6, Address.c_str(), &Temp.Begin);
		if (Result == FALSE)
	#else //x86
		SockLength = sizeof(sockaddr_in6);
		if (WSAStringToAddressA((LPSTR)Address.c_str(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
	#endif
		{
			::PrintError(IPFILTER_ERROR, L"IPv6 address format error", NULL, FileName, Line);
			return FALSE;
		}
	#ifdef _WIN64
		else if (Result == RETURN_ERROR)
		{
			::PrintError(IPFILTER_ERROR, L"IPv6 address convert error", WSAGetLastError(), FileName, Line);
			return FALSE;
		}
	#else //x86
		Temp.Begin.IPv6 = ((LPSOCKADDR_IN6)&SockAddr)->sin6_addr;
	#endif
		Address.clear();

	//End address
		Address.append(Data, Data.find(45) + 1U, Data.length() - Data.find(45));
	#ifdef _WIN64
		Result = inet_pton(AF_INET6, Address.c_str(), &Temp.End);
		if (Result == FALSE)
	#else //x86
		SockLength = sizeof(sockaddr_in6);
		if (WSAStringToAddressA((LPSTR)Address.c_str(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
	#endif
		{
			::PrintError(IPFILTER_ERROR, L"IPv6 address format error", NULL, FileName, Line);
			return FALSE;
		}
	#ifdef _WIN64
		else if (Result == RETURN_ERROR)
		{
			::PrintError(IPFILTER_ERROR, L"IPv6 address convert error", WSAGetLastError(), FileName, Line);
			return FALSE;
		}
	#else //x86
		Temp.End.IPv6 = ((LPSOCKADDR_IN6)&SockAddr)->sin6_addr;
	#endif

	//Check addresses range
		for (Index = 0;Index < sizeof(in6_addr) / sizeof(uint16_t);Index++)
		{
			if (ntohs(Temp.End.IPv6.u.Word[Index]) < ntohs(Temp.Begin.IPv6.u.Word[Index]))
			{
				::PrintError(IPFILTER_ERROR, L"IPv6 addresses range error", WSAGetLastError(), FileName, Line);
				return FALSE;
			}
			else if (ntohs(Temp.End.IPv6.u.Word[Index]) > ntohs(Temp.Begin.IPv6.u.Word[Index]))
			{
				break;
			}
			else {
				continue;
			}
		}
	}
	else { //IPv4
		Temp.Protocol = AF_INET;

	//Begin address
		Address.append(Data, 0, Data.find(45));
	#ifdef _WIN64
		Result = inet_pton(AF_INET, Address.c_str(), &Temp.Begin);
		if (Result == FALSE)
	#else //x86
		SockLength = sizeof(sockaddr_in);
		if (WSAStringToAddressA((LPSTR)Address.c_str(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
	#endif
		{
			::PrintError(IPFILTER_ERROR, L"IPv4 address format error", NULL, FileName, Line);
			return FALSE;
		}
	#ifdef _WIN64
		else if (Result == RETURN_ERROR)
		{
			::PrintError(IPFILTER_ERROR, L"IPv4 address convert error", WSAGetLastError(), FileName, Line);
			return FALSE;
		}
	#else //x86
		Temp.Begin.IPv4 = ((LPSOCKADDR_IN)&SockAddr)->sin_addr;
	#endif
		Address.clear();

	//End address
		Address.append(Data, Data.find(45) + 1U, Data.length() - Data.find(45));
	#ifdef _WIN64
		Result = inet_pton(AF_INET, Address.c_str(), &Temp.End);
		if (Result == FALSE)
	#else //x86
		SockLength = sizeof(sockaddr_in);
		if (WSAStringToAddressA((LPSTR)Address.c_str(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
	#endif
		{
			::PrintError(IPFILTER_ERROR, L"IPv4 address format error", NULL, FileName, Line);
			return FALSE;
		}
	#ifdef _WIN64
		else if (Result == RETURN_ERROR)
		{
			::PrintError(IPFILTER_ERROR, L"IPv4 address convert error", WSAGetLastError(), FileName, Line);
			return FALSE;
		}
	#else //x86
		Temp.End.IPv4 = ((LPSOCKADDR_IN)&SockAddr)->sin_addr;
	#endif

	//Check addresses range.
		if (Temp.End.IPv4.S_un.S_un_b.s_b1 < Temp.Begin.IPv4.S_un.S_un_b.s_b1)
		{
			::PrintError(IPFILTER_ERROR, L"IPv4 addresses range error", WSAGetLastError(), FileName, Line);
			return FALSE;
		}
		else if (Temp.End.IPv4.S_un.S_un_b.s_b1 == Temp.Begin.IPv4.S_un.S_un_b.s_b1)
		{
			if (Temp.End.IPv4.S_un.S_un_b.s_b2 < Temp.Begin.IPv4.S_un.S_un_b.s_b2)
			{
				::PrintError(IPFILTER_ERROR, L"IPv4 addresses range error", WSAGetLastError(), FileName, Line);
				return FALSE;
			}
			else if (Temp.End.IPv4.S_un.S_un_b.s_b2 == Temp.Begin.IPv4.S_un.S_un_b.s_b2)
			{
				if (Temp.End.IPv4.S_un.S_un_b.s_b3 < Temp.Begin.IPv4.S_un.S_un_b.s_b3)
				{
					::PrintError(IPFILTER_ERROR, L"IPv4 addresses range error", WSAGetLastError(), FileName, Line);
					return FALSE;
				}
				else if (Temp.End.IPv4.S_un.S_un_b.s_b3 == Temp.Begin.IPv4.S_un.S_un_b.s_b3)
				{
					if (Temp.End.IPv4.S_un.S_un_b.s_b4 < Temp.Begin.IPv4.S_un.S_un_b.s_b4)
					{
						::PrintError(IPFILTER_ERROR, L"IPv4 addresses range error", WSAGetLastError(), FileName, Line);
						return FALSE;
					}
				}
			}
		}
	}

//Add to global AddressRangeTable.
	AddressRangeModificating->push_back(Temp);
	return EXIT_SUCCESS;
}

//Read hosts from hosts file
size_t Configuration::ReadHosts(void)
{
	size_t Index = 0;

//Initialization(Available when file hash check is ON.)
	std::shared_ptr<char> Buffer;
	FileData Temp[5U];
	if (this->FileHash)
	{
		std::shared_ptr<char> TempBuffer(new char[FILE_BUFFER_SIZE]());
		Buffer.swap(TempBuffer);
		TempBuffer.reset();

		for (Index = 0;Index < sizeof(Temp) / sizeof(FileData);Index++)
		{
			std::shared_ptr<BitSequence> TempBuffer_SHA3(new BitSequence[SHA3_512_SIZE]());
			Temp[Index].Result.swap(TempBuffer_SHA3);
		}
	}

//Open file.
	FILE *Input = nullptr;
	std::vector<FileData> FileList;
	for (Index = 0;Index < sizeof(Temp) / sizeof(FileData);Index++)
		Temp[Index].FileName = Path;
	Temp[0].FileName.append(L"Hosts.ini");
	Temp[1U].FileName.append(L"Hosts.conf");
	Temp[2U].FileName.append(L"Hosts");
	Temp[3U].FileName.append(L"Hosts.txt");
	Temp[4U].FileName.append(L"Hosts.csv");
	for (Index = 0;Index < sizeof(Temp) / sizeof(FileData);Index++)
		FileList.push_back(Temp[Index]);

//File(s) monitor
	size_t ReadLength = 0, InnerIndex = 0;
	std::vector<FileData>::iterator FileListIter;
	Keccak_HashInstance HashInstance = {0};
	auto HashChanged = false;
	std::vector<HostsTable>::iterator HostsListIter;
	while (true)
	{
		HashChanged = false;
		for (FileListIter = FileList.begin();FileListIter != FileList.end();FileListIter++)
		{
			if (_wfopen_s(&Input, FileListIter->FileName.c_str(), L"rb") != 0)
			{
			//Cleanup hash result(s)
				if (this->FileHash)
				{
					FileListIter->Available = false;
					memset(FileListIter->Result.get(), 0, SHA3_512_SIZE);
				}

				continue;
			}
			else {
				if (Input == nullptr)
					continue;

			//Mark or check file(s) hash.
				if (this->FileHash)
				{
					memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
					memset(&HashInstance, 0, sizeof(Keccak_HashInstance));
					Keccak_HashInitialize_SHA3_512(&HashInstance);
					while (!feof(Input))
					{
						ReadLength = fread_s(Buffer.get(), FILE_BUFFER_SIZE, sizeof(char), FILE_BUFFER_SIZE, Input);
						Keccak_HashUpdate(&HashInstance, (BitSequence *)Buffer.get(), ReadLength * 8U);
					}
					memset(Buffer.get(), 0, FILE_BUFFER_SIZE);
					Keccak_HashFinal(&HashInstance, (BitSequence *)Buffer.get());

				//Set file pointer(s) to the beginning of file.
					if (_fseeki64(Input, 0, SEEK_SET) != 0)
					{
						::PrintError(HOSTS_ERROR, L"Read file(s) error", NULL, (PWSTR)FileListIter->FileName.c_str(), NULL);

						fclose(Input);
						Input = nullptr;
						continue;
					}
					else {
						if (FileListIter->Available)
						{
							if (memcmp(FileListIter->Result.get(), Buffer.get(), SHA3_512_SIZE) == 0)
							{
								fclose(Input);
								Input = nullptr;
								continue;
							}
							else {
								memcpy(FileListIter->Result.get(), Buffer.get(), SHA3_512_SIZE);
							}
						}
						else {
							FileListIter->Available = true;
							memcpy(FileListIter->Result.get(), Buffer.get(), SHA3_512_SIZE);
						}
					}
				}
			}

			HashChanged = true;
		//Read data.
			ReadText(Input, READTEXT_HOSTS, (PWSTR)FileListIter->FileName.c_str());
			fclose(Input);
			Input = nullptr;
		}

	//Update Hosts list.
		if (!HashChanged)
		{
		//Auto-refresh
			Sleep((DWORD)this->FileRefreshTime);
			continue;
		}
		else if (!HostsListModificating->empty())
		{
		//Check repeating items.
			for (HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();HostsListIter++)
			{
				if (HostsListIter->Type == HOSTS_NORMAL)
				{
				//AAAA Records(IPv6)
					if (HostsListIter->Protocol == AF_INET6 && HostsListIter->Length > sizeof(dns_aaaa_record))
					{
						for (Index = 0;Index < HostsListIter->Length / sizeof(dns_aaaa_record);Index++)
						{
							for (InnerIndex = Index + 1;InnerIndex < HostsListIter->Length / sizeof(dns_aaaa_record);InnerIndex++)
							{
								if (memcmp(HostsListIter->Response.get() + sizeof(dns_aaaa_record) * Index,
									HostsListIter->Response.get() + sizeof(dns_aaaa_record) * InnerIndex, sizeof(dns_aaaa_record)) == 0)
								{
									memmove(HostsListIter->Response.get() + sizeof(dns_aaaa_record) * InnerIndex, HostsListIter->Response.get() + sizeof(dns_aaaa_record) * (InnerIndex + 1U), sizeof(dns_aaaa_record) * (HostsListIter->Length / sizeof(dns_aaaa_record) - InnerIndex));
									HostsListIter->Length -= sizeof(dns_aaaa_record);
									InnerIndex--;
								}
							}
						}
					}
				//A Records(IPv4)
					else {
						for (Index = 0;Index < HostsListIter->Length / sizeof(dns_a_record);Index++)
						{
							for (InnerIndex = Index + 1;InnerIndex < HostsListIter->Length / sizeof(dns_a_record);InnerIndex++)
							{
								if (memcmp(HostsListIter->Response.get() + sizeof(dns_a_record) * Index,
									HostsListIter->Response.get() + sizeof(dns_a_record) * InnerIndex, sizeof(dns_a_record)) == 0)
								{
									memmove(HostsListIter->Response.get() + sizeof(dns_a_record) * InnerIndex, HostsListIter->Response.get() + sizeof(dns_a_record) * (InnerIndex + 1U), sizeof(dns_a_record) * (HostsListIter->Length / sizeof(dns_a_record) - InnerIndex));
									HostsListIter->Length -= sizeof(dns_a_record);
									InnerIndex--;
								}
							}
						}
					}
				}
			}

		//EDNS0 Lebal
			if (Parameter.EDNS0Label)
			{
				dns_hdr *pdns_hdr = nullptr;
				dns_edns0_label *EDNS0 = nullptr;

				for (HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();HostsListIter++)
				{
					if (HostsListIter->Length > PACKET_MAXSIZE - sizeof(dns_edns0_label))
					{
						::PrintError(HOSTS_ERROR, L"Data is too long when EDNS0 is available", NULL, nullptr, NULL);
						continue;
					}
					else if (!HostsListIter->Response)
					{
						continue;
					}
					else {
						EDNS0 = (dns_edns0_label *)(HostsListIter->Response.get() + HostsListIter->Length);
						EDNS0->Type = htons(DNS_EDNS0_RECORDS);
						EDNS0->UDPPayloadSize = htons((uint16_t)Parameter.EDNS0PayloadSize);
						HostsListIter->Length += sizeof(dns_edns0_label);
					}
				}
			}

		//Swap(or cleanup) using list.
			HostsListModificating->shrink_to_fit();
			std::unique_lock<std::mutex> HostsListMutex(HostsListLock);
			HostsListUsing->swap(*HostsListModificating);
			HostsListMutex.unlock();
			HostsListModificating->clear();
			HostsListModificating->shrink_to_fit();
		}
		else { //Hosts Table is empty.
			std::unique_lock<std::mutex> HostsListMutex(HostsListLock);
			HostsListUsing->clear();
			HostsListUsing->shrink_to_fit();
			HostsListMutex.unlock();
			HostsListModificating->clear();
			HostsListModificating->shrink_to_fit();
		}
		
	//Auto-refresh
		Sleep((DWORD)this->FileRefreshTime);
	}

	return EXIT_SUCCESS;
}

//Read hosts data from hosts file(s)
size_t Configuration::ReadHostsData(const PSTR Buffer, const PWSTR FileName, const size_t Line, bool &Comments, bool &Local)
{
	std::string Data(Buffer);
//Multi-line comments check
	if (Comments)
	{
		if (Data.find("*/") != std::string::npos)
		{
			Data = Buffer + Data.find("*/") + 2U;
			Comments = false;
		}
		else {
			return FALSE;
		}
	}
	while (Data.find("/*") != std::string::npos)
	{
		if (Data.find("*/") == std::string::npos)
		{
			Data.erase(Data.find("/*"), Data.length() - Data.find("/*"));
			Comments = true;
			break;
		}
		else {
			Data.erase(Data.find("/*"), Data.find("*/") - Data.find("/*") + 2U);
		}
	}

//Delete comments(Number Sign/NS and double slashs) and check minimum length of hosts items.
	if (Data.find(35) == 0 || Data.find(47) == 0)
		return EXIT_SUCCESS;
	else if (Data.rfind(" //") != std::string::npos)
		Data.erase(Data.rfind(" //"), Data.length() - Data.rfind(" //"));
	else if (Data.rfind("	//") != std::string::npos)
		Data.erase(Data.rfind("	//"), Data.length() - Data.rfind("	//"));
	else if (Data.rfind(" #") != std::string::npos)
		Data.erase(Data.rfind(" #"), Data.length() - Data.rfind(" #"));
	else if (Data.rfind("	#") != std::string::npos)
		Data.erase(Data.rfind("	#"), Data.length() - Data.rfind("	#"));
	if (Data.length() < 3U)
		return FALSE;

	auto Whitelist = false, Banned = false;
	SSIZE_T Result = 0;
//[Base] block
	if (Data.find("Version = ") == 0)
	{
		if (Data.length() < strlen("Version = ") + 8U)
		{
			double ReadVersion = atof(Data.c_str() + strlen("Version = "));
			if (ReadVersion > HOSTS_VERSION)
			{
				::PrintError(HOSTS_ERROR, L"Hosts file version error", NULL, nullptr, NULL);
				return EXIT_FAILURE;
			}
		}
		else {
			return EXIT_SUCCESS;
		}
	}
	else if (Data.find("Default TTL = ") == 0)
	{
		if (Data.length() < strlen("Default TTL = ") + 4U)
		{
			Result = atol(Data.c_str() + strlen("Default TTL = "));
			if (Result > 0 && Result < U8_MAXNUM)
			{
				if (this->HostsDefaultTTL == 0)
				{
					this->HostsDefaultTTL = (uint32_t)Result;
				}
				else if (this->HostsDefaultTTL != (uint32_t)Result)
				{
					::PrintError(HOSTS_ERROR, L"Default TTL redefinition", NULL, FileName, Line);
					return FALSE;
				}
			}
		}
		else {
			return EXIT_SUCCESS;
		}
	}

//Main hosts list
	else if (Data.find("[Hosts]") == 0)
	{
		Local = false;
		return EXIT_SUCCESS;
	}

//Local hosts list
	else if (Data.find("[Local Hosts]") == 0)
	{
		if (this->DNSTarget.Local_IPv4.sin_family != NULL || this->DNSTarget.Local_IPv6.sin6_family != NULL)
			Local = true;
			
		return EXIT_SUCCESS;
	}

//Whitelist
	else if (Data.find("NULL ") == 0 || Data.find("NULL	") == 0 || Data.find("NULL,") == 0 || 
			Data.find("Null ") == 0 || Data.find("Null	") == 0 || Data.find("Null,") == 0 || 
			Data.find("null ") == 0 || Data.find("null	") == 0 || Data.find("null,") == 0)
	{
		Whitelist = true;
	}

//Banned list
	else if (Data.find("BAN ") == 0 || Data.find("BAN	") == 0 || Data.find("BAN,") == 0 || 
		Data.find("BANNED ") == 0 || Data.find("BANNED	") == 0 || Data.find("BANNED,") == 0 || 
		Data.find("Ban ") == 0 || Data.find("Ban	") == 0 || Data.find("Ban,") == 0 || 
		Data.find("Banned ") == 0 || Data.find("Banned	") == 0 || Data.find("Banned,") == 0 || 
		Data.find("ban ") == 0 || Data.find("ban	") == 0 || Data.find("ban,") == 0 || 
		Data.find("banned ") == 0 || Data.find("banned	") == 0 || Data.find("banned,") == 0)
	{
		Banned = true;
	}

	std::shared_ptr<char> Addr(new char[ADDR_STRING_MAXSIZE]());
//Mark or delete space(s), horizontal tab/HT, and read Comma-Separated Values/CSV.
//	size_t Front = 0, Rear = 0;
	size_t Separated = 0;
	while (!Data.empty() && (Data.at(0) == 9 || Data.at(0) == 32))
		Data.erase(0, 1U);

	if (Data.find(44) != std::string::npos && //Comma-Separated Values/CSV, RFC 4180(https://tools.ietf.org/html/rfc4180), Common Format and MIME Type for Comma-Separated Values (CSV) Files).
		(Data.find(" ,") != std::string::npos || Data.find("	,") != std::string::npos || 
		Data.find(9) != std::string::npos && Data.find(9) > Data.find(44) || 
		Data.find(32) != std::string::npos && Data.find(32) > Data.find(44)))
	{
	//Delete space(s) and horizontal tab/HT
		while (Data.find(9) != std::string::npos)
			Data.erase(Data.find(9), 1U);
		while (Data.find(32) != std::string::npos)
			Data.erase(Data.find(32), 1U);

/* Old version
		while (Data.find(9) != std::string::npos)
			Data.erase(Data.find(9), 1U);
		while (Data.find(32) != std::string::npos)
			Data.erase(Data.find(32), 1U);
		Front = Data.find(44);
		Rear = Front;
		if (Data.find(44) == Data.length() - 1U)
		{
			Data.erase(Data.rfind(44));
			Front = 0;
			Rear = 0;
		}
		else if (Data.find(44) != Data.rfind(44))
		{
			Data.erase(Data.rfind(44));
		}
*/
	//Mark separated values.
		Separated = Data.find(44);
		Data.erase(Data.find(44), 1U);
	}
	else if (Data.find(44) == std::string::npos || 
		Data.find(44) != std::string::npos && (Data.find(9) < Data.find(44) || Data.find(32) < Data.find(44))) //Normal
	{
/* Old version
		if (Data.find(32) != std::string::npos) //Space
		{
			Front = Data.find(32);
			if (Data.rfind(32) > Front)
				Rear = Data.rfind(32);
			else 
				Rear = Front;
		}
		if (Data.find(9) != std::string::npos) //Horizontal Tab/HT
		{
			if (Front == 0)
				Front = Data.find(9);
			else if (Front > Data.find(9))
				Front = Data.find(9);
			if (Data.rfind(9) > Front)
				Rear = Data.rfind(9);
			else 
				Rear = Front;
		}
*/
	//Mark separated values.
		for (Separated = 0;Separated < Data.length();Separated++)
		{
			if (Data.at(Separated) == 9 || Data.at(Separated) == 32)
				break;
		}
		
	//Delete space(s) and horizontal tab/HT
		while (Data.find(9) != std::string::npos)
			Data.erase(Data.find(9), 1U);
		while (Data.find(32) != std::string::npos)
			Data.erase(Data.find(32), 1U);
	}
	else {
		::PrintError(HOSTS_ERROR, L"Item format error", NULL, FileName, Line);
		return FALSE;
	}

	HostsTable Temp;
//Whitelist part
	if (Whitelist || Banned)
	{
		Temp.PatternString.append(Data, Separated, Data.length() - Separated);
	//Mark patterns
		try {
			std::regex TempPattern(Temp.PatternString/* , std::regex_constants::extended */);
			Temp.Pattern.swap(TempPattern);
		}
		catch(std::regex_error)
		{
			::PrintError(HOSTS_ERROR, L"Regular expression pattern error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}

	//Check repeating items.
		for (auto HostsTableIter:*HostsListModificating)
		{
			if (HostsTableIter.PatternString == Temp.PatternString)
			{
				if (HostsTableIter.Type == HOSTS_NORMAL || HostsTableIter.Type == HOSTS_WHITE && !Whitelist || 
					HostsTableIter.Type == HOSTS_LOCAL || HostsTableIter.Type == HOSTS_BANNED && !Banned)
						::PrintError(HOSTS_ERROR, L"Repeating items error, the item is not available", NULL, FileName, Line);

				return EXIT_FAILURE;
			}
		}

	//Mark types.
		if (Banned)
			Temp.Type = HOSTS_BANNED;
		else 
			Temp.Type = HOSTS_WHITE;
		HostsListModificating->push_back(Temp);
	}
//Main hosts block
	else if (!Local)
	{
	//String length check
		if (Separated <= 2U) 
			return EXIT_FAILURE;
	
	//Response initialization
		std::shared_ptr<char> TempBuffer(new char[PACKET_MAXSIZE]());
		Temp.Response.swap(TempBuffer);
		TempBuffer.reset();

	//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
	#ifdef _WIN64
	#else //x86
		sockaddr_storage SockAddr = {0};
		int SockLength = 0;
	#endif

		dns_aaaa_record *pdns_aaaa_rsp = nullptr;
		dns_a_record *pdns_a_rsp = nullptr;
	//Single address
		if (Data.find(124) == std::string::npos)
		{
		//AAAA Records(IPv6)
			if (Data.find(58) < Separated)
			{
			//IPv6 addresses check
				if (Separated > ADDR_STRING_MAXSIZE)
				{
					::PrintError(HOSTS_ERROR, L"IPv6 address format error", NULL, FileName, Line);
					return EXIT_FAILURE;
				}
				else if (Data.at(0) < 48 || Data.at(0) > 58 && Data.at(0) < 65 || Data.at(0) > 71 && Data.at(0) < 97 || Data.at(0) > 102)
				{
					return EXIT_FAILURE;
				}

				Temp.Protocol = AF_INET6;
			//Make responses.
				pdns_aaaa_rsp = (dns_aaaa_record *)Temp.Response.get();
				pdns_aaaa_rsp->Name = htons(DNS_QUERY_PTR);
				pdns_aaaa_rsp->Classes = htons(DNS_CLASS_IN);
				pdns_aaaa_rsp->TTL = htonl(Parameter.HostsDefaultTTL);
				pdns_aaaa_rsp->Type = htons(DNS_AAAA_RECORDS);
				pdns_aaaa_rsp->Length = htons(sizeof(in6_addr));
			
			//Convert addresses.
				memcpy(Addr.get(), Data.c_str(), Separated);
			//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
			#ifdef _WIN64
				Result = inet_pton(AF_INET6, Addr.get(), &pdns_aaaa_rsp->Addr);
				if (Result == FALSE)
			#else //x86
				SockLength = sizeof(sockaddr_in6);
				if (WSAStringToAddressA(Addr.get(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
			#endif
				{
					::PrintError(HOSTS_ERROR, L"IPv6 address format error", NULL, FileName, Line);
					return EXIT_FAILURE;
				}
			#ifdef _WIN64
				else if (Result == RETURN_ERROR)
				{
					::PrintError(HOSTS_ERROR, L"IPv6 address convert error", WSAGetLastError(), FileName, Line);
					return EXIT_FAILURE;
				}
			#else //x86
				pdns_aaaa_rsp->Addr = ((LPSOCKADDR_IN6)&SockAddr)->sin6_addr;
			#endif
				Temp.Length = sizeof(dns_aaaa_record);
			}
		//A Records(IPv4)
			else {
			//IPv4 addresses check
				if (Separated > ADDR_STRING_MAXSIZE)
				{
					::PrintError(HOSTS_ERROR, L"IPv4 address format error", NULL, FileName, Line);
					return EXIT_FAILURE;
				}
				else if (Data.at(0) < 48 || Data.at(0) > 57)
				{
					return EXIT_FAILURE;
				}

				Temp.Protocol = AF_INET;
			//Make responses.
				pdns_a_rsp = (dns_a_record *)Temp.Response.get();
				pdns_a_rsp->Name = htons(DNS_QUERY_PTR);
				pdns_a_rsp->Classes = htons(DNS_CLASS_IN);
				pdns_a_rsp->TTL = htonl(Parameter.HostsDefaultTTL);
				pdns_a_rsp->Type = htons(DNS_A_RECORDS);
				pdns_a_rsp->Length = htons(sizeof(in_addr));
			
			//Convert addresses.
				memcpy(Addr.get(), Data.c_str(), Separated);
			//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
			#ifdef _WIN64
				Result = inet_pton(AF_INET, Addr.get(), &pdns_a_rsp->Addr);
				if (Result == FALSE)
			#else //x86
				SockLength = sizeof(sockaddr_in);
				if (WSAStringToAddressA(Addr.get(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
			#endif
				{
					::PrintError(HOSTS_ERROR, L"IPv4 address format error", NULL, FileName, Line);
					return EXIT_FAILURE;
				}
			#ifdef _WIN64
				else if (Result == RETURN_ERROR)
				{
					::PrintError(HOSTS_ERROR, L"IPv4 address convert error", WSAGetLastError(), FileName, Line);
					return EXIT_FAILURE;
				}
			#else //x86
				pdns_a_rsp->Addr = ((LPSOCKADDR_IN)&SockAddr)->sin_addr;
			#endif
				Temp.Length = sizeof(dns_a_record);
			}
		}
	//Multiple Addresses
		else {
			size_t Index = 0, VerticalIndex = 0;

		//AAAA Records(IPv6)
			if (Data.find(58) < Separated)
			{
			//IPv6 addresses check
				if (Data.at(0) < 48 || Data.at(0) > 58 && Data.at(0) < 65 || Data.at(0) > 71 && Data.at(0) < 97 || Data.at(0) > 102)
					return EXIT_FAILURE;

				Temp.Protocol = AF_INET6;
				for (Index = 0;Index <= Separated;Index++)
				{
				//Read data.
					if (Data.at(Index) == 124 || Index == Separated)
					{
					//Length check
						if (Temp.Length + sizeof(dns_aaaa_record) > PACKET_MAXSIZE)
						{
							::PrintError(HOSTS_ERROR, L"Too many Hosts IP addresses", NULL, FileName, Line);
							return EXIT_FAILURE;
						}
						else if (Index - VerticalIndex > ADDR_STRING_MAXSIZE)
						{
							::PrintError(HOSTS_ERROR, L"IPv6 address format error", NULL, FileName, Line);
							return EXIT_FAILURE;
						}

					//Make responses
						pdns_aaaa_rsp = (dns_aaaa_record *)(Temp.Response.get() + Temp.Length);
						pdns_aaaa_rsp->Name = htons(DNS_QUERY_PTR);
						pdns_aaaa_rsp->Classes = htons(DNS_CLASS_IN);
						pdns_aaaa_rsp->TTL = htonl(Parameter.HostsDefaultTTL);
						pdns_aaaa_rsp->Type = htons(DNS_AAAA_RECORDS);
						pdns_aaaa_rsp->Length = htons(sizeof(in6_addr));

					//Convert addresses.
						memset(Addr.get(), 0, ADDR_STRING_MAXSIZE);
						memcpy(Addr.get(), Data.c_str() + VerticalIndex, Index - VerticalIndex);
					//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
					#ifdef _WIN64
						Result = inet_pton(AF_INET6, Addr.get(), &pdns_aaaa_rsp->Addr);
						if (Result == FALSE)
					#else //x86
						SockLength = sizeof(sockaddr_in6);
						if (WSAStringToAddressA(Addr.get(), AF_INET6, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
					#endif
						{
							::PrintError(HOSTS_ERROR, L"IPv6 address format error", NULL, FileName, Line);
							return EXIT_FAILURE;
						}
					#ifdef _WIN64
						else if (Result == RETURN_ERROR)
						{
							::PrintError(HOSTS_ERROR, L"IPv6 address convert error", WSAGetLastError(), FileName, Line);
							return EXIT_FAILURE;
						}
					#else //x86
						pdns_aaaa_rsp->Addr = ((LPSOCKADDR_IN6)&SockAddr)->sin6_addr;
					#endif

						Temp.Length += sizeof(dns_aaaa_record);
						VerticalIndex = Index + 1U;
					}
				}
			}
		//A Records(IPv4)
			else {
			//IPv4 addresses check
				if (Data.at(0) < 48 || Data.at(0) > 57)
					return EXIT_FAILURE;

				Temp.Protocol = AF_INET;
				for (Index = 0;Index <= Separated;Index++)
				{
				//Read data.
					if (Data.at(Index) == 124 || Index == Separated)
					{
					//Length check
						if (Temp.Length + sizeof(dns_a_record) > PACKET_MAXSIZE)
						{
							::PrintError(HOSTS_ERROR, L"Too many Hosts IP addresses", NULL, FileName, Line);
							return EXIT_FAILURE;
						}
						else if (Index - VerticalIndex > ADDR_STRING_MAXSIZE)
						{
							::PrintError(HOSTS_ERROR, L"IPv4 address format error", NULL, FileName, Line);
							return EXIT_FAILURE;
						}

					//Make responses.
						pdns_a_rsp = (dns_a_record *)(Temp.Response.get() + Temp.Length);
						pdns_a_rsp->Name = htons(DNS_QUERY_PTR);
						pdns_a_rsp->Classes = htons(DNS_CLASS_IN);
						pdns_a_rsp->TTL = htonl(Parameter.HostsDefaultTTL);
						pdns_a_rsp->Type = htons(DNS_A_RECORDS);
						pdns_a_rsp->Length = htons(sizeof(in_addr));

					//Convert addresses.
						memset(Addr.get(), 0, ADDR_STRING_MAXSIZE);
						memcpy(Addr.get(), Data.c_str() + VerticalIndex, Index - VerticalIndex);
					//Minimum supported system of inet_ntop() and inet_pton() is Windows Vista. [Roy Tam]
					#ifdef _WIN64
						Result = inet_pton(AF_INET, Addr.get(), &pdns_a_rsp->Addr);
						if (Result == FALSE)
					#else //x86
						SockLength = sizeof(sockaddr_in);
						if (WSAStringToAddressA(Addr.get(), AF_INET, NULL, (LPSOCKADDR)&SockAddr, &SockLength) == SOCKET_ERROR)
					#endif
						{
							::PrintError(HOSTS_ERROR, L"IPv4 address format error", NULL, FileName, Line);
							return EXIT_FAILURE;
						}
					#ifdef _WIN64
						else if (Result == RETURN_ERROR)
						{
							::PrintError(HOSTS_ERROR, L"IPv4 address convert error", WSAGetLastError(), FileName, Line);
							return EXIT_FAILURE;
						}
					#else //x86
						pdns_a_rsp->Addr = ((LPSOCKADDR_IN)&SockAddr)->sin_addr;
					#endif

						Temp.Length += sizeof(dns_a_record);
						VerticalIndex = Index + 1;
					}
				}
			}
		}

	//Mark patterns.
		Temp.PatternString.append(Data, Separated, Data.length() - Separated);
		try {
			std::regex TempPattern(Temp.PatternString/* , std::regex_constants::extended */);
			Temp.Pattern.swap(TempPattern);
		}
		catch (std::regex_error)
		{
			::PrintError(HOSTS_ERROR, L"Regular expression pattern error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}

	//Check repeating items.
		for (auto HostsListIter = HostsListModificating->begin();HostsListIter != HostsListModificating->end();HostsListIter++)
		{
			if (HostsListIter->PatternString == Temp.PatternString)
			{
				if (HostsListIter->Type != HOSTS_NORMAL || HostsListIter->Protocol == 0)
				{
					::PrintError(HOSTS_ERROR, L"Repeating items error, the item is not available", NULL, FileName, Line);
					return EXIT_FAILURE;
				}
				else {
					if (HostsListIter->Protocol == Temp.Protocol)
					{
						if (HostsListIter->Length + Temp.Length < PACKET_MAXSIZE)
						{
							memcpy(HostsListIter->Response.get() + HostsListIter->Length, Temp.Response.get(), Temp.Length);
							HostsListIter->Length += Temp.Length;
						}

						return EXIT_SUCCESS;
					}
					else {
						continue;
					}
				}
			}
		}

	//Add to global HostsTable.
		if (Temp.Length >= sizeof(dns_qry) + sizeof(in_addr)) //Shortest reply is a A Records with Question part
			HostsListModificating->push_back(Temp);
	}

//[Local hosts] block
	else if (Local && (Parameter.DNSTarget.Local_IPv4.sin_family != NULL || Parameter.DNSTarget.Local_IPv6.sin6_family != NULL))
	{
	//Mark patterns.
		Temp.PatternString = Data;
		try {
			std::regex TempPattern(Temp.PatternString/* , std::regex_constants::extended */);
			Temp.Pattern.swap(TempPattern);
		}
		catch(std::regex_error)
		{
			::PrintError(HOSTS_ERROR, L"Regular expression pattern error", NULL, FileName, Line);
			return EXIT_FAILURE;
		}

	//Check repeating items.
		for (auto HostsTableIter:*HostsListModificating)
		{
			if (HostsTableIter.PatternString == Temp.PatternString)
			{
				if (HostsTableIter.Type != HOSTS_LOCAL)
					::PrintError(HOSTS_ERROR, L"Repeating items error, the item is not available", NULL, FileName, Line);

				return EXIT_FAILURE;
			}
		}

	//Mark types.
		Temp.Type = HOSTS_LOCAL;
		HostsListModificating->push_back(Temp);
	}

	return EXIT_SUCCESS;
}

//Read encoding of file
inline void __fastcall ReadEncoding(const PSTR Buffer, const size_t Length, size_t &Encoding, size_t &NextLineType)
{
//Read next line type.
	for (size_t Index = 4U;Index < Length - 3U;Index++)
	{
		if (Buffer[Index] == 0x0A && 
			((Encoding == ANSI || Encoding == UTF_8) && Buffer[Index - 1U] == 0x0D || 
			(Encoding == UTF_16_LE || Encoding == UTF_16_BE) && Buffer[Index - 2U] == 0x0D || 
			(Encoding == UTF_32_LE || Encoding == UTF_32_BE) && Buffer[Index - 4U] == 0x0D))
		{
			NextLineType = NEXTLINETYPE_CRLF;
			return;
		}
		else if (Buffer[Index] == 0x0A && 
			((Encoding == ANSI || Encoding == UTF_8) && Buffer[Index - 1U] != 0x0D || 
			(Encoding == UTF_16_LE || Encoding == UTF_16_BE) && Buffer[Index - 2U] != 0x0D || 
			(Encoding == UTF_32_LE || Encoding == UTF_32_BE) && Buffer[Index - 4U] != 0x0D))
		{
			NextLineType = NEXTLINETYPE_LF;
			return;
		}
		else if (Buffer[Index] == 0x0D && 
			((Encoding == ANSI || Encoding == UTF_8) && Buffer[Index + 1U] != 0x0A || 
			(Encoding == UTF_16_LE || Encoding == UTF_16_BE) && Buffer[Index + 2U] != 0x0A || 
			(Encoding == UTF_32_LE || Encoding == UTF_32_BE) && Buffer[Index + 4U] != 0x0A))
		{
			NextLineType = NEXTLINETYPE_CR;
			return;
		}
	}

//8-bit Unicode Transformation Format/UTF-8 with BOM
	if (Buffer[0] == 0xFFFFFFEF && Buffer[1U] == 0xFFFFFFBB && Buffer[2U] == 0xFFFFFFBF) //0xEF, 0xBB, 0xBF(Unsigned char)
		Encoding = UTF_8;
//32-bit Unicode Transformation Format/UTF-32 Little Endian/LE
	else if (Buffer[0] == 0xFFFFFFFF && Buffer[1U] == 0xFFFFFFFE && Buffer[2U] == 0 && Buffer[3U] == 0) //0xFF, 0xFE, 0x00, 0x00(Unsigned char)
		Encoding = UTF_32_LE;
//32-bit Unicode Transformation Format/UTF-32 Big Endian/BE
	else if (Buffer[0] == 0 && Buffer[1U] == 0 && Buffer[2U] == 0xFFFFFFFE && Buffer[3U] == 0xFFFFFFFF) //0x00, 0x00, 0xFE, 0xFF(Unsigned char)
		Encoding = UTF_32_BE;
//16-bit Unicode Transformation Format/UTF-16 Little Endian/LE
	else if (Buffer[0] == 0xFFFFFFFF && Buffer[1U] == 0xFFFFFFFE) //0xFF, 0xFE(Unsigned char)
		Encoding = UTF_16_LE;
//16-bit Unicode Transformation Format/UTF-16 Big Endian/BE
	else if (Buffer[0] == 0xFFFFFFFE && Buffer[1U] == 0xFFFFFFFF) //0xFE, 0xFF(Unsigned char)
		Encoding = UTF_16_BE;
//8-bit Unicode Transformation Format/UTF-8 without BOM/Microsoft Windows ANSI Codepages
	else 
		Encoding = ANSI;

	return;
}

//Compare two addresses
inline size_t __fastcall CompareAddresses(const void *vAddrBegin, const void *vAddrEnd, const uint16_t Protocol)
{
//IPv6
	if (Protocol == AF_INET6)
	{
		auto AddrBegin = (in6_addr *)vAddrBegin, AddrEnd = (in6_addr *)vAddrEnd;
		for (size_t Index = 0;Index < sizeof(in6_addr) / sizeof(uint16_t);Index++)
		{
			if (ntohs(AddrBegin->u.Word[Index]) > ntohs(AddrEnd->u.Word[Index]))
			{
				return COMPARE_GREATER;
			}
			else if (AddrBegin->u.Word[Index] == AddrEnd->u.Word[Index])
			{
				if (Index == sizeof(in6_addr) / sizeof(uint16_t) - 1U)
					return COMPARE_EQUAL;
				else 
					continue;
			}
			else {
				return COMPARE_LESS;
			}
		}
	}
//IPv4
	else {
		auto AddrBegin = (in_addr *)vAddrBegin, AddrEnd = (in_addr *)vAddrEnd;
		if (AddrBegin->S_un.S_un_b.s_b1 > AddrEnd->S_un.S_un_b.s_b1)
		{
			return COMPARE_GREATER;
		}
		else if (AddrBegin->S_un.S_un_b.s_b1 == AddrEnd->S_un.S_un_b.s_b1)
		{
			if (AddrBegin->S_un.S_un_b.s_b2 > AddrEnd->S_un.S_un_b.s_b2)
			{
				return COMPARE_GREATER;
			}
			else if (AddrBegin->S_un.S_un_b.s_b2 == AddrEnd->S_un.S_un_b.s_b2)
			{
				if (AddrBegin->S_un.S_un_b.s_b3 > AddrEnd->S_un.S_un_b.s_b3)
				{
					return COMPARE_GREATER;
				}
				else if (AddrBegin->S_un.S_un_b.s_b3 == AddrEnd->S_un.S_un_b.s_b3)
				{
					if (AddrBegin->S_un.S_un_b.s_b4 > AddrEnd->S_un.S_un_b.s_b4)
						return COMPARE_GREATER;
					else if (AddrBegin->S_un.S_un_b.s_b4 == AddrEnd->S_un.S_un_b.s_b4)
						return COMPARE_EQUAL;
					else 
						return COMPARE_LESS;
				}
				else {
					return COMPARE_LESS;
				}
			}
			else {
				return COMPARE_LESS;
			}
		}
		else {
			return COMPARE_LESS;
		}
	}

	return EXIT_SUCCESS;
}

//Convert hex chars to binary.
inline size_t __fastcall HexToBinary(PUINT8 Binary, const PSTR Buffer, const size_t Length)
{
	std::shared_ptr<char> Hex(new char[FILE_BUFFER_SIZE]());
//Delete colons.
	size_t Index = 0, NowLength = 0, MaxLength = 0;
	uint8_t BinaryChar[] = {0, 0, 0};
	for (Index = 0;Index < Length;Index++)
	{
		if (Buffer[Index] != 58)
		{
			Hex.get()[NowLength] = Buffer[Index];
			NowLength++;
		}
	}

	MaxLength = NowLength;
	for (Index = 0, NowLength = 0;Index < MaxLength;Index++)
	{
		memcpy(BinaryChar, Hex.get() + Index, sizeof(uint8_t) * 2U);
		Binary[NowLength] = (uint8_t)strtol((PSTR)&BinaryChar, NULL, 16);
		Index++;
		NowLength++;
	}

	return NowLength;
}
