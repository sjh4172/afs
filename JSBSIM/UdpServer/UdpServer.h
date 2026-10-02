#pragma once

#ifdef CREATEDLL_EXPORTS
#define UDP_SERVER_DLL __declspec(dllexport)
#else
#define UDP_SERVER_DLL __declspec(dllimport)
#endif
class UdpServer
{
public:
	UdpServer();
	~UdpServer();
	
};

extern "C" UDP_SERVER_DLL int recvMessage();
