/**
********************************************************************************
* Copyright (c) 2024 APPIA
* @brief   Header File\n
* @file
* @version 1.0
* @date    2024/04/12
* @author  Ho Chae Jeong (JHC)
********************************************************************************
* @remark  [Version History]\n
********************************************************************************
*/


/*
********************************************************************************
*                       DEFINITIONS & MACROS
********************************************************************************
*/
#pragma once
#pragma comment(lib,"winmm.lib")


/*
********************************************************************************
*                       INCLUDES
********************************************************************************
*/
#include <Windows.h>
#include <joystickapi.h>



/*
********************************************************************************
*                       DEFINITIONS & MACROS
********************************************************************************
*/
#define STICK_PRODUCT_ID		(1026)
#define THROTTLE_PRODUCT_ID		(1028)
#define RUDDER_PRODUCT_ID		(46735)

#ifndef _WIN32
#define DECLARE_DLL
#else
#ifdef EXPORT_MODEL_DLL
#define DECLARE_DLL __declspec(dllexport) 
#else
#define DECLARE_DLL __declspec(dllimport) 
#endif
#endif

/*
********************************************************************************
*                       DATA TYPES & STRUCTURES
********************************************************************************
*/
typedef enum{
	STICK,
	THROTTLE,
	RUDDER,
	JOYSTICK_COUNT
} JOYSTICKTYPE;

typedef struct JoyData{
	int Xpos;
	int Ypos;
	int Zpos;
	unsigned char Button;
} StickData;


typedef struct {
	double pitchCmd;
	double rollCmd;
	double throttleCmd;
	double rudderCmd;
} StickCmd;

typedef struct{
	int mode;
	int x;
	int y;
	int throttle;
	int rudder;
} StickInput;

#ifdef __cplusplus
extern "C" {
#endif


	/*
	********************************************************************************
	*                       VARIABLE EXTERNALS
	********************************************************************************
	*/
	extern DECLARE_DLL StickInput	stickInput;
	extern DECLARE_DLL StickCmd		stickCmd;


	/*
	********************************************************************************
	*                       FUNCTION PROTOTYPES
	********************************************************************************
	*/
	extern void InitStick();
	extern void StepStick();
	extern void StickScale();

#ifdef __cplusplus
}
#endif

