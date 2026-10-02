#include "Server.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <mmsystem.h>
#pragma comment(lib, "ws2_32.lib")

#include "../UdpServer/UdpServer.h"
#pragma comment(lib, "../Debug/UdpServer.lib")

#include "../UdpClient/UdpClient.h"
#pragma comment(lib, "../Debug/UdpClient.lib")

#include "../JSBSIM/Test.h"
#include "../JSBSIM/Waypoint.h"
#pragma comment(lib, "../Debug/JSBSim.lib")

#include "../Joystick/StickTest.h"
#include "../Joystick/ServerJoystick.h"
#pragma comment(lib, "../Debug/Joystick.lib")



static std::thread       g_thRull;
static std::thread       g_thStick;
static std::atomic<bool> g_aliveRull(false);
static std::atomic<bool> g_aliveStick(false);
static std::thread recvJoystick;

RullPoseMsg _rullPose;
StickPoseMsg _stickPose;

Server::Server()
{
}


Server::~Server()
{
}
static inline void setRunAll(int v)   { setRullRun(v);  setStickRun(v); }
static inline void setQuitAll(int v)  { setRullQuit(v); setStickQuit(v); }

void getPoseMsg()
{
	while (1)
	{
		Sleep(1000);
		_rullPose = getRullPose();
		_stickPose = getStickPose();
	}
}

int main()
{
	std::thread a(getPoseMsg);
	// --- WinSock 초기화 1회 ---
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
		std::printf("WSAStartup failed\n");
		return 1;
	}

	// --- 소켓 생성/바인드 ---
	SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET) {
		std::printf("socket failed: %d\n", WSAGetLastError());
		WSACleanup(); return 1;
	}

	sockaddr_in me;
	std::memset(&me, 0, sizeof(me));
	me.sin_family = AF_INET;
	me.sin_port = htons(50000);
	me.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0
	int namelen = static_cast<int>(sizeof(me));
	if (::bind(s, reinterpret_cast<const sockaddr*>(&me), namelen) == SOCKET_ERROR) {
		const int wsaErr = WSAGetLastError();
		std::printf("bind failed: %d\n", WSAGetLastError());
		closesocket(s); WSACleanup(); return 1;
	}
	recvJoystick = std::thread(recvJoystickMessage);

	std::printf("UDP listening on 0.0.0.0:50000\n");
	// --- 수신 루프 ---
	for (;;) {
		char buf[1500];
		sockaddr_in from;
		int fromlen = sizeof(from);

		int n = recvfrom(s, buf, sizeof(buf)-1, 0,
			reinterpret_cast<sockaddr*>(&from), &fromlen);
		if (n <= 0) continue;

		buf[n] = '\0'; // 문자열로 다루려면 반드시 널 종료

		std::printf("recv %d bytes: %s\n", n, buf);

		// 명령 처리
		if (std::strcmp(buf, "START") == 0) {
			// 재시작 가능하게 QUIT 플래그 리셋
			setQuitAll(0);

			// ★ rullBase 스레드가 없으면 생성
			if (!g_aliveRull.load()) {
				g_thRull = std::thread(waypointBase);
				g_aliveRull.store(true);
				std::printf("[rull] thread CREATED\n");
			}
			// ★ stickBase 스레드가 없으면 생성
			if (!g_aliveStick.load()) {
				g_thStick = std::thread(stickBase);
				g_aliveStick.store(true);
				std::printf("[stick] thread CREATED\n");
			}

			// 둘 다 실행 상태로 전환
			setRunAll(1);
			std::printf("worker: START/RESUME (both)\n");
		}
		else if (std::strcmp(buf, "STOP") == 0) {
			// 둘 다 일시정지 (스레드는 살아있음)
			setRunAll(0);
			std::printf("worker: STOP (paused, both)\n");
		}
		else if (std::strcmp(buf, "QUIT") == 0 || std::strcmp(buf, "INIT") == 0) {
			// 완전 종료: run=0, quit=1 → join 두 개 모두
			setRunAll(0);
			setQuitAll(1);

			if (g_aliveRull.load() && g_thRull.joinable()) { g_thRull.join(); g_aliveRull.store(false); }
			if (g_aliveStick.load() && g_thStick.joinable()) { g_thStick.join(); g_aliveStick.store(false); }
			std::printf("worker: QUIT (joined both)\n");
		} 
		else {
			std::printf("unknown cmd: '%s'\n", buf);
		}
		// 그 외 명령은 무시 또는 로그
	}

	// (도달하지 않지만, 정리 코드 예시)
	//if (g_worker_running.load()) {
	//	g_worker_running.store(false);
	//	if (g_worker.joinable()) g_worker.join();
	//}
	recvJoystick.join();
	a.join();
	closesocket(s);
	WSACleanup();
	return 0;
	
	//recvMessage();
}
//std::thread rullBase(rullBase);
//std::thread stickBase(stickBase);
//rullBase.join();
//stickBase.join();