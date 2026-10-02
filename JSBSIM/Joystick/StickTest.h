#pragma once
#include <atomic>
#include "CommJoystcik.h"

#ifdef CREATEDLL_EXPORTS
#define UDP_SERVER_DLL __declspec(dllexport)
#else
#define UDP_SERVER_DLL __declspec(dllimport)
#endif
class StickTest
{
public:
	StickTest();
	~StickTest();
};

extern std::atomic<int> g_run;   // 1: step 진행, 0: 일시정지
extern std::atomic<int> g_quit;  // 1: 완전 종료 요청

extern "C" UDP_SERVER_DLL int  getStickRun();                   // 현재 run 값
extern "C" UDP_SERVER_DLL void setStickRun(int v);              // run 설정 (START/STOP)
extern "C" UDP_SERVER_DLL int  getStickQuit();
extern "C" UDP_SERVER_DLL void setStickQuit(int v);
extern "C" UDP_SERVER_DLL int stickBase();
extern "C" UDP_SERVER_DLL StickPoseMsg getStickPose();

