/*
   clockp.c -- Paints the clock

   Created by Microsoft Corporation, 1989

   Modified by J. Ridges, 1990
*/

#define CLR_FACE CLR_CYAN
#define CLR_TICKS CLR_DARKCYAN
#define CLR_HOURHAND CLR_PINK
#define CLR_MINUTEHAND CLR_DARKPINK

#define HT_HOUR 1
#define HT_MINUTE 2
#define HT_SECOND 3

#define INCL_GPIBITMAPS
#define INCL_GPIPATHS
#define INCL_GPIPRIMITIVES
#define INCL_GPITRANSFORMS

#include <os2.h>

typedef struct {
	BOOL enabled[5];
	int size;
} PROFILEREC;

FIXED far pascal FixMul(FIXED, FIXED);
VOID paintclock(HPS,BOOL);
static VOID near ClkDrawHand(HPS,SHORT,SHORT);
static VOID near ClkDrawFace(HPS);
static VOID near ClkDrawTicks(HPS);
static FIXED near fixsin(FIXED);

extern PROFILEREC near profile;

static MATRIXLF matlfModel = {
	MAKEFIXED(1,0),MAKEFIXED(0,0),0L,
	MAKEFIXED(0,0),MAKEFIXED(1,0),0L,
	0L,0L,1L
};
static FIXED fxSin[60] = {
	0x00000000, 0x00001ac2, 0x00003539, 0x00004f1b, 0x0000681f, 0x00007fff,
	0x00009679, 0x0000ab4c, 0x0000be3e, 0x0000cf1b, 0x0000ddb3, 0x0000e9de,
	0x0000f378, 0x0000fa67, 0x0000fe98, 0x0000ffff, 0x0000fe98, 0x0000fa67,
	0x0000f378, 0x0000e9de, 0x0000ddb3, 0x0000cf1b, 0x0000be3e, 0x0000ab4c,
	0x00009679, 0x00008000, 0x00006820, 0x00004f1b, 0x00003539, 0x00001ac2,
	0x00000000, 0xffffe53e, 0xffffcac7, 0xffffb0e5, 0xffff97e1, 0xffff8001,
	0xffff6988, 0xffff54b5, 0xffff41c2, 0xffff30e5, 0xffff224d, 0xffff1622,
	0xffff0c88, 0xffff0599, 0xffff0168, 0xffff0001, 0xffff0167, 0xffff0599,
	0xffff0c88, 0xffff1622, 0xffff224d, 0xffff30e5, 0xffff41c2, 0xffff54b4,
	0xffff6987, 0xffff8000, 0xffff97e0, 0xffffb0e4, 0xffffcac6, 0xffffe53e
};

VOID paintclock(HPS hps,BOOL first)
{
	DATETIME dtNew ;
	static DATETIME dt;

	if (first) {
		dt.minutes = 60;
		ClkDrawTicks(hps);
	}
	DosGetDateTime(&dtNew);
	if (dtNew.minutes != dt.minutes) {
		ClkDrawFace(hps);
		ClkDrawHand(hps,HT_HOUR,dtNew.hours*60+dtNew.minutes);
		ClkDrawHand(hps,HT_MINUTE,dtNew.minutes);
	}
	else ClkDrawHand(hps,HT_SECOND,dt.seconds);
	ClkDrawHand(hps,HT_SECOND,dtNew.seconds);
	dt = dtNew ;
}

static VOID near ClkDrawHand(HPS hps,SHORT sHandType,SHORT sAngle)
{
	static POINTL aptlHour[] = { { 8, 0 }, { 0, 60 }, { -8, 0 },
		{ 0, -10 }, { 8, 0 } };
	static POINTL aptlMinute[] = { { 6 ,0 }, { 0, 80 }, { -6, 0 },
		{ 0, -15 }, { 6, 0 } };
	static POINTL aptlSecond[] = { { 0, -15 }, { 0, 85 } };

	if (sHandType == HT_HOUR) {
		matlfModel.fxM11 = matlfModel.fxM22 = fixsin((FIXED)sAngle+180);
		matlfModel.fxM12 = fixsin((FIXED)sAngle+360);
		matlfModel.fxM21 = fixsin((FIXED)sAngle);
	}
	else {
		matlfModel.fxM11 = matlfModel.fxM22 = fxSin[(sAngle+15)%60];
		matlfModel.fxM12 = fxSin[(sAngle+30)%60];
		matlfModel.fxM21 = fxSin[sAngle];
	}
	GpiSetModelTransformMatrix(hps,9L,&matlfModel,TRANSFORM_REPLACE);
	switch(sHandType) {
	case HT_HOUR:
		GpiSetColor(hps,CLR_HOURHAND);
		GpiBeginPath(hps,1L);
		GpiSetCurrentPosition(hps,aptlHour);
		GpiPolyLine(hps,(LONG)(sizeof(aptlHour)/sizeof(POINTL)),aptlHour);
		GpiEndPath(hps);
		GpiFillPath(hps,1L,FPATH_ALTERNATE);
		break;
	case HT_MINUTE:
		GpiSetColor(hps,CLR_MINUTEHAND);
		GpiBeginPath(hps,1L) ;
		GpiSetCurrentPosition(hps,aptlMinute);
		GpiPolyLine(hps,(LONG)(sizeof(aptlMinute)/sizeof(POINTL)),aptlMinute);
		GpiEndPath(hps);
		GpiFillPath(hps,1L,FPATH_ALTERNATE);
		break;
	case HT_SECOND:
		GpiSetMix(hps,profile.enabled[1] ? FM_INVERT : FM_LEAVEALONE);
		GpiSetCurrentPosition(hps,aptlSecond);
		GpiPolyLine(hps,(LONG)(sizeof(aptlSecond)/sizeof(POINTL)),aptlSecond);
		GpiSetMix(hps,FM_OVERPAINT);
	}
}

static VOID near ClkDrawFace(HPS hps)
{
	static POINTL ptlOrigin = { 0, 0 };

	GpiSetModelTransformMatrix(hps,0L,NULL,TRANSFORM_REPLACE);
	GpiSetColor(hps,profile.enabled[4] ? CLR_FACE : CLR_BLACK);
	GpiSetCurrentPosition(hps,&ptlOrigin);
	GpiFullArc(hps,DRO_FILL,MAKEFIXED(85+!profile.enabled[4],0));
}

static VOID near ClkDrawTicks(HPS hps)
{
	USHORT usAngle;
	static POINTL aptlMajorTick[] = { { -3, 94 }, { 3, 100 } };
	static POINTL aptlMinorTick = { 0, 94 };

	GpiSetColor(hps,CLR_TICKS);
	for (usAngle = 0; usAngle < 60; usAngle++) {
		matlfModel.fxM11 = matlfModel.fxM22 = fxSin[(usAngle+15)%60];
		matlfModel.fxM12 = fxSin[(usAngle+30)%60];
		matlfModel.fxM21 = fxSin[usAngle];
		GpiSetModelTransformMatrix(hps,9L,&matlfModel,TRANSFORM_REPLACE);
		if (profile.enabled[3] && !(usAngle%5)) {
			GpiSetCurrentPosition(hps,&aptlMajorTick[0]);
			GpiBox(hps,DRO_FILL,&aptlMajorTick[1],0L,0L);
		}
		else if (profile.enabled[2]) GpiSetPel(hps,&aptlMinorTick);
	}
}

static FIXED near fixsin(arg)
FIXED arg;
{
	int i;
	FIXED r,s;
	static FIXED c[] = { -5018418, 5347884, -2709368, 411775 };

	arg = FixMul(arg,5965232);
	arg -= arg&0xffff0000;
	if (arg > 49152L) arg -= 65536L;
	else if (arg > 16384L) arg = 32768L-arg;
	s = FixMul(arg,arg);
	r = 2602479;
	for (i = 0; i < 4; i++) r = c[i]+FixMul(s,r);
	return FixMul(arg,r);
}
