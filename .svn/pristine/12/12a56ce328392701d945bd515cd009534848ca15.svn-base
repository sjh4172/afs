#pragma once
class Server
{
public:
	Server();
	~Server();
};


#include <cstdint>
// Send Struct
struct PoseMsg
{
	uint16_t header;
	uint64_t Latitude;
	uint64_t Longitude;
	uint16_t Altitude;
	uint16_t AGL;
	uint16_t VelocityX, VelocityY, VelocityZ;
	uint16_t Azimuth;
	uint16_t Roll, Pitch;
	uint16_t PresentTrueHeading;
	uint16_t AccelerationX, AccelerationY, AccelerationZ;
	uint16_t CurrentG;
	uint16_t FPMHorizontal;
	uint16_t FPMVertical;
	uint64_t EnemyLatitude;
	uint64_t EnemyLongitude;
	uint16_t EnemyAltitude;
	uint16_t EnemyAzimuth;
	uint16_t EnemyRoll, EnemyPitch;
	uint16_t RESERVED1, RESERVED2, RESERVED3;
};
