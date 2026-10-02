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
*                       INCLUDES
********************************************************************************
*/
#pragma once
#include "STICK.h"
#include <stdio.h>

/*
********************************************************************************
*                       LOCAL DEFINITIONS & MACROS
********************************************************************************
*/
#define MIN(a, b) (a<b ? a : b)
#define MAX(a, b) (a>b ? a : b)

/*
********************************************************************************
*                       LOCAL Variable
********************************************************************************
*/
int			ID[JOYSTICK_COUNT];;
JOYINFOEX	joyData[JOYSTICK_COUNT];
StickData	data[JOYSTICK_COUNT];
unsigned short m_msg[14];

StickInput	stickInput;
StickCmd	stickCmd;


/**
********************************************************************************
* @brief    Joystick 초기화
* @param    None
* @return   None
* @remark
********************************************************************************
*/
void InitStick(){

	
	memset(&stickCmd, 0x00, sizeof(stickCmd));
	
	for (int i = 0; i < JOYSTICK_COUNT; i++){
		ID[i] = -1;
		memset(&data[i], 0x00, sizeof(data[i]));
	}

	short Devnum;
	if ((Devnum = joyGetNumDevs()) == 0){
		printf("No Device!\n");
		return;
	}

	
	for (int i = 0; i < Devnum; i++){
		JOYCAPS joyInfo;

		joyGetDevCaps(i, &joyInfo, sizeof(JOYCAPS));
		if (joyInfo.wPid == STICK_PRODUCT_ID){
			ID[STICK] = i;
		}
		else if (joyInfo.wPid == THROTTLE_PRODUCT_ID){
			ID[THROTTLE] = i;
		}
		else if (joyInfo.wPid == RUDDER_PRODUCT_ID){
			ID[RUDDER] = i;
		}
	}


	for (int i = 0; i < 3; i++){
		joyData[i].dwSize = sizeof(JOYINFOEX);
		joyData[i].dwFlags = JOY_RETURNALL;
	}
}


/**
********************************************************************************
* @brief    Joystick 값 얻기
* @param    None
* @return   None
* @remark
********************************************************************************
*/
void StepStick(){
	if (stickInput.mode == 0)
	{
		// Joystick mode
		for (int i = 0; i<JOYSTICK_COUNT; i++){
			if (ID[i] >= 0 && ID[i] <16){
				joyGetPosEx(ID[i], &joyData[i]);
			}
		}
		
		/*
		printf("x : %d, y  : %d, z : %d, r : %d, u :%d, d : %d\n",
			joyData[STICK].dwXpos,
			joyData[STICK].dwYpos,
			joyData[STICK].dwZpos,
			joyData[STICK].dwRpos,
			joyData[STICK].dwUpos,
			joyData[STICK].dwVpos);
		*/
		//printf("throttle : %d\n", joyData[THROTTLE].dwXpos);

		data[STICK].Xpos = joyData[STICK].dwXpos;
		data[STICK].Ypos = joyData[STICK].dwYpos;
		data[STICK].Button = joyData[STICK].dwButtons & JOY_BUTTON1;
		data[THROTTLE].Zpos = 65534 - joyData[THROTTLE].dwXpos;
		//data[RUDDER].Xpos = 65534 - joyData[RUDDER].dwXpos;
		//data[RUDDER].Ypos = 65534 - joyData[RUDDER].dwYpos;
		data[RUDDER].Zpos = 32767 - joyData[STICK].dwRpos;
	}
	else
	{
		// Keyboard mode
		data[STICK].Xpos = stickInput.x;
		data[STICK].Ypos = stickInput.y;
		data[THROTTLE].Zpos = stickInput.throttle;
		data[RUDDER].Zpos = 32767 - stickInput.rudder;;	
	}

	//----------------------------------------------
	StickScale();
	//----------------------------------------------

	/*
	printf("%lf, %lf, %lf \n", 
		stickCmd.rollCmd,
		stickCmd.pitchCmd,
		stickCmd.rollCmd, 
		stickCmd.throttleCmd);
	*/
}


void  StickScale(){
	int PortID = 1;

	unsigned short pitchCmdTemp = data[STICK].Ypos;
	unsigned short rollCmdTemp = data[STICK].Xpos;
	unsigned short throttleCmdTemp = data[THROTTLE].Zpos;
	unsigned short rudder1CmdTemp = data[RUDDER].Xpos;
	unsigned short rudder2CmdTemp = data[RUDDER].Ypos;
	unsigned short rudder3CmdTemp = data[RUDDER].Zpos;

	// 현재 범위
	int minValue = 0;
	int maxValue = 65535;
	int adjust = 0;

	// 원하는 새로운 범위
	double newMinValue = -1.0;
	double newMaxValue = 1.0;

	
	stickCmd.rollCmd = ((double)((short)32767 - rollCmdTemp)) / 32767.0 * -1.0;
	stickCmd.pitchCmd = ((double)((short)32767 - pitchCmdTemp)) / 32767.0 * -1.0;

	minValue = -32767;
	maxValue = 32767;
	stickCmd.rudderCmd = ((double)((short)rudder3CmdTemp - minValue) / (maxValue - minValue)) * (newMaxValue - newMinValue) + newMinValue;
	stickCmd.rudderCmd = 0.0;// stickCmd.rudderCmd * -1;

	minValue = 0;
	maxValue = 65535;
	newMinValue = 0.0;
	newMaxValue = 1.0;
	stickCmd.throttleCmd = ((double)(throttleCmdTemp - minValue) / (maxValue - minValue)) * (newMaxValue - newMinValue) + newMinValue;


	stickCmd.rollCmd = MIN(MAX(stickCmd.rollCmd, -1.0), 1.0);
	stickCmd.pitchCmd = MIN(MAX(stickCmd.pitchCmd, -1.0), 1.0);
	stickCmd.rudderCmd = MIN(MAX(stickCmd.rudderCmd, -1.0), 1.0);
	stickCmd.throttleCmd = MIN(MAX(stickCmd.throttleCmd, 0.0), 1.0);
}
