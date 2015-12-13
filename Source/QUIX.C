/***************************************************************************\
* Module Name: quix.c
\***************************************************************************/

#define DELAYTIME 20
#define MAXSPEED 3277
#define COLORSPEED 1000
#define SHAPESPEED 6666

#define INCL_DOSPROCESS
#define INCL_WINTIMER

#include <os2.h>
#include <stdlib.h>

#define ONELINE 0
#define TWOLINES 1
#define BOX 2
#define STAR 3
#define TRIANGLE 4
#define DIAMOND 5
#define VEE 6
#define PLUS 7
#define HOURGLASS 8
#define CIRCLE 9

typedef struct {
	HAB deskpichab;
	HWND screenhwnd;
	RECTL screenrectl;
	ULONG volatile closesemaphore;
	HMODULE thismodule;
} SAVERBLOCK;

typedef struct {
	POINTL pt1,pt2;
	ULONG color;
	int shape;
} FIFO;

void far pascal quixdaemon(void);
static void near plotshape(POINTL, POINTL, int, long, HPS);

extern int near parameter;
extern SAVERBLOCK near saverinfo;

void far pascal quixdaemon()

{
	int i,fifolen;
	HAB dhab;
	HPS hps;
	SEL fifosel;
	FIFO *fifo;
	ULONG time;
	int ffpnt = 0;
	static int count = 0;
	static int scount = 0;
	static int cshape = ONELINE;
	static ULONG ccol = CLR_RED;
	static POINTL mypt[2] = {-1, -1, -1, -1};
	static POINTL p[2] = {0, 0, 0, 0};
	static ULONG col[15] = {CLR_WHITE,CLR_RED,CLR_PINK,CLR_GREEN,
		CLR_CYAN,CLR_YELLOW,CLR_DARKGRAY,CLR_DARKBLUE,CLR_DARKRED,CLR_DARKPINK,
		CLR_DARKGREEN,CLR_DARKCYAN,CLR_BROWN,CLR_PALEGRAY,CLR_BLUE};

	dhab = WinInitialize(0);
	fifolen = parameter;
	DosAllocSeg(sizeof(FIFO)*fifolen,&fifosel,SEG_NONSHARED);
	fifo = MAKEP(fifosel,0);
	for (i = 0; i < fifolen; i++) fifo[i].pt1.x = -1;

	hps = WinGetPS(saverinfo.screenhwnd);
	while (!saverinfo.closesemaphore) {
		time = WinGetCurrentTime(dhab);
		for (i = 0; i < 2; i++) {
			mypt[i].x += p[i].x;
			mypt[i].y += p[i].y;
			if (mypt[i].x < saverinfo.screenrectl.xLeft) {
				mypt[i].x = saverinfo.screenrectl.xLeft;
				p[i].x = 1+rand()/MAXSPEED;
			}
			else if (mypt[i].x >= saverinfo.screenrectl.xRight) {
				mypt[i].x = saverinfo.screenrectl.xRight-1;
				p[i].x = -1-rand()/MAXSPEED;
			}
			if (mypt[i].y < saverinfo.screenrectl.yBottom) {
				mypt[i].y = saverinfo.screenrectl.yBottom;
				p[i].y = 1+rand()/MAXSPEED;
			}
			else if (mypt[i].y >= saverinfo.screenrectl.yTop) {
				mypt[i].y = saverinfo.screenrectl.yTop-1;
				p[i].y = -1-rand()/MAXSPEED;
			}
		}
		plotshape(mypt[0],mypt[1],cshape,ccol,hps);
		if (fifo[ffpnt].pt1.x >= 0)
			plotshape(fifo[ffpnt].pt1,fifo[ffpnt].pt2,fifo[ffpnt].shape,
				CLR_BLACK,hps);
		fifo[ffpnt].pt1 = mypt[0];
		fifo[ffpnt].pt2 = mypt[1];
		fifo[ffpnt].color = ccol;
		fifo[ffpnt].shape = cshape;
		if (++ffpnt >= fifolen) ffpnt = 0;
		if (++count >= COLORSPEED) {
			count = 0;
			ccol = col[rand()/2185];
		}
		if (++scount >= SHAPESPEED ||
			(cshape == CIRCLE && scount >= SHAPESPEED/3)) {
			scount = 0;
			cshape = rand()/3450;
		}
		while (time+DELAYTIME > WinGetCurrentTime(dhab));
	}
	WinReleasePS(hps);
	DosFreeSeg(fifosel);
	WinTerminate(dhab);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo.closesemaphore);
}

static void near plotshape(mypt1,mypt2,shape,color,hps)
POINTL mypt1,mypt2;
int shape;
long color;
HPS hps;

{
	long radius_x,radius_y;
	POINTL tmppt1,tmppt2,tmppt3,tmppt4;

	GpiSetColor(hps,color);
	switch(shape) {
	case ONELINE:
		GpiMove(hps,&mypt1);
		GpiLine(hps,&mypt2);
		break;
	case TWOLINES:
		GpiMove(hps,&mypt1);
		GpiLine(hps,&mypt2);
		tmppt1.x = mypt1.x;
		tmppt1.y = mypt2.y;
		tmppt2.x = mypt2.x;
		tmppt2.y = mypt1.y;
		GpiMove(hps,&tmppt1);
		GpiLine(hps,&tmppt2);
		break;
	case BOX:
		GpiMove(hps,&mypt1);
		GpiBox(hps,DRO_OUTLINE,&mypt2,0L,0L);
		break;
	case CIRCLE:
		GpiMove(hps,&mypt1);
		GpiBox(hps,DRO_OUTLINE,&mypt2,
			labs(mypt2.x-mypt1.x),labs(mypt2.y-mypt1.y));
		break;
	case TRIANGLE:
		tmppt1.x = mypt1.x;
		tmppt1.y = mypt2.y;
		tmppt2.x = mypt1.x+mypt2.x>>1;
		tmppt2.y = mypt1.y;
		GpiMove(hps,&tmppt1);
		GpiLine(hps,&tmppt2);
		GpiLine(hps,&mypt2);
		GpiLine(hps,&tmppt1);
		break;
	case DIAMOND:
		tmppt1.x = tmppt3.x = mypt1.x+mypt2.x>>1;
		tmppt1.y = mypt1.y;
		tmppt2.x = mypt1.x;
		tmppt2.y = tmppt4.y = mypt1.y+mypt2.y>>1;
		tmppt3.y = mypt2.y;
		tmppt4.x = mypt2.x;
		GpiMove(hps,&tmppt1);
		GpiLine(hps,&tmppt2);
		GpiLine(hps,&tmppt3);
		GpiLine(hps,&tmppt4);
		GpiLine(hps,&tmppt1);
		break;
	case VEE:
		tmppt1.x = mypt1.x+mypt2.x>>1;
		tmppt1.y = mypt2.y;
		tmppt2.x = mypt2.x;
		tmppt2.y = mypt1.y;
		GpiMove(hps,&mypt1);
		GpiLine(hps,&tmppt1);
		GpiLine(hps,&tmppt2);
		break;
	case PLUS:
		tmppt1.x = tmppt3.x = mypt1.x+mypt2.x>>1;
		tmppt1.y = mypt1.y;
		tmppt2.x = mypt1.x;
		tmppt2.y = tmppt4.y = mypt1.y+mypt2.y>>1;
		tmppt3.y = mypt2.y;
		tmppt4.x = mypt2.x;
		GpiMove(hps,&tmppt1);
		GpiLine(hps,&tmppt3);
		GpiMove(hps,&tmppt2);
		GpiLine(hps,&tmppt4);
		break;
	case HOURGLASS:
		tmppt1.x = mypt1.x;
		tmppt1.y = mypt2.y;
		tmppt2.x = mypt2.x;
		tmppt2.y = mypt1.y;
		GpiMove(hps,&mypt1);
		GpiLine(hps,&mypt2);
		GpiLine(hps,&tmppt1);
		GpiLine(hps,&tmppt2);
		GpiLine(hps,&mypt1);
		break;
	case STAR:
		tmppt4.x = mypt1.x+mypt2.x>>1;
		tmppt4.y = mypt1.y+mypt2.y>>1;
		radius_x = mypt2.x-mypt1.x>>1;
		radius_y = mypt2.y-mypt1.y>>1;
		tmppt1.x = tmppt4.x;
		tmppt1.y = tmppt4.y+radius_y;
		GpiMove(hps,&tmppt1);
		tmppt2.x = tmppt4.x+(radius_x*-38521>>16);
		tmppt2.y = tmppt3.y = tmppt4.y+(radius_y*-53020>>16);
		GpiLine(hps,&tmppt2);
		tmppt2.x = tmppt4.x+(radius_x*62328>>16);
		tmppt2.y = tmppt4.y+(radius_y*20251>>16);
		GpiLine(hps,&tmppt2);
		tmppt2.x = tmppt4.x+(radius_x*-62328>>16);
		GpiLine(hps,&tmppt2);
		tmppt3.x = tmppt4.x+(radius_x*38521>>16);
		GpiLine(hps,&tmppt3);
		GpiLine(hps,&tmppt1);
	}
}
