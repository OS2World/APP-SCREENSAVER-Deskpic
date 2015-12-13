/***************************************************************************\
* FIRE.C - A fireworks screen saver extension by John Ridges
\***************************************************************************/

#define MAXMISSLES 200
#define NUMBRIGHT 6
#define NUMDARK 5

#define INCL_DOSPROCESS
#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINENTRYFIELDS
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
	int xc,yc,xv,yv;
	int xm,ym,xvm,yvm;
	long c,cm;
} SPARK;

typedef struct {
	BOOL enabled;
	int missles;
} PROFILEREC;

FIXED far pascal FixMul(FIXED, FIXED);
FIXED far pascal FixDiv(FIXED, FIXED);
static int near intrnd(int);
static FIXED near fixrnd(FIXED);
static FIXED near fixsqr(int);
static FIXED near fixsin(FIXED);
static FIXED near fixatn(FIXED);
static int near rand(void);

static ULONG near seed;
static char name[] = "Fireworks";
static BOOL gotprofile = FALSE;
static PROFILEREC profile = { TRUE, 75 };

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
	SHORT i;
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
		WinSetDlgItemShort(hwnd,4,profile.missles,TRUE);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == 5) {
			WinMessageBox(HWND_DESKTOP,hwnd,"The fireworks screen saver was "
				"inspired by the DOS version from Hand Crafted Software.",
				name,0,MB_OK|MB_NOICON);
			return FALSE;
		}
		if (SHORT1FROMMP(mp1) == 1) {
			WinQueryDlgItemShort(hwnd,4,&i,TRUE);
			if (i < 10 || i > MAXMISSLES) {
				WinMessageBox(HWND_DESKTOP,hwnd,"The number of missles must be "
					"between 10 and 200",NULL,0,MB_OK|MB_ICONHAND);
				WinSendDlgItemMsg(hwnd,4,EM_SETSEL,MPFROM2SHORT(0,
					WinQueryDlgItemTextLength(hwnd,4)),0);
				WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,4));
				return FALSE;
			}
			profile.missles = i;
			profile.enabled = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,3,BM_QUERYCHECK,0,0));
			WinWriteProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,
				sizeof(PROFILEREC));
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

void far pascal _loadds saverthread()

{
	int shots,xm,vxm,vym,i,tm,t,x,y,vx,vy,xo,yo;
	long c1,c2,c3;
	FIXED ang,f,a,v;
	HAB hab;
	HPS hps;
	SEL sparksel;
	POINTL pnt;
	SPARK *spark;
	static long bright[] = { CLR_WHITE, CLR_PINK, CLR_GREEN, CLR_CYAN,
		CLR_YELLOW, CLR_RED };
	static long dark[] = { CLR_DARKBLUE, CLR_DARKPINK, CLR_DARKCYAN,
		CLR_DARKGRAY, CLR_BLACK };

	hab = WinInitialize(0);
	DosAllocSeg(sizeof(SPARK)*profile.missles,&sparksel,SEG_NONSHARED);
	spark = MAKEP(sparksel,0);
	seed = WinGetCurrentTime(hab);
	shots = 10;
	xm = intrnd((int)(saverinfo->screenrectl.xRight-
		saverinfo->screenrectl.xLeft)-40)+20;
	hps = WinGetPS(saverinfo->screenhwnd);
	while (!saverinfo->closesemaphore) {
		i = (int)(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom)-15;
		vym = FIXEDINT(fixsqr((i-FIXEDINT(fixrnd(fixrnd(49152L*i))))*2));
		vxm = (intrnd((int)(saverinfo->screenrectl.xRight-
			saverinfo->screenrectl.xLeft)+160)-80-xm)/(2*vym);
		ang = fixatn(FixDiv(MAKEFIXED(vxm,0),MAKEFIXED(vym,0)));
		f = fixsqr(vxm*vxm+vym*vym);
		for (i = 0; i < profile.missles; i++) {
			if (saverinfo->closesemaphore) goto giveitup;
			spark[i].xm = xm+(i&1);
			spark[i].ym = 15+((i>>1)&1);
			spark[i].cm = CLR_RED;
			if (!i || (rand()&1)) {
				spark[i].xvm = vxm;
				spark[i].yvm = vym;
				if (rand()&1) spark[i].cm = CLR_DARKGRAY;
			}
			else {
				a = ang+32768-fixrnd(65536);
				v = FixMul(fixrnd(fixrnd(26214L))+6554,f);
				spark[i].xvm = FIXEDINT(FixMul(v,fixsin(a)));
				spark[i].yvm = FIXEDINT(FixMul(v,fixsin(a+102944)));
			}
		}
		tm = 3+intrnd(2*vym-6);
		f = fixrnd(fixrnd(262144))+65536;
		c1 = bright[intrnd(NUMBRIGHT)];
		c2 = bright[intrnd(NUMBRIGHT)];
		c3 = dark[intrnd(NUMDARK)];
		for (i = 0; i < profile.missles; i++) {
			if (saverinfo->closesemaphore) goto giveitup;
			a = fixrnd(411775);
			v = fixrnd(f);
			spark[i].xv = FIXEDINT(10*FixMul(v,fixsin(a+102944)));
			spark[i].yv = FIXEDINT(8*FixMul(v,fixsin(a)));
			if (rand()&1) spark[i].c = c1;
			else spark[i].c = c2;
		}
		if (shots > 2+intrnd(5)) {
			WinFillRect(hps,&saverinfo->screenrectl,CLR_BLACK);
			shots = 0;
		}
		for (t = 1; t <= tm; t++) {
			if (saverinfo->closesemaphore) goto giveitup;
			for (i = 0; i < profile.missles; i++) {
				x = spark[i].xm;
				vx = spark[i].xvm;
				y = spark[i].ym;
				vy = spark[i].yvm;
				if (t > 1) {
					GpiSetColor(hps,CLR_DARKGRAY);
					pnt.x = x-vx+saverinfo->screenrectl.xLeft;
					pnt.y = y-vy+saverinfo->screenrectl.yBottom;
					GpiMove(hps,&pnt);
					pnt.x = x+saverinfo->screenrectl.xLeft;
					pnt.y = y+saverinfo->screenrectl.yBottom;
					GpiLine(hps,&pnt);
				}
				else {
					pnt.x = x+saverinfo->screenrectl.xLeft;
					pnt.y = y+saverinfo->screenrectl.yBottom;
					GpiMove(hps,&pnt);
				}
				if (y < 0) {
					if (vy < 0) {
						vy = vx = 0;
						spark[i].cm = CLR_DARKGRAY;
					}
				}
				else vy -= 1;
				if (t < tm) {
					x += vx;
					y += vy;
					GpiSetColor(hps,spark[i].cm);
					pnt.x = x+saverinfo->screenrectl.xLeft;
					pnt.y = y+saverinfo->screenrectl.yBottom;
					GpiLine(hps,&pnt);
					spark[i].xm = x;
					spark[i].xvm = vx;
					spark[i].ym = y;
					spark[i].yvm = vy;
				}
			}
		}
		xo = spark[0].xm;
		vxm = spark[0].xvm/4;
		yo = spark[0].ym;
		vym = spark[0].yvm/8;
		tm = 90+intrnd(20);
		for (t = 1; t <= tm; t++) {
			if (saverinfo->closesemaphore) goto giveitup;
			for (i = 0; i < profile.missles; i++) {
				vx = spark[i].xv;
				vy = spark[i].yv;
				if (t > 1) {
					x = spark[i].xc;
					y = spark[i].yc;
					GpiSetColor(hps,c3);
					pnt.x = x-vx+saverinfo->screenrectl.xLeft;
					pnt.y = y-vy+saverinfo->screenrectl.yBottom;
					GpiMove(hps,&pnt);
					pnt.x = x+saverinfo->screenrectl.xLeft;
					pnt.y = y+saverinfo->screenrectl.yBottom;
					GpiLine(hps,&pnt);
				}
				else {
					x = xo;
					vx += vxm;
					y = yo;
					vy += vym;
					pnt.x = x+saverinfo->screenrectl.xLeft;
					pnt.y = y+saverinfo->screenrectl.yBottom;
					GpiMove(hps,&pnt);
				}
				if (t < tm) {
					if (y < 25) {
						if (vy < 0) {
							vy = -vy/4;
							if (vy) vx = vx/4;
							else {
								vx = 0;
								spark[i].c = c3;
							}
						}
					}
					else vy -= (t&1);
					x += vx;
					y += vy;
					GpiSetColor(hps,spark[i].c);
					pnt.x = x+saverinfo->screenrectl.xLeft;
					pnt.y = y+saverinfo->screenrectl.yBottom;
					GpiLine(hps,&pnt);
					spark[i].xc = x;
					spark[i].xv = vx;
					spark[i].yc = y;
					spark[i].yv = vy;
				}
			}
		}
		shots++;
		if (!(rand()&3))
			xm = intrnd((int)(saverinfo->screenrectl.xRight-
			saverinfo->screenrectl.xLeft)-40)+20;
	}
giveitup:
	WinReleasePS(hps);
	DosFreeSeg(sparksel);
	WinTerminate(hab);
	DosEnterCritSec();
	DosSemClear((HSEM)&saverinfo->closesemaphore);
}

static int near intrnd(arg)
int arg;
{
	return FIXEDINT(fixrnd(MAKEFIXED(arg,0)));
}

static FIXED near fixrnd(arg)
FIXED arg;
{
	return FixMul(arg,(FIXED)rand()<<1);
}

static FIXED near fixsqr(arg)
int arg;
{
	int i;
	FIXED a,b,c;

	a = 0L;
	b = 8388608;
	c = MAKEFIXED(arg,0);
	for (i = 0; i < 23; i++) {
		a ^= b;
		if (FixMul(a,a) > c) a ^= b;
		b >>= 1;
	}
	return a;
}

static FIXED near fixsin(arg)
FIXED arg;
{
	int i;
	FIXED r,s;
	static FIXED c[] = { -5018418, 5347884, -2709368, 411775 };

	arg = FixDiv(arg,411775);
	arg -= arg&0xffff0000;
	if (arg > 49152L) arg -= 65536L;
	else if (arg > 16384L) arg = 32768L-arg;
	s = FixMul(arg,arg);
	r = 2602479;
	for (i = 0; i < 4; i++) r = c[i]+FixMul(s,r);
	return FixMul(arg,r);
}

static FIXED near fixatn(arg)
FIXED arg;
{
	int i;
	FIXED a,r,s;
	static FIXED c[] = { -20L, 160L, -1553L, 27146L };

	a = arg;
	if (arg < 0L) a = -a;
	if (a > 65536L) a = FixDiv(65536L,a);
	r = FixMul(a,92682);
	a = FixDiv(r+a-65536L,r-a+65536L);
	s = FixMul(a,a);
	r = 2L;
	for (i = 0; i < 4; i++) r = c[i]+FixMul(s,r);
	a = 25736+FixMul(a,r);
	if (arg > 65536L || arg < -65536L) a = 102944-a;
	if (arg < 0L) a = -a;
	return a;
}

static int near rand()
{
	seed = seed*0x343fd+0x269ec3;
	return (int)(HIUSHORT(seed)&0x7fff);
}
