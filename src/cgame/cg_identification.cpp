// ETJump 2.0.3 authentication system
//
// ETJump 2.0.3 comes with a new authentication system. The old system with
// usernames & passwords is no longer there. This new system will be more
// secure while also being easier to use.
//
// Each client is authenticated using a unique identifier generated by
// boost::uuid. A hash of this unique identifier will be generated and saved
// to "etguid" in the mod folder. If a client already has a guid the existing
// one will be used.
//
// Each guid is hashed with Sha1 algorithm and the hash is 40 characters in
// length.
//
// Once the GUID is either read or generated, it goes through a test to see
// if it is a legit GUID. If the guid is legit it will be sent to server
//     using the following command:
//
// "etguid AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
//
// Server receives the guid, checks if it's valid and generates a Sha1-hash
// from it. The hash it then saved to a data structure. Server tracks whether
// user has already sent a guid. If user hasn't sent a guid commands that
// require user to have a guid will print an error message upon execution.


extern "C" {
#include "cg_local.h"
}

#include <string>
#include <fstream>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

// User's unique identifier
std::string userGuid = "";

// Uses boost::uuid to generate a random guid and returns it as a string
std::string GenerateUUID()
{
	boost::uuids::random_generator gen;
	boost::uuids::uuid             u = gen();

	return boost::lexical_cast<std::string>(u);
}

bool CreateNewGuid()
{
	fileHandle_t f = -1;

	userGuid = G_SHA1(GenerateUUID().c_str());
	// Convert to uppercase
	boost::to_upper(userGuid);

	if (trap_FS_FOpenFile("etguid.dat", &f, FS_WRITE) < 0)
	{
		// Couldn't open "etguid", print error and send the generated
		// guid for temporary identification.
		CG_Printf("^1ERROR: ^7failed to open file \"etguid\" to "
		          "save guid. Sending a temporary guid to server.\n");

		return false;

	}

	// Write the guid to a file
	trap_FS_Write(userGuid.c_str(), userGuid.length(), f);
	trap_FS_FCloseFile(f);
	return true;

}

bool ValidGuid(const std::string& guid)
{
	// Make sure length is correct
	if (guid.length() != 40)
	{
		// Length was not correct
		CG_Printf("^1ERROR:^7 invalid guid length: %d\n", guid.length());
		return false;

	}

	// Check each character for illegal chars
	for (size_t i = 0; i < guid.length(); i++)
	{
		// It's a sha hashed guid converted to hexadecimal. If values
		// greater than F are found in it, it's a fake guid.
		if (guid[i] < '0' || guid[i] > 'F')
		{
			CG_Printf("^1ERROR: ^7invalid character %c in guid.\n",
			          guid[i]);
			return false;

		}

	}

	return true;

}

const int GUID_LEN = 40;
// Reads the guid and if it does not exist, create it
void ReadGuid()
{
	fileHandle_t f = -1;

	int fileLen = trap_FS_FOpenFile("etguid.dat", &f, FS_READ);

	// Let's see if the file exists already
	if (fileLen < 0)
	{
		trap_FS_FCloseFile(f);
		// The GUID does not exist, create a new one.
		CreateNewGuid();
		CG_Printf("^3No GUID was found. Creating a new one.\n");
	}
	else
	{
		// The GUID exists, read it
		char guidBuf[GUID_LEN + 1];
		trap_FS_Read(guidBuf, GUID_LEN, f);
		guidBuf[GUID_LEN] = 0;

		if (!ValidGuid(guidBuf))
		{
			CreateNewGuid();
		}

		// if it was a valid guid, just save it to data structure
		userGuid = guidBuf;
		CG_Printf("^3GUID was found.\n");
		trap_FS_FCloseFile(f);
	}
}

const char *GetHWID(void);

// Function is called when the guid is sent
void SendGuid()
{
	// Let's first make sure we have the guid
	ReadGuid();

	// Hash the guid again and send it to server
	trap_SendClientCommand(va("etguid %s %s", G_SHA1(userGuid.c_str()), GetHWID()));
}

#if defined __linux__

#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>

void CG_Minimize_f(void)
{
	return;
}

/*
 * Sends a hashed MAC address as I didn't find a decent HWID
 * library for linux
 */

const char *GetHWID(void)
{
	struct ifreq  ifr;
	struct ifconf ifc;
	char          buf[1024];
	int           success = 0;

	int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1)
	{                 /* handle error*/
	}
	;

	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;
	if (ioctl(sock, SIOCGIFCONF, &ifc) == -1)
	{                                           /* handle error */
	}

	struct ifreq              *it = ifc.ifc_req;
	const struct ifreq *const end = it + (ifc.ifc_len / sizeof(struct ifreq));

	for (; it != end; ++it)
	{
		strcpy(ifr.ifr_name, it->ifr_name);
		if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0)
		{
			if (!(ifr.ifr_flags & IFF_LOOPBACK))   // don't count loopback
			{
				if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0)
				{
					success = 1;
					break;
				}
			}
		}
		else
		{        /* handle error */
		}
	}

	unsigned char mac_address[6];

	if (success)
	{
		memcpy(mac_address, ifr.ifr_hwaddr.sa_data, 6);
		return G_SHA1(va("%02X:%02X:%02X:%02X:%02X:%02X",
		                 mac_address[0],
		                 mac_address[1],
		                 mac_address[2],
		                 mac_address[3],
		                 mac_address[4],
		                 mac_address[5]));
	}
	else
	{
		return "NOHWID";
	}
}

#elif defined WIN32
#define Rectangle LCC_Rectangle
#include <Windows.h>
#undef Rectangle

const char *GetHWID(void)
{
	int   systemInfoSum         = 0;
	char  hwId[MAX_TOKEN_CHARS] = "\0";
	char  rootdrive[MAX_PATH]   = "\0";
	char  vsnc[MAX_PATH]        = "\0";
	DWORD vsn;

	SYSTEM_INFO systemInfo;
	GetSystemInfo(&systemInfo);

	// Random data from processor
	systemInfoSum = systemInfo.dwProcessorType +
	                systemInfo.wProcessorLevel + systemInfo.wProcessorArchitecture;

	_itoa(systemInfoSum, hwId, 10);
	// volume serial number
	GetEnvironmentVariable("HOMEDRIVE", rootdrive, sizeof(rootdrive));
	Q_strcat(rootdrive, sizeof(rootdrive), "\\");

	if (GetVolumeInformation(rootdrive, 0, 0, &vsn, 0, 0, 0, 0) == 0)
	{
		// Failed to get volume info
		Q_strcat(vsnc, sizeof(vsnc), "failed");
	}

	_itoa(vsn, vsnc, 10);

	Q_strcat(hwId, sizeof(hwId), vsnc);

	return G_SHA1(hwId);
}

/*
 * Doesn't really belong here, but didn't feel like including Windows.h
 * again.
 */

void CG_Minimize_f(void)
{
	HWND wnd = GetForegroundWindow();
	if (wnd)
	{
		ShowWindow(wnd, SW_MINIMIZE);
	}
}

#endif
