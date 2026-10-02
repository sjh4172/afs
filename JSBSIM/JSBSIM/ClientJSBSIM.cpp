#include "ClientJSBSIM.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdio>

#pragma comment(lib, "ws2_32.lib")

// --- 내부 상태 ---
static bool     g_inited = false;
static SOCKET   g_sock = INVALID_SOCKET;
static sockaddr_in g_to = {};
static WSADATA  g_wsa = {};

bool SendClient_Init(const char* ip, unsigned short port)
{
	if (g_inited) return true;

	if (WSAStartup(MAKEWORD(2, 2), &g_wsa) != 0) {
		std::fprintf(stderr, "[SendClient] WSAStartup failed\n");
		return false;
	}

	g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (g_sock == INVALID_SOCKET) {
		std::fprintf(stderr, "[SendClient] socket() failed: %d\n", WSAGetLastError());
		WSACleanup();
		return false;
	}

	// 목적지 설정
	std::memset(&g_to, 0, sizeof(g_to));
	g_to.sin_family = AF_INET;
	g_to.sin_port = htons(port);

	if (inet_pton(AF_INET, ip, &g_to.sin_addr) != 1) {
		std::fprintf(stderr, "[SendClient] inet_pton failed for %s\n", ip);
		closesocket(g_sock);
		WSACleanup();
		return false;
	}

	g_inited = true;
	return true;
}

// 필요하면 여기서 엔디안 변환 추가 (예시용 no-op)
static inline void to_network(RullPoseMsg& /*m*/) {
	// uint16_t/uint64_t 필드에 대해 htons/htonl/사용자 htonll 적용.
	// 현재는 같은 머신/테스트 기준으로 생략.
}

bool sendMessage()
{
	if (!g_inited || g_sock == INVALID_SOCKET) {
		std::fprintf(stderr, "[SendClient] not initialized\n");
		return false;
	}

	to_network(rullPose);

	const int len = static_cast<int>(sizeof(RullPoseMsg));

	const int sent = sendto(g_sock,
		reinterpret_cast<const char*>(&rullPose),
		len,
		0,
		reinterpret_cast<sockaddr*>(&g_to),
		sizeof(g_to));

	if (sent != len) {
		std::fprintf(stderr, "[SendClient] sendto error: %d (sent=%d, expect=%d)\n",
			WSAGetLastError(), sent, len);
		return false;
	}
	return true;
}

void SendClient_Close()
{
	if (g_sock != INVALID_SOCKET) {
		closesocket(g_sock);
		g_sock = INVALID_SOCKET;
	}
	if (g_inited) {
		WSACleanup();
		g_inited = false;
	}
}
