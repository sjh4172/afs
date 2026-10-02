#pragma once
#include <atomic>
#include "CommJSBSIM.h"

#ifdef CREATEDLL_EXPORTS
#define UDP_SERVER_DLL __declspec(dllexport)
#else
#define UDP_SERVER_DLL __declspec(dllimport)
#endif

class Waypoint
{
public:
	Waypoint();
	~Waypoint();
};

extern "C" UDP_SERVER_DLL int waypointBase();                 // 워커 스레드 함수 (한 번만 생성)
