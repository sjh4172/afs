#pragma once
#include <cstdint>
#include "FGFDMExec.h"
#include "ServerJoystick.h"
#include "simgear/misc/sg_path.hxx"

#pragma pack(push, 1)
struct StickPoseMsg
{
	int16_t header;
	int32_t Latitude;
	int32_t Longitude;
	int16_t Altitude;
	int16_t AGL;
	int16_t VelocityX, VelocityY, VelocityZ;
	int16_t Azimuth;
	int16_t Roll, Pitch;
	int16_t PresentTrueHeading;
	int16_t AccelerationX, AccelerationY, AccelerationZ;
	int16_t CurrentG;
	int16_t FPMHorizontal;
	int16_t FPMVertical;
	int32_t EnemyLatitude;
	int32_t EnemyLongitude;
	int16_t EnemyAltitude;
	int16_t EnemyAzimuth;
	int16_t EnemyRoll, EnemyPitch;
	int16_t Speed, EnemySpeed;
	int16_t stickHp, rullHp;
};
#pragma pack(pop)

void UpdatePoseFromJSBSim(JSBSim::FGFDMExec& fdm);

extern StickPoseMsg stickPose;
extern unityMsg stickMsg;
