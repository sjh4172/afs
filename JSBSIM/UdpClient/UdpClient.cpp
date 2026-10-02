#include "UdpClient.h"


UdpClient::UdpClient()
{
}


UdpClient::~UdpClient()
{
}

#include <iostream>
#include <stdio.h>
#include <winsock2.h>     // 윈속 헤더 포함 
#include <windows.h>
#include <chrono>
#include <thread>
#include <ws2tcpip.h>     // inet_pton()

#pragma comment (lib,"ws2_32.lib") // 윈속 라이브러리 링크

#define BUFFER_SIZE 1024  // 버퍼 사이즈

using namespace std;

int sendMessage()
{
	WSADATA wsaData;           // 윈속 데이터 구조체
	SOCKET ClientSocket;       // 소켓 선언
	SOCKADDR_IN ToServer;      // 서버로 보내는 주소정보 구조체
	SOCKADDR_IN FromServer;    // 서버에서 받는 주소정보 구조체

	int Send_Size;
	int FromServer_Size;
	int Recv_Size;
	ULONG ServerPort = 61557;  // 서버 포트번호

	using clock = std::chrono::steady_clock;
	auto period = std::chrono::duration<double>(1.0 / 1); // 1 Hz
	auto next = clock::now();

	double dd = 100.12;
	char Buffer[BUFFER_SIZE] = {};
	sprintf_s(Buffer, "STOP,%.2lf\n", dd);

	if (WSAStartup(0x202, &wsaData) == SOCKET_ERROR) {
		cout << "WinSock 초기화부분에서 문제 발생 " << endl;
		WSACleanup();
		exit(0);
	}

	memset(&ToServer, 0, sizeof(ToServer));
	memset(&FromServer, 0, sizeof(FromServer));

	ToServer.sin_family = AF_INET;
	ToServer.sin_addr.s_addr = inet_addr("192.253.0.33");
	ToServer.sin_port = htons(ServerPort); // 포트번호

	ClientSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (ClientSocket == INVALID_SOCKET) {
		cout << "소켓을 생성할수 없습니다." << endl;
		closesocket(ClientSocket);
		WSACleanup();
		exit(0);
	}

	while (1) {
		next += std::chrono::duration_cast<clock::duration>(period);

		// 패킷 송신
		Send_Size = sendto(ClientSocket,
			Buffer,
			BUFFER_SIZE,
			0,
			(struct sockaddr*)&ToServer,
			sizeof(ToServer));

		printf("%s\n", Buffer);

		// 패킷송신시 에러처리
		if (Send_Size != BUFFER_SIZE) {
			cout << "sendto() error!" << endl;
			exit(0);
		}

		std::this_thread::sleep_until(next); // 다음 주기까지 대기
	}

	FromServer_Size = sizeof(FromServer);
	memset(Buffer, 0, BUFFER_SIZE);

	// 패킷 수신
	Recv_Size = recvfrom(ClientSocket,
		Buffer,
		BUFFER_SIZE,
		0,
		(struct sockaddr*)&FromServer,
		&FromServer_Size);

	// 패킷수신시 에러처리
	if (Recv_Size < 0) {
		cout << "recvfrom() error!" << endl;
		exit(0);
	}

	// 수신한 패킷에서 IP / Data 출력
	cout << "Recv from " << inet_ntoa(FromServer.sin_addr) << endl;
	cout << "Data " << Buffer << endl;

	closesocket(ClientSocket); // 소켓 닫기
	WSACleanup();
}