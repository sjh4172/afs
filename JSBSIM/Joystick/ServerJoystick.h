#pragma once
#include <cstdint>  

#ifdef CREATEDLL_EXPORTS
#define UDP_SERVER_DLL __declspec(dllexport)
#else
#define UDP_SERVER_DLL __declspec(dllimport)
#endif

class ServerJoystick
{
public:
	ServerJoystick();
	~ServerJoystick();
};

#pragma pack(push, 1)
struct unityMsg
{
	int16_t header;
	int16_t throttleCmd;
	int16_t pitchCmd;
	int16_t rollCmd;
	int16_t rudderCmd;
	uint16_t attackFlag;
	uint16_t damageFlag;
	int16_t RESERVED1, RESERVED2, RESERVED3, RESERVED4, RESERVED5, RESERVED6, RESERVED7, RESERVED8, RESERVED9, RESERVED10;
	int16_t RESERVED11, RESERVED12, RESERVED13, RESERVED14, RESERVED15, RESERVED16, RESERVED17, RESERVED18, RESERVED19, RESERVED20;
	int16_t RESERVED21, RESERVED22, RESERVED23, RESERVED24, RESERVED25;
};
#pragma pack(pop)

extern "C" UDP_SERVER_DLL int recvJoystickMessage();
extern unityMsg stickMsg;
