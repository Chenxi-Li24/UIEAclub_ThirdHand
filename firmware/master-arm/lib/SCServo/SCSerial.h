/*
 * SCSerial.h
 * 飞特串行舵机硬件接口层程序
 */

#ifndef _SCSERIAL_H
#define _SCSERIAL_H

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif

#include "SCS.h"

class SCSerial : public SCS
{
public:
	SCSerial();
	SCSerial(u8 End);
	SCSerial(u8 End, u8 Level);

protected:
	int writeSCS(unsigned char *nDat, int nLen);
	int readSCS(unsigned char *nDat, int nLen);
	int readSCS(unsigned char *nDat, int nLen, unsigned long TimeOut);
	int writeSCS(unsigned char bDat);
	void rFlushSCS();
	void wFlushSCS();
public:
	unsigned long IOTimeOut;
	HardwareSerial *pSerial;
};

#endif
