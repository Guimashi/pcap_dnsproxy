@echo off

:Type
sc stop PcapDNSProxyService
sc delete PcapDNSProxyService
if "%PROCESSOR_ARCHITECTURE%%PROCESSOR_ARCHITEW6432%" == "x86" (goto X86) else goto X64

:X86
sc create PcapDNSProxyService binPath= "%~dp0Pcap_DNSProxy_x86.exe" start= auto
reg add HKLM\SYSTEM\CurrentControlSet\Services\PcapDNSProxyService\Parameters /v Application /d "%~dp0Pcap_DNSProxy_x86.exe" /f
reg add HKLM\SYSTEM\CurrentControlSet\Services\PcapDNSProxyService\Parameters /v AppDirectory /d "%~dp0" /f
Pcap_DNSProxy_x86 --FirstStart
goto Exit

:X64
sc create PcapDNSProxyService binPath= "%~dp0Pcap_DNSProxy.exe" start= auto
reg add HKLM\SYSTEM\CurrentControlSet\Services\PcapDNSProxyService\Parameters /v Application /d "%~dp0Pcap_DNSProxy.exe" /f
reg add HKLM\SYSTEM\CurrentControlSet\Services\PcapDNSProxyService\Parameters /v AppDirectory /d "%~dp0" /f
Pcap_DNSProxy --FirstStart

:Exit
sc description PcapDNSProxyService "A local DNS server base on WinPcap and LibPcap."
sc start PcapDNSProxyService
echo.
echo Done. Please confirm the PcapDNSProxyService service had been installed.
echo.
pause
