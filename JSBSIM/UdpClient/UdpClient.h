#pragma once

#ifdef CREATEDLL_EXPORTS
#define UDP_CLIENT_DLL __declspec(dllexport)
#else
#define UDP_CLIENT_DLL __declspec(dllimport)
#endif

class UdpClient
{
public:
	UdpClient();
	~UdpClient();
};

extern "C" UDP_CLIENT_DLL int sendMessage();