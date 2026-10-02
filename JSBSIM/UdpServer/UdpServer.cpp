#include "UdpServer.h"


UdpServer::UdpServer()
{
}


UdpServer::~UdpServer()
{
}

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#pragma comment(lib, "ws2_32.lib")

int recvMessage()
{
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		std::cerr << "WSAStartup failed\n"; return 1;
	}

	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		std::cerr << "socket failed: " << WSAGetLastError() << "\n";
		WSACleanup(); return 1;
	}

	// 재실행 편의를 위해(선택)
	BOOL yes = TRUE;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

	// ★ 모든 NIC에서 수신 (중요: 127.0.0.1 금지)
	sockaddr_in me{};
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = INADDR_ANY;           // 0.0.0.0 → 외부/내부 모두 수신
	me.sin_port = htons(20010);                // 상대가 보내는 포트와 일치시켜야 함

	if (bind(s, (sockaddr*)&me, sizeof(me)) == SOCKET_ERROR) {
		std::cerr << "bind failed: " << WSAGetLastError() << "\n";
		closesocket(s); WSACleanup(); return 1;
	}

	char buf[1500];
	sockaddr_in from{}; int fromlen = sizeof(from);

	while (true) {
		int n = recvfrom(s, buf, sizeof(buf)-1, 0, (sockaddr*)&from, &fromlen);
		if (n == SOCKET_ERROR) {
			std::cerr << "recvfrom failed: " << WSAGetLastError() << "\n";
			continue;
		}
		buf[n] = 0;

		char ip[64];
		inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
		std::cout << "[" << ip << ":" << ntohs(from.sin_port) << "] "
			<< n << " bytes : " << buf << "\n";

		// 에코 응답(선택)
		int sent = sendto(s, buf, n, 0, (sockaddr*)&from, fromlen);
		if (sent == SOCKET_ERROR) {
			std::cerr << "sendto failed: " << WSAGetLastError() << "\n";
		}
	}

	closesocket(s); WSACleanup();
	return 0;
}