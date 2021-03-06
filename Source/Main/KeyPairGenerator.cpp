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


#include "KeyPairGenerator.h"

//Main function of program
int main(int argc, char *argv[])
{
//Libsodium initialization
	if (sodium_init() != 0)
	{
		wprintf_s(L"Libsodium initialization error\n");
		system("Pause");

		return EXIT_FAILURE;
	}

	FILE *Output = nullptr;
//Output
	_wfopen_s(&Output, L"KeyPair.txt", L"w+,ccs=UTF-8");
	if (Output != nullptr)
	{
	//Initialization and make keypair.
		std::shared_ptr<char> Buffer(new char[KEYPAIR_MESSAGE_LENGTH]());
		std::shared_ptr<wchar_t> wBuffer(new wchar_t[KEYPAIR_MESSAGE_LENGTH]());
		std::shared_ptr<uint8_t> PublicKey(new uint8_t[crypto_box_PUBLICKEYBYTES]()), SecretKey(new uint8_t[crypto_box_SECRETKEYBYTES]());
		crypto_box_curve25519xsalsa20poly1305_keypair(PublicKey.get(), SecretKey.get());

	//Write public key.
		BinaryToHex(Buffer.get(), KEYPAIR_MESSAGE_LENGTH, PublicKey.get(), crypto_box_PUBLICKEYBYTES);
		MultiByteToWideChar(CP_ACP, NULL, Buffer.get(), MBSTOWCS_NULLTERMINATE, wBuffer.get(), KEYPAIR_MESSAGE_LENGTH);
		fwprintf_s(Output, L"Public Key: %s\n", wBuffer.get());

	//Write secret key.
		memset(Buffer.get(), 0, KEYPAIR_MESSAGE_LENGTH);
		memset(wBuffer.get(), 0, KEYPAIR_MESSAGE_LENGTH*sizeof(wchar_t));
		BinaryToHex(Buffer.get(), KEYPAIR_MESSAGE_LENGTH, SecretKey.get(), crypto_box_SECRETKEYBYTES);
		MultiByteToWideChar(CP_ACP, NULL, Buffer.get(), MBSTOWCS_NULLTERMINATE, wBuffer.get(), KEYPAIR_MESSAGE_LENGTH);
		fwprintf_s(Output, L"Secret Key: %s\n", wBuffer.get());
		fclose(Output);
	}
	else {
		wprintf_s(L"Cannot create target file(KeyPair.txt)\n");
		system("Pause");

		return EXIT_FAILURE;
	}

	wprintf_s(L"Create ramdom key pair success, please check KeyPair.txt.\n");
	system("Pause");

	return EXIT_SUCCESS;
}

//Convert binary to hex chars
inline size_t __fastcall BinaryToHex(PSTR Buffer, const size_t MaxLength, const PUINT8 Binary, const size_t Length)
{
	size_t BufferLength = 0, Colon = 0;
	for (size_t Index = 0;Index < Length;Index++)
	{
		if (BufferLength < MaxLength)
		{
			Buffer[BufferLength] = Binary[Index] >> 4U;
			Buffer[BufferLength + 1U] = Binary[Index] << 4U;
			Buffer[BufferLength + 1U] = Binary[BufferLength + 1U] >> 4U;

		//Convert to ASCII.
			if (Buffer[BufferLength] < 10U)
				Buffer[BufferLength] += 48U; //Number
			else if (Buffer[BufferLength] <= 16U)
				Buffer[BufferLength] += 55U; //Captain letters
			if (Buffer[BufferLength + 1U] < 10U)
				Buffer[BufferLength + 1U] += 48U; //Number
			else if (Buffer[BufferLength + 1U] <= 16U)
				Buffer[BufferLength + 1U] += 55U; //Captain letters

		//Add colons.
			Colon++;
			if (Colon == 2U && Index != Length - 1U)
			{
				Colon = 0;
				Buffer[BufferLength + 2U] = 58;
				BufferLength += 3U;
			}
			else {
				BufferLength += 2U;
			}
		}
		else {
			break;
		}
	}

	return BufferLength;
}
