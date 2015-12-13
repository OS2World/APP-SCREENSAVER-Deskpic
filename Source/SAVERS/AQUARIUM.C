/***************************************************************************\
* AQUARIUM.C - A screen saver extension by John Ridges
\***************************************************************************/

#define MAXFISH 25
#define MAXSPEED 10
#define FISHTYPE 3
#define DELAYTIME 50

#define abs(x) ((x) > 0 ? (x) : -(x))

#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINENTRYFIELDS
#define INCL_WINRECTANGLES
#define INCL_WINSHELLDATA
#define INCL_WINTIMER

#include <os2.h>

typedef struct {
	HAB deskpichab;
	HWND screenhwnd;
	RECTL screenrectl;
	ULONG volatile closesemaphore;
	HMODULE thismodule;
} SAVERBLOCK;

char far * far pascal _loadds saverstatus(SAVERBLOCK far *, BOOL far *);
MRESULT CALLBACK saverdialog(HWND, USHORT, MPARAM, MPARAM);
void far pascal _loadds saverthread(void);

static SAVERBLOCK far *saverinfo;

typedef struct {
	int velocity,tempvel;
	int xposition,yposition,ytop;
	int fishtype,flip;
	int countdown;
} FISHREC;

typedef struct {
	BOOL enabled;
	int numfish,minspeed,maxspeed;
} PROFILEREC;

static void near error(HWND, USHORT, char *);
static int near collide(FISHREC far *, int, int);
static int near rand(void);

static int near sizex[FISHTYPE*4];
static ULONG near seed;
static char name[] = "Aquarium";
static BOOL gotprofile = FALSE;
static PROFILEREC profile = { TRUE, 3, 1, 3 };

char far * far pascal _loadds saverstatus(initptr,enabledptr)
SAVERBLOCK far *initptr;
BOOL far *enabledptr;
{
	SHORT i;
	
	saverinfo = initptr;
	if (!gotprofile) {
		i = sizeof(PROFILEREC);
		WinQueryProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,&i);
		gotprofile = TRUE;
	}
	*enabledptr = profile.enabled;
	return name;
}

MRESULT CALLBACK saverdialog(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;
{
	SHORT i,j,k;
	RECTL ourrect,parentrect;

	switch(message) {
	case WM_INITDLG:
		WinQueryWindowRect(hwnd,&ourrect);
		WinQueryWindowRect(HWND_DESKTOP,&parentrect);
		WinSetWindowPos(hwnd,NULL,
			(int)(parentrect.xRight-ourrect.xRight+ourrect.xLeft)/2,
			(int)(parentrect.yTop-ourrect.yTop+ourrect.yBottom)/2,
			0,0,SWP_MOVE);
		if (profile.enabled)
			WinSendDlgItemMsg(hwnd,3,BM_SETCHECK,MPFROMSHORT(1),0);
		WinSetDlgItemShort(hwnd,4,profile.numfish,TRUE);
		WinSetDlgItemShort(hwnd,5,profile.minspeed,TRUE);
		WinSetDlgItemShort(hwnd,6,profile.maxspeed,TRUE);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == 1) {
			WinQueryDlgItemShort(hwnd,4,&i,TRUE);
			if (i < 1 || i > MAXFISH) {
				error(hwnd,4,"The number of fish must be between 1 and 25");
				return FALSE;
			}
			WinQueryDlgItemShort(hwnd,5,&j,TRUE);
			if (j < 1 || j > MAXSPEED) {
				error(hwnd,5,"The speed of the fish must be between 1 and 10");
				return FALSE;
			}
			WinQueryDlgItemShort(hwnd,6,&k,TRUE);
			if (k < 1 || k > MAXSPEED) {
				error(hwnd,6,"The speed of the fish must be between 1 and 10");
				return FALSE;
			}
			profile.numfish = i;
			if (j < k) {
				profile.minspeed = j;
				profile.maxspeed = k;
			}
			else {
				profile.minspeed = k;
				profile.maxspeed = j;
			}
			profile.enabled = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,3,BM_QUERYCHECK,0,0));
			WinWriteProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,
				sizeof(PROFILEREC));
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

void far pascal _loadds saverthread()
{
	int i,j,k,m,type,numdone;
	int sizey[FISHTYPE*4];
	HAB hab;
	HPS hps;
	SEL fishsel;
	FISHREC far *fish;
	SIZEL sizl;
	RECTL rectup;
	HDC memhdc[FISHTYPE*4];
	HPS memhps[FISHTYPE*4];
	HBITMAP bitmap,memhbm[FISHTYPE*4];
	ULONG time,aldata[2];
	POINTL aptl[4];
	BITMAPINFOHEADER bmi;
	DEVOPENSTRUC dop;

	hab = WinInitialize(0);
	hps = WinGetPS(saverinfo->screenhwnd);
	WinFillRect(hps,&saverinfo->screenrectl,CLR_BLACK);
	DosAllocSeg(sizeof(FISHREC)*profile.numfish,&fishsel,SEG_NONSHARED);
	fish = MAKEP(fishsel,0);
	seed = WinGetCurrentTime(hab);
	for (numdone = 0; numdone < FISHTYPE*4; numdone++) {
		dop.pszLogAddress = NULL;
		dop.pszDriverName = "DISPLAY";
		dop.pdriv = NULL;
		dop.pszDataType = NULL;
		memhdc[numdone] = DevOpenDC(hab,OD_MEMORY,"*",4L,(PDEVOPENDATA)&dop,
			NULL);
		sizl.cx = sizl.cy = 0;
		memhps[numdone] = GpiCreatePS(hab,memhdc[numdone],&sizl,
			PU_PELS|GPIF_DEFAULT|GPIT_MICRO|GPIA_ASSOC);
		GpiQueryDeviceBitmapFormats(memhps[numdone],2L,aldata);
		bmi.cbFix = sizeof(BITMAPINFOHEADER);
		sizex[numdone] = 32+32*(~numdone&1)+profile.maxspeed;
		bmi.cx = sizex[numdone]*8;
		bmi.cy = sizey[numdone] = 32+16*(~numdone&2);
		bmi.cPlanes = (USHORT)aldata[0];
		bmi.cBitCount = (USHORT)aldata[1];
		memhbm[numdone] = GpiCreateBitmap(memhps[numdone],&bmi,0L,NULL,NULL);
		GpiSetBitmap(memhps[numdone],memhbm[numdone]);
		if (numdone%4 < 2) {
			WinSetRect(hab,&rectup,0,0,bmi.cx,bmi.cy);
			WinFillRect(memhps[numdone],&rectup,CLR_BLACK);
			bitmap = GpiLoadBitmap(memhps[numdone],saverinfo->thismodule,
				numdone/4+1,0L,0L);
			for (j = 0; j < 8; j++) {
				if (saverinfo->closesemaphore) goto giveup;
				aptl[0].x = j*sizex[numdone]+profile.maxspeed*(j < 2 || j == 4);
				aptl[0].y = aptl[2].y = 0;
				aptl[1].x = aptl[0].x+sizex[numdone]-profile.maxspeed-1;
				aptl[1].y = sizey[numdone]-1;
				aptl[2].x = j*64;
				aptl[2].y = 0;
				aptl[3].x = aptl[2].x+64;
				aptl[3].y = 64;
				GpiWCBitBlt(memhps[numdone],bitmap,4L,aptl,ROP_SRCCOPY,BBO_IGNORE);
			}
			GpiDeleteBitmap(bitmap);
		}
		else {
			if (saverinfo->closesemaphore) goto giveup;
			aptl[0].x = aptl[0].y = aptl[2].x = aptl[2].y = 0;
			aptl[1].x = bmi.cx;
			aptl[1].y = bmi.cy;
			aptl[3].x = sizex[numdone-2]*8;
			aptl[3].y = sizey[numdone-2];
			GpiBitBlt(memhps[numdone],memhps[numdone-2],4L,aptl,ROP_SRCCOPY,
				BBO_IGNORE);
		}
	}
	numdone--;
	for (i = 0; i < profile.numfish; i++)
		fish[i].yposition = fish[i].ytop = -1000;
	while (!saverinfo->closesemaphore) {
		time = WinGetCurrentTime(hab);
		for (i = 0; i < profile.numfish; i++)
			fish[i].xposition += fish[i].velocity;
		for (i = 0; i < profile.numfish; i++) {
			if (fish[i].yposition == -1000) {
				fish[i].fishtype = type = (int)((long)rand()*FISHTYPE*4>>15);
				k = rand()&1;
				j = (int)(((long)rand()*(profile.maxspeed-
					profile.minspeed+1)>>15))+profile.minspeed;
				fish[i].velocity = k ? j : -j;
				fish[i].yposition = (int)((rand()*(saverinfo->screenrectl.yTop-
					saverinfo->screenrectl.yBottom-64)>>15)+
					saverinfo->screenrectl.yBottom);
				fish[i].ytop = fish[i].yposition+sizey[type];
				fish[i].xposition = k ? -sizex[type] : (int)saverinfo->screenrectl.xRight;
				fish[i].flip = fish[i].countdown = 0;
				if (collide(fish,i,0) >= 0) fish[i].yposition = fish[i].ytop = -1000;
			}
			else type = fish[i].fishtype;
			j = i+1;
		retrycollide:
			if ((j = collide(fish,i,j)) >= 0) {
				if (!((fish[i].velocity < 0 && fish[j].velocity > 0) ||
					(fish[i].velocity > 0 && fish[j].velocity < 0))) {
					if ((fish[i].velocity+fish[j].velocity > 0 &&
						fish[i].xposition < fish[j].xposition) ||
						(fish[i].velocity+fish[j].velocity < 0 &&
						fish[i].xposition > fish[j].xposition)) k = j;
					else k = i;
					if (fish[k].velocity) fish[k].velocity = -fish[k].velocity;
					else fish[k].velocity = fish[i].velocity+fish[j].velocity > 0 ?
						-abs(fish[k].tempvel) : abs(fish[k].tempvel);
				}
				if (type%4 != fish[j].fishtype%4) k = type%4 < fish[j].fishtype%4;
				else k = abs(fish[i].velocity) > abs(fish[j].velocity);
				WinSetRect(hab,&rectup,fish[i].xposition,fish[i].yposition,
					fish[i].xposition+sizex[type],fish[i].ytop);
				WinFillRect(hps,&rectup,CLR_BLACK);
				WinSetRect(hab,&rectup,fish[j].xposition,fish[j].yposition,
					fish[j].xposition+sizex[fish[j].fishtype],fish[j].ytop);
				WinFillRect(hps,&rectup,CLR_BLACK);
				m = k ? i : j;
				k = k ? j : i;
				fish[m].yposition = fish[k].yposition;
				fish[m].ytop = fish[m].yposition+sizey[fish[m].fishtype];
				if (fish[m].velocity > 0)
					fish[m].xposition += sizex[fish[k].fishtype]>>1;
				else fish[m].xposition -= sizex[fish[k].fishtype]>>1;
				fish[k].yposition = fish[k].ytop = -1000;
				fish[m].countdown = -2;
				j = 0;
				goto retrycollide;
			}
			if (fish[i].velocity) {
				if (!(rand()>>5)) {
					fish[i].tempvel = fish[i].velocity;
					fish[i].velocity = 0;
					fish[i].countdown = 32+(rand()>>10);
				}
			}
			else if (!--fish[i].countdown)
				fish[i].velocity = rand()&1 ? fish[i].tempvel : -fish[i].tempvel;
			if (fish[i].xposition < -sizex[type] ||
				fish[i].xposition > (int)saverinfo->screenrectl.xRight)
				fish[i].yposition = fish[i].ytop = -1000;
			else {
				aptl[0].x = fish[i].xposition;
				aptl[0].y = fish[i].yposition;
				aptl[1].x = aptl[0].x+sizex[type];
				aptl[1].y = aptl[0].y+sizey[type];
				if (fish[i].countdown < 0) {
					fish[i].countdown++;
					k = 4+(fish[i].velocity < 0);
				}
				else k = ((fish[i].velocity <= 0)+(!fish[i].velocity<<1)<<1)+
					((fish[i].flip>>2)&1);
				aptl[2].x = k*sizex[type];
				aptl[2].y = 0;
				GpiBitBlt(hps,memhps[type],3L,aptl,ROP_SRCCOPY,BBO_IGNORE);
				fish[i].flip++;
			}
		}
		while (time+DELAYTIME > WinGetCurrentTime(hab));
	}
giveup:
	for (i = 0; i <= numdone; i++) {
		GpiSetBitmap(memhps[i],NULL);
		GpiDeleteBitmap(memhbm[i]);
		GpiDestroyPS(memhps[i]);
		DevCloseDC(memhdc[i]);
	}
	DosFreeSeg(fishsel);
	WinReleasePS(hps);
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo->closesemaphore);
}

static void near error(hwnd,id,message)
HWND hwnd;
USHORT id;
char *message;
{
	WinMessageBox(HWND_DESKTOP,hwnd,message,NULL,0,MB_OK|MB_ICONHAND);
	WinSendDlgItemMsg(hwnd,id,EM_SETSEL,MPFROM2SHORT(0,
		WinQueryDlgItemTextLength(hwnd,id)),0);
	WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,id));
}

static int near collide(fish,i,start)
FISHREC far *fish;
int i,start;
{
	int low,high;

	low = fish[i].yposition;
	high = fish[i].ytop;
	for (; start < profile.numfish; start++) if (i != start &&
		low < fish[start].ytop && high > fish[start].yposition &&
		fish[i].xposition < fish[start].xposition+sizex[fish[start].fishtype] &&
		fish[i].xposition+sizex[fish[i].fishtype] > fish[start].xposition)
		return start;
	return -1;
}

static int near rand()
{
	seed = seed*0x343fd+0x269ec3;
	return (int)(HIUSHORT(seed)&0x7fff);
}
