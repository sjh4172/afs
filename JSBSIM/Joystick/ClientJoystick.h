#pragma once
#include <cstdint>
#include "CommJoystcik.h"

// stickPose는 다른 곳(CommJSBSIM.cpp 등)에서 정의되어 있다고 가정
extern StickPoseMsg stickPose;

bool SendClient_Init(const char* ip, unsigned short port);
bool sendMessage();           // 루프에서 계속 호출(1회 송신)
void SendClient_Close();
