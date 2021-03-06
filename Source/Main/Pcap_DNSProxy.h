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


#include "Pcap_DNSProxy_Base.h"

//Base defines
#define U8_MAXNUM            0x00FF               //Maximum value of uint8_t/8 bits
#define U16_MAXNUM           0xFFFF               //Maximum value of uint16_t/16 bits

//Length defines
#define FILE_BUFFER_SIZE     4096U                //Maximum size of file buffer
#define PACKET_MAXSIZE       1512U                //Maximum size of packets(1500 bytes maximum payload length + 8 bytes Ethernet header + 4 bytes FCS), Standard MTU of Ethernet network
#define LARGE_PACKET_MAXSIZE 4096U                //Maximum size of packets(4KB/4096 bytes) of TCP protocol
#define BUFFER_RING_MAXNUM   64U                  //Maximum packet buffer number
#define ADDR_STRING_MAXSIZE  64U                  //Maximum size of addresses(IPv4/IPv6) words
#define ICMP_PADDING_MAXSIZE 1484U                //Length of ICMP padding data must between 18 bytes and 1464 bytes(Ethernet MTU - IPv4 Standard Header - ICMP Header).
#define QUEUE_MAXLENGTH      64U                  //Maximum length of queue
#define QUEUE_PARTNUM        4U                   //Parts of queues(00: IPv6/UDP, 01: IPv4/UDP, 02: IPv6/TCP, 03: IPv4/TCP)
#define ALTERNATE_SERVERNUM  12U                  //Alternate switching of Main(00: TCP/IPv6, 01: TCP/IPv4, 02: UDP/IPv6, 03: UDP/IPv4), Local(04: TCP/IPv6, 05: TCP/IPv4, 06: UDP/IPv6, 07: UDP/IPv4), DNSCurve(08: TCP/IPv6, 09: TCP/IPv4, 10: UDP/IPv6, 11: UDP/IPv4)
#define DOMAIN_MAXSIZE       256U                 //Maximum size of whole level domain is 253 bytes(Section 2.3.1 in RFC 1035).

//Code defines
#define RETURN_ERROR                   -1
#define SHA3_512_SIZE                  64U           //SHA3-512 instance as specified in the FIPS 202 draft in April 2014(http://csrc.nist.gov/publications/drafts/fips-202/fips_202_draft.pdf), 512 bits/64 bytes.
#define MBSTOWCS_NULLTERMINATE         -1            //MultiByteToWideChar() find null-terminate.
#define SYSTEM_SOCKET                  UINT_PTR      //System Socket defined(WinSock2.h), not the same in x86(unsigned int) and x64(unsigned __int64) platform.

//Time defines
#define TIME_OUT                       1000U         //Timeout(1000 ms or 1 second)
#define PCAP_FINALLDEVS_RETRY          90U           //Retry to get device list in 15 minutes(900 seconds).
#define PCAP_DEVICESRECHECK_TIME       10U           //Time between every WinPcap/LibPcap devices recheck
#define RELIABLE_SOCKET_TIMEOUT        5U            //Timeout of reliable sockets(TCP, 5 seconds)
#define UNRELIABLE_SOCKET_TIMEOUT      2U            //Timeout of unreliable sockets(ICMP/ICMPv6/UDP, 2 seconds)
#define DEFAULT_FILEREFRESH_TIME       5U            //Default time between file(s) auto-refreshing
#define DEFAULT_ALTERNATE_TIMES        10U           //Default times of requesting timeout.
#define DEFAULT_ALTERNATE_RANGE        10U           //Default time of checking timeout.
#define DEFAULT_ALTERNATERESET_TIME    180U          //Default time to reset switching of alternate server(s).
#define DEFAULT_ICMPTEST_TIME          5U            //Default time between ICMP Test
#define DEFAULT_HOSTS_TTL              900U          //Default Hosts DNS TTL, 15 minutes(900 seconds)
#define DEFAULT_DNSCURVE_RECHECK_TIME  3600U         //Default DNSCurve key(s) recheck time, 1 hour(3600 seconds)
#define SHORTEST_DNSCURVE_RECHECK_TIME 10U           //The shortset DNSCurve key(s) recheck time, 10 seconds
#define SENDING_INTERVAL_TIME          5U            //Time between every sending
#define ONCESEND_TIME                  3U            //Repeat 3 times between every sending

//Default data defines
#define LOCAL_SERVICENAME    L"PcapDNSProxyService"                 //Name of local server service
#define LOCAL_SERVERNAME     ("pcap-dnsproxy.localhost.server")     //Ping padding data(Microsoft Windows)
#define LOCAL_PADDINGDATA    ("abcdefghijklmnopqrstuvwabcdefghi")   //Localhost DNS server name

//Error Type defines, see PrintError.cpp
#define SYSTEM_ERROR         0
#define PARAMETER_ERROR      1U
#define IPFILTER_ERROR       2U
#define HOSTS_ERROR          3U
#define WINSOCK_ERROR        4U
#define WINPCAP_ERROR        5U
#define DNSCURVE_ERROR       6U

//Modes and types defines
#define LISTEN_PROXYMODE               0
#define LISTEN_PRIVATEMODE             1U 
#define LISTEN_SERVERMODE              2U
#define LISTEN_CUSTOMMODE              3U
//#define REQUEST_UDPMODE                0
#define REQUEST_TCPMODE                1U
#define HOSTS_NORMAL                   0
#define HOSTS_WHITE                    1U
#define HOSTS_LOCAL                    2U
#define HOSTS_BANNED                   3U
#define CACHE_TIMER                    1U
#define CACHE_QUEUE                    2U
//#define DNSCURVE_REQUEST_UDPMODE       0
#define DNSCURVE_REQUEST_TCPMODE       1U

//Server type defines
#define PREFERRED_SERVER               0             //Main
#define ALTERNATE_SERVER               1U            //Alternate
#define DNSCURVE_MAINIPV6              0             //DNSCurve Main(IPv6)
#define DNSCURVE_MAINIPV4              1U            //DNSCurve Main(IPv4)
#define DNSCURVE_ALTERNATEIPV6         2U            //DNSCurve Alternate(IPv6)
#define DNSCURVE_ALTERNATEIPV4         3U            //DNSCurve Alternate(IPv4)

//Socket Data structure
typedef struct _socket_data_
{
	SYSTEM_SOCKET            Socket;
	sockaddr_storage         SockAddr;
	int                      AddrLen;
}SOCKET_Data, SOCKET_DATA;

//File Data structure
typedef struct _filedata_data_ {
	bool                               Available;
	std::wstring                       FileName;
	std::shared_ptr<BitSequence>       Result;
}FileData, FILE_DATA;

//DNS Cache structure
typedef struct _dnscache_data_ {
	std::string              Domain;
	std::shared_ptr<char>    Response;
	uint16_t                 Protocol;
	size_t                   Length;
	size_t                   Time;
}DNSCacheData, DNSCACHE_DATA;

//Configuration class
class Configuration {
public:
	SYSTEM_SOCKET            LocalSocket[QUEUE_PARTNUM];
//[Base] block
	double                   Version;
	bool                     PrintError;
	size_t                   FileRefreshTime;
	bool                     FileHash;
	bool                     HostsOnly;
//[DNS] block
	size_t                   RquestMode;
	size_t                   CacheType;
	size_t                   CacheParameter;
//[Listen] block
	size_t                   OperationMode;
	uint16_t                 ListenPort;
	struct _ipfilter_options_ {
		bool                 Type;
		size_t               IPFilterLevel;
	}IPFilterOptions;
//[Addresses] block
	struct _dns_target_ {
		sockaddr_in          IPv4;
		sockaddr_in          Alternate_IPv4;
		sockaddr_in6         IPv6;
		sockaddr_in6         Alternate_IPv6;
		sockaddr_in          Local_IPv4;
		sockaddr_in          Alternate_Local_IPv4;
		sockaddr_in6         Local_IPv6;
		sockaddr_in6         Alternate_Local_IPv6;
	}DNSTarget;
//[Values] block
	size_t                   EDNS0PayloadSize;
	struct _hoplimit_options_ {
		uint8_t              IPv4TTL;
		uint8_t              Alternate_IPv4TTL;
		uint8_t              IPv6HopLimit;
		uint8_t              Alternate_IPv6HopLimit;
		uint8_t              HopLimitFluctuation;
	}HopLimitOptions;
	struct _alternate_options_ {
		size_t               AlternateTimes;
		size_t               AlternateTimeRange;
		size_t               AlternateResetTime;
	}AlternateOptions;
	struct _icmp_options_ {
		uint16_t             ICMPID;
		uint16_t             ICMPSequence;
		size_t               ICMPSpeed;
	}ICMPOptions;
	struct _domaintest_options_ {
		bool                 DomainTestCheck;
		PSTR                 DomainTest;
		uint16_t             DomainTestID;
		size_t               DomainTestSpeed;
	}DomainTestOptions;
//[Switches] block
	bool                     EDNS0Label;
	bool                     DNSSECRequest;
	bool                     IPv4DataCheck;
	bool                     TCPDataCheck;
	bool                     DNSDataCheck;
	bool                     Blacklist;
//[Data] block
	struct _paddingdata_options_ {
		PSTR                 PaddingData;
		size_t               PaddingDataLength;
	}PaddingDataOptions;
	struct _localhostserver_options_ {
		PSTR                 LocalhostServer;
		size_t               LocalhostServerLength;
	}LocalhostServerOptions;
//[DNSCurve/DNSCrypt] block
	bool                     DNSCurve;
//Hosts file(s) block
	uint32_t                 HostsDefaultTTL;

	::Configuration(void);
	~Configuration(void);

//Configuration.cpp
	inline bool ReadText(const FILE *Input, const size_t InputType, const PWSTR FileName);
	size_t ReadParameter(void);
	size_t ReadIPFilter(void);
	size_t ReadHosts(void);

private:
	size_t ReadParameterData(const PSTR Buffer, const PWSTR FileName, const size_t Line, bool &Comments);
	size_t ReadIPFilterData(const PSTR Buffer, const PWSTR FileName, const size_t Line, bool &Comments);
	size_t ReadHostsData(const PSTR Buffer, const PWSTR FileName, const size_t Line, bool &Comments, bool &Local);
};

//Hosts lists class
class HostsTable {
public:
	size_t                   Type;
	uint16_t                 Protocol;
	size_t                   Length;
	std::shared_ptr<char>    Response;
	std::regex               Pattern;
	std::string              PatternString;

	HostsTable(void);
};

//IP(v4/v6) addresses ranges class
class AddressRange {
public:
	uint16_t                 Protocol;
	size_t                   Level;
	union {
		in_addr              IPv4;
		in6_addr             IPv6;
	}Begin;
	union {
		in_addr              IPv4;
		in6_addr             IPv6;
	}End;

	AddressRange(void);
};

//System&Request port list class
class PortTable {
public:
	SOCKET_DATA              *SendData;    //Request ports records
	SOCKET_DATA              *RecvData;    //System receive sockets/Addresses records
	PortTable(void);
	~PortTable(void);

//Capture.cpp
	size_t MatchToSend(const PSTR Buffer, const size_t Length, const uint16_t RequestPort);
};

//Alternate swap table class
class AlternateSwapTable {
public:
	bool                     Swap[ALTERNATE_SERVERNUM];
	size_t                   TimeoutTimes[ALTERNATE_SERVERNUM];
	size_t                   *PcapAlternateTimeout;

	AlternateSwapTable(void);
};

//DNSCurve Configuration class
class DNSCurveConfiguration {
public:
	size_t                   DNSCurvePayloadSize;
	size_t                   DNSCurveMode;
	bool                     Encryption;
	bool                     EncryptionOnly;
	size_t                   KeyRecheckTime;
	struct _dnscurve_target_ {
		sockaddr_in          IPv4;
		sockaddr_in          Alternate_IPv4;
		sockaddr_in6         IPv6;
		sockaddr_in6         Alternate_IPv6;
	//Server Provider Name
		PSTR                 IPv4_ProviderName;
		PSTR                 Alternate_IPv4_ProviderName;
		PSTR                 IPv6_ProviderName;
		PSTR                 Alternate_IPv6_ProviderName;
	}DNSCurveTarget;
	struct _dnscurve_key_ {
		PUINT8               Client_PublicKey;
		PUINT8               Client_SecretKey;
	//DNSCurve Precomputation Keys
		PUINT8               IPv4_PrecomputationKey;
		PUINT8               Alternate_IPv4_PrecomputationKey;
		PUINT8               IPv6_PrecomputationKey;
		PUINT8               Alternate_IPv6_PrecomputationKey;
	//Server Public Keys
		PUINT8               IPv4_ServerPublicKey;
		PUINT8               Alternate_IPv4_ServerPublicKey;
		PUINT8               IPv6_ServerPublicKey;
		PUINT8               Alternate_IPv6_ServerPublicKey;
	//Server Fingerprints
		PUINT8               IPv4_ServerFingerprint;
		PUINT8               Alternate_IPv4_ServerFingerprint;
		PUINT8               IPv6_ServerFingerprint;
		PUINT8               Alternate_IPv6_ServerFingerprint;
	}DNSCurveKey;
	struct _dnscurve_magic_number_ {
	//Receive Magic Number(Same from server receive)
		PSTR                 IPv4_ReceiveMagicNumber;
		PSTR                 Alternate_IPv4_ReceiveMagicNumber;
		PSTR                 IPv6_ReceiveMagicNumber;
		PSTR                 Alternate_IPv6_ReceiveMagicNumber;
	//Server Magic Number(Send to server)
		PSTR                 IPv4_MagicNumber;
		PSTR                 Alternate_IPv4_MagicNumber;
		PSTR                 IPv6_MagicNumber;
		PSTR                 Alternate_IPv6_MagicNumber;
	}DNSCurveMagicNumber;

	::DNSCurveConfiguration(void);
	~DNSCurveConfiguration(void);
};

//PrintError.cpp
size_t __fastcall PrintError(const size_t Type, const PWSTR Message, const SSIZE_T Code, const PWSTR FileName, const size_t Line);

//Protocol.cpp
bool __fastcall CheckEmptyBuffer(const void *Buffer, size_t Length);
uint64_t __fastcall hton64(const uint64_t Val);
uint64_t __fastcall ntoh64(const uint64_t Val);
//uint32_t __fastcall GetFCS(const PUINT8 Buffer, const size_t Length);
uint16_t __fastcall GetChecksum(const uint16_t *Buffer, const size_t Length);
uint16_t __fastcall ICMPv6Checksum(const PUINT8 Buffer, const size_t Length, const in6_addr Destination, const in6_addr Source);
bool __fastcall CheckSpecialAddress(const void *Addr, const uint16_t Protocol);
uint16_t __fastcall TCPUDPChecksum(const PUINT8 Buffer, const size_t Length, const uint16_t NetworkLayer, const uint16_t TransportLayer);
size_t __fastcall CharToDNSQuery(const PSTR FName, PSTR TName);
size_t __fastcall DNSQueryToChar(const PSTR TName, PSTR FName);
bool __fastcall GetLocalAddress(sockaddr_storage &SockAddr, const uint16_t Protocol);
size_t __fastcall LocalAddressToPTR(const uint16_t Protocol);
void __fastcall RamdomDomain(PSTR Domain);

//Configuration.cpp
inline void __fastcall ReadEncoding(const PSTR Buffer, const size_t Length, size_t &Encoding, size_t &NextLineType);
inline size_t __fastcall CompareAddresses(const void *vAddrBegin, const void *vAddrEnd, const uint16_t Protocol);
inline size_t __fastcall HexToBinary(PUINT8 Binary, const PSTR Buffer, const size_t Length);

//Service.cpp
size_t WINAPI ServiceMain(DWORD argc, LPTSTR *argv);
size_t WINAPI ServiceControl(const DWORD dwControlCode);
BOOL WINAPI ExecuteService(void);
void WINAPI TerminateService(void);
DWORD WINAPI ServiceProc(LPVOID lpParameter);
BOOL WINAPI UpdateServiceStatus(const DWORD dwCurrentState, const DWORD dwWin32ExitCode, const DWORD dwServiceSpecificExitCode, const DWORD dwCheckPoint, const DWORD dwWaitHint);

//Main.cpp
inline size_t __fastcall FileInit(const PWSTR wPath);
inline size_t __fastcall FirewallTest(uint16_t Protocol);

//Monitor.cpp
size_t __fastcall MonitorInit(void);
size_t __fastcall UDPMonitor(const SOCKET_DATA LocalhostData);
size_t __fastcall TCPMonitor(const SOCKET_DATA LocalhostData);
inline bool __fastcall CustomModeFilter(const void *pAddr, const uint16_t Protocol);
inline void __fastcall AlternateServerSwitcher(void);
void __fastcall DNSCacheTimerMonitor(const size_t CacheType);

//DNSCurve.cpp
bool __fastcall VerifyKeypair(const PUINT8 PublicKey, const PUINT8 SecretKey);
size_t __fastcall DNSCurveInit(void);
inline size_t LocalSignatureRequest(const PSTR Send, const size_t SendSize, PSTR Recv, const size_t RecvSize);
inline bool __fastcall TCPSignatureRequest(const uint16_t NetworkLayer, const bool Alternate);
inline bool __fastcall UDPSignatureRequest(const uint16_t NetworkLayer, const bool Alternate);
bool __fastcall GetSignatureData(const PSTR Buffer, const size_t ServerType);
size_t __fastcall TCPDNSCurveRequest(const PSTR Send, const size_t SendSize, PSTR Recv, const size_t RecvSize, const SOCKET_DATA TargetData, const bool Alternate, const bool Encryption);
size_t __fastcall UDPDNSCurveRequest(const PSTR Send, const size_t SendSize, PSTR Recv, const size_t RecvSize, const SOCKET_DATA TargetData, const bool Alternate, const bool Encryption);

//Process.cpp
size_t __fastcall RequestProcess(const PSTR Send, const size_t Length, const SOCKET_DATA FunctionData, const uint16_t Protocol, const size_t Index);
inline size_t __fastcall CheckHosts(const PSTR Request, const size_t Length, PSTR Result, const size_t ResultSize, bool &Local);
size_t __fastcall TCPReceiveProcess(const SOCKET_DATA FunctionData, const size_t Index);
size_t __fastcall MarkDNSCache(const PSTR Buffer, const size_t Length);

//Captrue.cpp
size_t __fastcall CaptureInit(void);
inline void __fastcall FilterRulesInit(std::string &FilterRules);
size_t __fastcall Capture(const pcap_if *pDrive, const bool List);
size_t __fastcall NetworkLayer(const PSTR Recv, const size_t Length, const uint16_t Protocol);
inline bool __fastcall ICMPCheck(const PSTR Buffer, const size_t Length, const uint16_t Protocol);
inline bool __fastcall TCPCheck(const PSTR Buffer);
inline bool __fastcall DTDNSDCheck(const PSTR Buffer, const size_t Length, bool &SignHopLimit);
inline size_t __fastcall DNSMethod(const PSTR Recv, const size_t Length, const uint16_t Protocol);

//Request.cpp
size_t __fastcall DomainTestRequest(const uint16_t Protocol);
size_t __fastcall ICMPEcho(void);
size_t __fastcall ICMPv6Echo(void);
size_t __fastcall TCPRequest(const PSTR Send, const size_t SendSize, PSTR Recv, const size_t RecvSize, const SOCKET_DATA TargetData, const bool Local, const bool Alternate);
size_t __fastcall UDPRequest(const PSTR Send, const size_t Length, const SOCKET_DATA TargetData, const size_t Index, const bool Local, const bool Alternate);
size_t __fastcall UDPCompleteRequest(const PSTR Send, const size_t SendSize, PSTR Recv, const size_t RecvSize, const SOCKET_DATA TargetData, const bool Local, const bool Alternate);

//Console.cpp
#ifdef _DEBUG
BOOL WINAPI CtrlHandler(const DWORD fdwCtrlType);
#endif
