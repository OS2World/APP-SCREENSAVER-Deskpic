/***************************************************************************\
* Module Name: space.c
\***************************************************************************/

#define EYEPOINT 512
#define VANISHING 45000L
#define NEWSTARS 3

#define INCL_DOSPROCESS
#define INCL_GPIPRIMITIVES
#define INCL_GPIBITMAPS

#include <os2.h>
#include <stdlib.h>

typedef struct {
	HAB deskpichab;
	HWND screenhwnd;
	RECTL screenrectl;
	ULONG volatile closesemaphore;
	HMODULE thismodule;
} SAVERBLOCK;

typedef struct {
	long x,y,z,color;
	POINTL oldpoint;
} STAR;

FIXED far pascal FixMul(FIXED, FIXED);
void far pascal spacedaemon(void);

extern int near parameter;
extern SAVERBLOCK near saverinfo;

void far pascal spacedaemon()

{
	int i,extra,speed,maxstars;
	long xsize,ysize,xcenter,ycenter,x,y;
	HAB dhab;
	HPS hps;
	SEL starsel;
	STAR *star;

	dhab = WinInitialize(0);
	speed = parameter*16;
	maxstars = (int)((VANISHING/speed)*NEWSTARS)/2;
	DosAllocSeg(sizeof(STAR)*maxstars,&starsel,SEG_NONSHARED);
	star = MAKEP(starsel,0);
	xcenter = saverinfo.screenrectl.xRight+saverinfo.screenrectl.xLeft>>1;
	ycenter = saverinfo.screenrectl.yTop+saverinfo.screenrectl.yBottom>>1;
	xsize = ((saverinfo.screenrectl.xRight-saverinfo.screenrectl.xLeft>>1)+1)*
		VANISHING/EYEPOINT;
	ysize = ((saverinfo.screenrectl.yTop-saverinfo.screenrectl.yBottom>>1)+1)*
		VANISHING/EYEPOINT;
	extra = 0;
	hps = WinGetPS(saverinfo.screenhwnd);
	for (i = 0; i < maxstars; i++) star[i].z = -1;
	GpiSetMix(hps,FM_XOR);
	GpiSetColor(hps,CLR_WHITE);
	while (!saverinfo.closesemaphore) {
		extra += NEWSTARS;
		for (i = 0; i < maxstars; i++) {
			if (saverinfo.closesemaphore) break;
			if (star[i].z > 0) {
				if (star[i].color != CLR_WHITE) GpiSetColor(hps,star[i].color);
				GpiSetPel(hps,&star[i].oldpoint);
				star[i].z -= speed;
				if (star[i].z > 0) {
					x = xcenter+(EYEPOINT*star[i].x)/star[i].z;
					y = ycenter+(EYEPOINT*star[i].y)/star[i].z;
					if (x > saverinfo.screenrectl.xRight ||
						x < saverinfo.screenrectl.xLeft ||
						y > saverinfo.screenrectl.yTop ||
						y < saverinfo.screenrectl.yBottom)
						star[i].z = -1;
					else {
						star[i].oldpoint.x = x;
						star[i].oldpoint.y = y;
						GpiSetPel(hps,&star[i].oldpoint);
					}
				}
				if (star[i].color != CLR_WHITE) GpiSetColor(hps,CLR_WHITE);
			}
			if (extra && star[i].z <= 0) {
				extra--;
				star[i].z = VANISHING+EYEPOINT;
				star[i].x =	FixMul((FIXED)xsize<<1,(FIXED)rand()<<1)-xsize;
				star[i].y =	FixMul((FIXED)ysize<<1,(FIXED)rand()<<1)-ysize;
				if (rand()&63) star[i].color = CLR_WHITE;
				else star[i].color = rand()&1 ? CLR_CYAN : CLR_RED;
				star[i].oldpoint.x = star[i].oldpoint.y = -1;
			}
		}
	}
	WinReleasePS(hps);
	DosFreeSeg(starsel);
	WinTerminate(dhab);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo.closesemaphore);
}
