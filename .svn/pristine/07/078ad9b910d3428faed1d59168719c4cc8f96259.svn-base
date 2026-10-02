#pragma once
#include <atomic>
#include "CommJSBSIM.h"

#ifdef CREATEDLL_EXPORTS
#define UDP_SERVER_DLL __declspec(dllexport)
#else
#define UDP_SERVER_DLL __declspec(dllimport)
#endif
class Test
{
public:
	Test();
	~Test();
};

extern std::atomic<int> g_run;   // 1: step 진행, 0: 일시정지
extern std::atomic<int> g_quit;  // 1: 완전 종료 요청

extern "C" UDP_SERVER_DLL int  getRullRun();                   // 현재 run 값
extern "C" UDP_SERVER_DLL void setRullRun(int v);              // run 설정 (START/STOP)
extern "C" UDP_SERVER_DLL int  getRullQuit();
extern "C" UDP_SERVER_DLL void setRullQuit(int v);
extern "C" UDP_SERVER_DLL int  rullBase();                 // 워커 스레드 함수 (한 번만 생성)
extern "C" UDP_SERVER_DLL RullPoseMsg getRullPose();
