#include "ServerJoystick.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <cstring>
#include <cstdint>
#pragma comment(lib, "ws2_32.lib")

ServerJoystick::ServerJoystick()
{
}


ServerJoystick::~ServerJoystick()
{
}

unityMsg stickMsg{};


//static_assert(sizeof(unityMsg) == 64, "unityMsg must be 64 bytes.");

// 보낸 데이터가 네트워크 바이트오더(빅엔디언)인지 여부.
// Unity/C#에서 별도 변환 없이 short를 Write 했다면 보통 리틀엔디언 → 0으로 두세요.
#define DATA_IS_NETWORK_ORDER 0

#if DATA_IS_NETWORK_ORDER
static inline int16_t ntoh16_i(int16_t v)
{
	// ntohs는 unsigned short 기준이므로 변환 후 다시 int16_t로 캐스팅
	const uint16_t u = static_cast<uint16_t>(v);
	const uint16_t h = ntohs(u);
	return static_cast<int16_t>(h);
}

// 구조체 내 모든 16비트 필드 변환
static inline void ntoh_unity_inplace(unityMsg& m)
{
	int16_t* p = &m.header;
	for (int i = 0; i < 32; ++i) { // 32개 필드
		p[i] = ntoh16_i(p[i]);
	}
}
#endif

int recvJoystickMessage()
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

	BOOL yes = TRUE;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

	sockaddr_in me{};
	me.sin_family = AF_INET;
	me.sin_addr.s_addr = INADDR_ANY;
	me.sin_port = htons(20010);

	if (bind(s, (sockaddr*)&me, sizeof(me)) == SOCKET_ERROR) {
		std::cerr << "bind failed: " << WSAGetLastError() << "\n";
		closesocket(s); WSACleanup(); return 1;
	}

	char buf[1500];
	sockaddr_in from{}; int fromlen = sizeof(from);

	while (true) {
		fromlen = sizeof(from); // 매번 초기화
		int n = recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &fromlen);
		if (n == SOCKET_ERROR) {
			std::cerr << "recvfrom failed: " << WSAGetLastError() << "\n";
			continue;
		}

		// 1) 길이 확인 (정확히 64바이트여야 안전)
		if (n < (int)sizeof(unityMsg)) {
			std::cerr << "packet too short: " << n << " bytes (need 64)\n";
			continue;
		}

		// 2) 구조체로 복사
		std::memcpy(&stickMsg, buf, sizeof(unityMsg));

		// 3) 필요 시 바이트오더 보정
#if DATA_IS_NETWORK_ORDER
		ntoh_unity_inplace(stickMsg);
#endif
		// 4) 사용 예시: 송신자 IP/포트 + 주요 필드 출력
		char ip[64];
		inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));


		// 필요 시 에코 응답
		//int sent = sendto(s, buf, sizeof(unityMsg), 0, (sockaddr*)&from, fromlen);
		//if (sent == SOCKET_ERROR) {
		//	std::cerr << "sendto failed: " << WSAGetLastError() << "\n";
		//}
	}

	closesocket(s); WSACleanup();
	return 0;
}
