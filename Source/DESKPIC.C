/***************************************************************************\
* Module Name: deskpic.c
\***************************************************************************/

#define OURFORMATS 5
#define MAXFORMATS 30
#define OURSAVERS 4
#define MAXSAVERS 54
#define TIMEOUT 300
#define QUIXLEN 30
#define SPACESPEED 50
#define SAVERSTACK 4096

#define INCL_DOSMISC
#define INCL_DOSMODULEMGR
#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_GPIPRIMITIVES
#define INCL_GPIREGIONS
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINLISTBOXES
#define INCL_WINMENUS
#define INCL_WINPOINTERS
#define INCL_WINTIMER
#define INCL_WINRECTANGLES
#define INCL_WINSHELLDATA
#define INCL_WINSYS
#define INCL_WINWINDOWMGR

#include <os2.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	BOOL pict,saver;
	int fit;
	char path[128];
	int now,never;
	USHORT timeout;
	BOOL enabled[OURSAVERS];
	int params[OURSAVERS];
	BOOL single;
	int listpos;
} OPTIONS;

typedef struct {
	ULONG cbFix;
	USHORT cx;
	USHORT cy;
	USHORT cPlanes;
	USHORT cBitCount;
	RGB argbColor[256];
} COLORMAPINFO;

typedef struct {
	char far * (far pascal *extension)(HMODULE);
	ULONG (far pascal *getsize)(HFILE);
	BOOL (far pascal *read)(HFILE, SEL, COLORMAPINFO *);
	void (far pascal *info)(char far *, HWND);
	HMODULE module;
} FORMATS;

typedef struct {
	HAB deskpichab;
	HWND screenhwnd;
	RECTL screenrectl;
	ULONG volatile closesemaphore;
	HMODULE thismodule;
} SAVERBLOCK;

typedef struct {
	char far * (far pascal *status)(SAVERBLOCK far *, BOOL far *);
	MRESULT (EXPENTRY _export *dialog)(HWND, USHORT, MPARAM, MPARAM);
	void (far pascal *thread)(void);
	HMODULE module;
} SCREENSAVER;

typedef struct {
	HAB animatehab;
	HPS shadowhps;
	HWND screenhwnd;
	RECTL screenrectl;
	ULONG hpssemaphore;
	ULONG volatile closesemaphore;
	HMODULE thismodule;
	BOOL (far pascal _loadds *screenvisible)(void);
} INITBLOCK;

typedef struct {
	char (far pascal *name)(void);
	BOOL (far pascal *init)(INITBLOCK far *);
	void (far pascal *keychar)(char);
	void (far pascal *dblclk)(MPARAM);
	void (far pascal *paint)(HPS, RECTL far *);
	void (far pascal *close)(void);
	void (far pascal *thread)(void);
} DESKTOPS;

OPTIONS options = { TRUE, TRUE, 1, ".\\", 1, 3, TIMEOUT,
	{ TRUE, TRUE, TRUE, TRUE },
	{ QUIXLEN, QUIXLEN, SPACESPEED, 0 },
	FALSE, 0
};

void far pascal quixdaemon(void);
void far pascal spacedaemon(void);
void far pascal fadedaemon(void);
int volatile far * FAR PASCAL InstallDeskHook(HAB, HWND);
void FAR PASCAL ReleaseDeskHook(void);
int far loadpic(char *, HWND);
char far * far pascal aniextension(HMODULE);
char far * far pascal metextension(HMODULE);
char far * far pascal bmpextension(HMODULE);
char far * far pascal gifextension(HMODULE);
ULONG far pascal getgifsize(HFILE);
BOOL far pascal readgif(HFILE, SEL, COLORMAPINFO far *);
char far * far pascal pcxextension(HMODULE);
ULONG far pascal getpcxsize(HFILE);
BOOL far pascal readpcx(HFILE, SEL, COLORMAPINFO far *);
MRESULT EXPENTRY _export deskdialog(HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY _export saverdialog(HWND, USHORT, MPARAM, MPARAM);
void cdecl main(int, char **);
MRESULT EXPENTRY _export deskwndproc(HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY _export deskframeproc(HWND, USHORT, MPARAM, MPARAM);
void far destroypic(void);
void far iconize(BOOL, HWND, POINTL *);
BOOL far pascal _loadds screenvisible(void);
void cdecl _setenvp(void);

extern int near cursaver;
extern char near *ourname[];
extern HBITMAP near hbmmem;
extern SEL near stacksel;
extern TID near tid;
extern RECTL near picrect;
extern DESKTOPS near animate;
extern int near pictype;
extern HDC near hdcmem;
extern BOOL near allread;
extern INITBLOCK near init;

int near parameter;
HWND near framehndl;
int near oldpos;
SAVERBLOCK near saverinfo;
FORMATS far * near form;
SCREENSAVER near saver[MAXSAVERS];
int numformats = OURFORMATS;
int numsavers = OURSAVERS;
char deskpic[] = "Deskpic";
char opt[] = "Options";
BOOL nodlg = TRUE;
ULONG savesem = 0;

static int volatile far * near dllstatus;
static BOOL ver20;
static SEL near stack;
static PFNWP near pfnwp;
static TID near threadid;
static HWND near hwndtitlebar;
static HWND near hwndsysmenu;
static ULONG near timedown;
static long pause = 0;
static BOOL pausing = FALSE;
static BOOL bideing = TRUE;
static BOOL notsafe = FALSE;
static BOOL volatile visible = FALSE;
static BOOL saveractive = FALSE;
static HWND behind = HWND_BOTTOM;

void cdecl main(argc,argv)
int argc;
char **argv;
{
	char str1[255];
	char str2[255];
	char far *name;
	HMQ hmsgq;
	QMSG qmsg;
	HWND ltemp;
	HSWITCH hsw;
	USHORT i,j;
	BOOL err;
	HDIR hdir;
	SEL formsel;
	MENUITEM submenu;
	FILEFINDBUF findbuf;
	HMODULE module;
	DATETIME date;
	ULONG fcfval = FCF_ICON|FCF_TITLEBAR|FCF_NOBYTEALIGN|FCF_SYSMENU;
	static SHORT cont[] = { SC_MINIMIZE, SC_MAXIMIZE, SC_RESTORE, SC_SIZE };
	static MENUITEM mi = { MIT_END, MIS_SEPARATOR, 0, 256, NULL, 0 };
	static SWCNTRL swctl = { 0, 0, 0, 0, 0, SWL_VISIBLE,
		SWL_NOTJUMPABLE, "Desktop Picture / Screen Saver", 0 };

	DosGetVersion(&i);
	ver20 = HIBYTE(i) >= 20;
	saverinfo.deskpichab = init.animatehab = WinInitialize(0);
	hmsgq = WinCreateMsgQueue(saverinfo.deskpichab,DEFAULT_QUEUE_SIZE);
	i = sizeof(OPTIONS);
	WinQueryProfileData(saverinfo.deskpichab,deskpic,opt,&options,&i);
	timedown = options.timeout;
	oldpos = options.listpos;
	strcpy(str1,argv[0]);
	strrchr(str1,'\\')[1] = 0;

	WinRegisterClass(saverinfo.deskpichab,deskpic,deskwndproc,0L,0);
	framehndl = WinCreateStdWindow(HWND_DESKTOP,0L,&fcfval,deskpic,
		deskpic,0L,0,1,&saverinfo.screenhwnd);
	init.screenhwnd = saverinfo.screenhwnd;
	saverinfo.screenrectl.xLeft = saverinfo.screenrectl.yBottom = 0;
	saverinfo.screenrectl.xRight = WinQuerySysValue(HWND_DESKTOP,SV_CXSCREEN);
	saverinfo.screenrectl.yTop = WinQuerySysValue(HWND_DESKTOP,SV_CYSCREEN);
	init.screenrectl = saverinfo.screenrectl;
	WinSetWindowPos(framehndl,NULL,0,0,(SHORT)saverinfo.screenrectl.xRight,
		(SHORT)saverinfo.screenrectl.yTop,SWP_SIZE|SWP_MOVE);  
	DosAllocSeg(SAVERSTACK,&stack,SEG_NONSHARED);

	if (dllstatus = InstallDeskHook(saverinfo.deskpichab,saverinfo.screenhwnd)) {
		hwndtitlebar = WinWindowFromID(framehndl,FID_TITLEBAR);
		hwndsysmenu = WinWindowFromID(framehndl,FID_SYSMENU);
		i = SHORT1FROMMR(WinSendMsg(hwndsysmenu,MM_ITEMIDFROMPOSITION,0,0));
		WinSendMsg(hwndsysmenu,MM_QUERYITEM,MPFROM2SHORT(i,FALSE),MPFROMP(&submenu));
		for (i = 0; i < sizeof(cont)/sizeof(cont[0]); i++)
			WinSendMsg(submenu.hwndSubMenu,MM_DELETEITEM,MPFROM2SHORT(cont[i],FALSE),0);
		WinSendMsg(submenu.hwndSubMenu,MM_INSERTITEM,MPFROMP(&mi),MPFROMP(""));
		mi.afStyle = MIS_SYSCOMMAND|MIS_TEXT;
		mi.id = 1;
		WinSendMsg(submenu.hwndSubMenu,MM_INSERTITEM,MPFROMP(&mi),MPFROMP("~Options..."));

		DosAllocSeg(sizeof(FORMATS)*MAXFORMATS,&formsel,SEG_NONSHARED);
		form = MAKEP(formsel,0);
		form[0].extension = aniextension;
		form[1].extension = metextension;
		form[2].extension = bmpextension;
		form[3].extension = gifextension;
		form[3].getsize = getgifsize;
		form[3].read = readgif;
		form[4].extension = pcxextension;
		form[4].getsize = getpcxsize;
		form[4].read = readpcx;
		strcpy(str2,str1);
		strcat(str2,"*.DPR");
		hdir = HDIR_SYSTEM;
		i = 1;
		err = DosFindFirst(str2,&hdir,FILE_NORMAL,&findbuf,sizeof(FILEFINDBUF),
			&i,0L);
		while (!err && numformats < MAXFORMATS) {
			strcpy(str2,str1);
			strcat(str2,findbuf.achName);
			if (!DosLoadModule(NULL,0,str2,&module)) {
				DosGetProcAddr(module,(PSZ)1,(PPFN)&form[numformats].extension);
				DosGetProcAddr(module,(PSZ)2,(PPFN)&form[numformats].getsize);
				DosGetProcAddr(module,(PSZ)3,(PPFN)&form[numformats].read);
				if (DosGetProcAddr(module,(PSZ)4,&form[numformats].info))
					form[numformats].info = NULL;
				form[numformats++].module = module;
			}
			i = 1;
			err = DosFindNext(hdir,&findbuf,sizeof(FILEFINDBUF),&i);
		}
		DosFindClose(hdir);

		saver[0].thread = saver[1].thread = quixdaemon;
		saver[2].thread = spacedaemon;
		saver[3].thread = fadedaemon;
		for (i = 0; i < OURSAVERS; i++) {
			saver[i].dialog = saverdialog;
			saver[i].module = 0;
		}
		strcpy(str2,str1);
		strcat(str2,"*.DSS");
		hdir = HDIR_SYSTEM;
		i = 1;
		err = DosFindFirst(str2,&hdir,FILE_NORMAL,&findbuf,sizeof(FILEFINDBUF),
			&i,0L);
		while (!err && numsavers < MAXSAVERS) {
			strcpy(str2,str1);
			strcat(str2,findbuf.achName);
			if (!DosLoadModule(NULL,0,str2,&module)) {
				DosGetProcAddr(module,(PSZ)1,(PPFN)&saver[numsavers].status);
				DosGetProcAddr(module,(PSZ)2,(PPFN)&saver[numsavers].dialog);
				DosGetProcAddr(module,(PSZ)3,&saver[numsavers].thread);
				saver[numsavers++].module = module;
			}
			i = 1;
			err = DosFindNext(hdir,&findbuf,sizeof(FILEFINDBUF),&i);
		}
		DosFindClose(hdir);

		ltemp = WinCreateWindow(saverinfo.screenhwnd,WC_LISTBOX,"",0,0,0,100,100,
			saverinfo.screenhwnd,HWND_TOP,43,NULL,NULL);
		for (i = 0; i < (USHORT)numsavers; i++) {
			if (i < OURSAVERS) name = ourname[i];
			else {
				saverinfo.thismodule = saver[i].module;
				name = (saver[i].status)(&saverinfo,&j);
			}
			if (name) {
				j = SHORT1FROMMR(WinSendMsg(ltemp,LM_INSERTITEM,
					MPFROMSHORT(LIT_SORTASCENDING),MPFROMP(name)));
				WinSendMsg(ltemp,LM_SETITEMHANDLE,MPFROMSHORT(j),MPFROMSHORT(i));
			}
		}
		cursaver = SHORT1FROMMR(WinSendMsg(ltemp,LM_QUERYITEMHANDLE,
			MPFROMSHORT(oldpos),0));
		WinDestroyWindow(ltemp);

		DosGetDateTime(&date);
		srand((unsigned int)date.seconds*100+date.hundredths);
		for (i = 0; i < 17; i++) rand();
	retrypict:
		if (options.pict) {
			i = loadpic(argc > 1 ? argv[1] : options.path,framehndl);
			if (i == MBID_OK) {
				WinDlgBox(HWND_DESKTOP,framehndl,deskdialog,0,666,NULL);
				argc = 0;
				goto retrypict;
			}
			else if (i == MBID_CANCEL) {
				options.pict = FALSE;
				WinWriteProfileData(saverinfo.deskpichab,deskpic,opt,&options,
					sizeof(options));
				goto retrypict;
			}
		}
		else i = 0;
		if (!i) {
			if (!options.pict) {
				WinSetWindowPos(framehndl,HWND_BOTTOM,0,0,0,0,
					SWP_ZORDER|SWP_SHOW|SWP_NOREDRAW);
				notsafe = TRUE;
				WinSetWindowPos(framehndl,NULL,0,0,0,0,SWP_MINIMIZE);
			}
			else {
				WinSetParent(hwndtitlebar,HWND_OBJECT,FALSE);
				WinSetParent(hwndsysmenu,HWND_OBJECT,FALSE);
				WinSendMsg(framehndl,WM_UPDATEFRAME,
					MPFROMSHORT(FCF_TITLEBAR|FCF_SYSMENU),0);
				WinSetWindowPos(framehndl,HWND_BOTTOM,0,0,0,0,SWP_ZORDER|SWP_SHOW);  
			}
			pfnwp = WinSubclassWindow(framehndl,deskframeproc);
			swctl.hwnd = framehndl;
			WinQueryWindowProcess(framehndl,&swctl.idProcess,NULL);
			hsw = WinAddSwitchEntry(&swctl);
			WinStartTimer(saverinfo.deskpichab,saverinfo.screenhwnd,0,1000);

			while(WinGetMsg(saverinfo.deskpichab,&qmsg,NULL,0,0))
				WinDispatchMsg(saverinfo.deskpichab,&qmsg);

			WinStopTimer(saverinfo.deskpichab,saverinfo.screenhwnd,0);
			WinRemoveSwitchEntry(hsw);
			if (options.pict) {
				WinDestroyWindow(hwndtitlebar);
				WinDestroyWindow(hwndsysmenu);
			}
			if (saveractive) {
				DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,0,threadid);
				DosSemClear(&savesem);
				DosSemSetWait((HSEM)&saverinfo.closesemaphore,SEM_INDEFINITE_WAIT);
				WinShowPointer(HWND_DESKTOP,TRUE);
				if (!pictype) DosSemClear(&init.hpssemaphore);
			}
			destroypic();
		}
		for (i = OURSAVERS; (int)i < numsavers; i++) DosFreeModule(saver[i].module);
		for (i = OURFORMATS; (int)i < numformats; i++) DosFreeModule(form[i].module);
		DosFreeSeg(formsel);
		ReleaseDeskHook();
	}
	DosFreeSeg(stack);
	WinDestroyWindow(framehndl);
	WinDestroyMsgQueue(hmsgq);
	WinTerminate(saverinfo.deskpichab);
}

MRESULT EXPENTRY _export deskwndproc(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;
{
	int i,j,k;
	long l,m;
	HPS hps;
	POINTL mousep;
	POINTL aptl[3];
	RECTL rectup,pictrect,temprect;
	BOOL notvisible;
	static HWND near oldfocus;
	static POINTL near pntr;
	static int mouse = 0;
	static BOOL saveron[OURSAVERS] = { TRUE, FALSE, TRUE, FALSE };

	switch(message) {
	case WM_PAINT:
		if (saveractive || !options.pict) {
			WinQueryUpdateRect(hwnd,&rectup);
			WinValidateRect(hwnd,&rectup,FALSE);
		}
		else {
			WinSetWindowPos(framehndl,behind,0,0,0,0,SWP_ZORDER);
			if (!pictype) {
				DosSemClear(&pause);
				DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,PRTYD_MINIMUM,tid);
				DosSemRequest(&init.hpssemaphore,SEM_INDEFINITE_WAIT);
			}
			hps = WinBeginPaint(hwnd,NULL,&rectup);
			if (allread) {
				if (hdcmem) {
					for (l = saverinfo.screenrectl.yTop; l > saverinfo.screenrectl.yBottom;
						l -= picrect.yTop)
						for (m = saverinfo.screenrectl.xLeft; m < saverinfo.screenrectl.xRight;
						m += picrect.xRight) {
						if (options.fit < 2) pictrect = picrect;
						else {
							pictrect.yTop = l;
							pictrect.yBottom = l-picrect.yTop;
							pictrect.xLeft = m;
							pictrect.xRight = m+picrect.xRight;
						}
						if (WinIntersectRect(saverinfo.deskpichab,&temprect,&pictrect,
							&rectup)) {
							aptl[0].x = temprect.xLeft;
							aptl[0].y = temprect.yBottom;
							aptl[1].x = temprect.xRight;
							aptl[1].y = temprect.yTop;
							aptl[2].x = aptl[0].x-pictrect.xLeft;
							aptl[2].y = aptl[0].y-pictrect.yBottom;
							GpiBitBlt(hps,init.shadowhps,3L,aptl,ROP_SRCCOPY,BBO_IGNORE);
						}
						if (options.fit < 2) goto notile;
					}
				notile:
					if (picrect.xLeft > 0 || picrect.yBottom > 0) {
						temprect = picrect;
						temprect.xRight--;
						temprect.yTop--;
						GpiExcludeClipRectangle(hps,&temprect);
						WinFillRect(hps,&rectup,SYSCLR_BACKGROUND);
					}
				}
				else if (!pictype) (animate.paint)(hps,&rectup);
				if (!pictype && bideing) {
					bideing = FALSE;
					DosSemClear(&pause);
				}
			}
			else WinFillRect(hps,&rectup,CLR_BLACK);
			WinEndPaint(hps);
			if (!pictype) {
				DosSemClear(&init.hpssemaphore);
				DosSetPrty(PRTYS_THREAD,PRTYC_IDLETIME,0,tid);
			}
		}
		break;
	case WM_CHAR:
		if (!(SHORT1FROMMP(mp1)&KC_KEYUP) && !saveractive) {
			if (SHORT2FROMMP(mp2) == VK_F3 &&
				!(SHORT1FROMMP(mp1)&(KC_SHIFT|KC_ALT|KC_CTRL)))
				WinSendMsg(hwnd,WM_CLOSE,0,0);
			else if (options.pict && SHORT1FROMMP(mp1)&KC_CHAR)
			switch((char)SHORT1FROMMP(mp2)) {
			case 'F':
			case 'f':
				mouse = 3;
				if (behind == HWND_TOP) behind = HWND_BOTTOM;
				else behind = HWND_TOP;
				WinSetWindowPos(framehndl,behind,0,0,0,0,SWP_ZORDER);
				break;
			case 'P':
			case 'p':
				if (!pictype) {
					pausing = !pausing;
					DosSemClear(&pause);
				}
				break;
			default:
				if (!pictype) {
					DosSemClear(&pause);
					DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,0,tid);
					DosSemRequest(&init.hpssemaphore,SEM_INDEFINITE_WAIT);
					(animate.keychar)((char)SHORT1FROMMP(mp2));
					DosSemClear(&init.hpssemaphore);
					DosSetPrty(PRTYS_THREAD,PRTYC_IDLETIME,0,tid);
				}
				break;
			}
			return MRFROMSHORT(TRUE);
		}
		break;
	case WM_SEM1:
		if (saveractive) {
			DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,0,threadid);
			DosSemClear(&savesem);
			DosSemSetWait((HSEM)&saverinfo.closesemaphore,SEM_INDEFINITE_WAIT);
			saveractive = FALSE;
			WinShowPointer(HWND_DESKTOP,TRUE);
			if (WinQueryFocus(HWND_DESKTOP,FALSE) == hwnd) {
				if (WinIsWindow(saverinfo.deskpichab,oldfocus))
					WinFocusChange(HWND_DESKTOP,oldfocus,
					FC_NOLOSEACTIVE|FC_NOLOSEFOCUS|FC_NOLOSESELECTION|
					FC_NOSETACTIVE|FC_NOSETFOCUS|FC_NOSETSELECTION);
				else WinSetFocus(HWND_DESKTOP,NULL);
			}
			if (options.pict) {
				WinSetWindowPos(framehndl,behind,0,0,0,0,SWP_ZORDER);
				WinInvalidateRect(hwnd,NULL,FALSE);
			}
			else iconize(TRUE,HWND_BOTTOM,&pntr);
			if (!pictype) {
				DosSemClear(&init.hpssemaphore);
				DosSetPrty(PRTYS_THREAD,PRTYC_IDLETIME,0,tid);
			}
		}
		break;
	case WM_USER:
		allread = TRUE;
		if (pictype) DosFreeSeg(stacksel);
		WinInvalidateRect(hwnd,NULL,TRUE);
		break;
	case WM_USER+1:
		goto gonow;
	case WM_TIMER:
		if (options.pict && !saveractive && behind == HWND_BOTTOM &&
			framehndl != WinQueryWindow(HWND_DESKTOP,QW_BOTTOM,FALSE))
			WinSetWindowPos(framehndl,HWND_BOTTOM,0,0,0,0,SWP_ZORDER);
		if ((options.saver || !nodlg) && !saveractive) {
			if (pictype) {
				mousep.x = mousep.y = 0;
				hps = WinGetScreenPS(HWND_DESKTOP);
				visible = (GpiPtVisible(hps,&mousep) == PVIS_VISIBLE);
				WinReleasePS(hps);
			}
			if (!visible) break;
			if (!*dllstatus) {
				*dllstatus = 1;
				WinQueryPointerPos(HWND_DESKTOP,&mousep);
				if ((!(options.now&1) || mousep.x > saverinfo.screenrectl.xRight-3) &&
					((options.now&1) || mousep.x < saverinfo.screenrectl.xLeft+3) &&
					(!(options.now&2) || mousep.y < saverinfo.screenrectl.yBottom+3) &&
					((options.now&2) || mousep.y > saverinfo.screenrectl.yTop-3))
					timedown = 1;
				else if ((!(options.never&1) || mousep.x > saverinfo.screenrectl.xRight-3) &&
					((options.never&1) || mousep.x < saverinfo.screenrectl.xLeft+3) &&
					(!(options.never&2) || mousep.y < saverinfo.screenrectl.yBottom+3) &&
					((options.never&2) || mousep.y > saverinfo.screenrectl.yTop-3))
					timedown = 0;
				else timedown = options.timeout;
				break;
			}
			if (--timedown) break;
		gonow:
			if (nodlg && !options.single) {
				i = -1;
				k = 1;
				for (j = 0; j < numsavers; j++) {
					if (j < OURSAVERS) notvisible = options.enabled[j];
					else {
						saverinfo.thismodule = saver[j].module;
						(saver[j].status)(&saverinfo,&notvisible);
					}
					if (notvisible && !((long)rand()*k++>>15)) i = j;
				}
				if (i == -1) break;
			}
			else i = cursaver;
			if (!pictype) {
				DosSemClear(&pause);
				DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,PRTYD_MINIMUM,tid);
				DosSemRequest(&init.hpssemaphore,SEM_INDEFINITE_WAIT);
			}
			saveractive = TRUE;
			WinShowPointer(HWND_DESKTOP,FALSE);
			if (options.pict) {
				if (behind != HWND_TOP)
					WinSetWindowPos(framehndl,HWND_TOP,0,0,0,0,SWP_ZORDER);
			}
			else iconize(FALSE,HWND_TOP,&pntr);
			oldfocus = WinQueryFocus(HWND_DESKTOP,FALSE);
			WinFocusChange(HWND_DESKTOP,hwnd,
				FC_NOLOSEACTIVE|FC_NOLOSEFOCUS|FC_NOLOSESELECTION|
				FC_NOSETACTIVE|FC_NOSETFOCUS|FC_NOSETSELECTION);
			if (i < OURSAVERS) {
				if (saveron[i]) {
					hps = WinGetPS(hwnd);
					WinFillRect(hps,&saverinfo.screenrectl,CLR_BLACK);
					WinReleasePS(hps);
				}
				parameter = options.params[i];
			}
			else saverinfo.thismodule = saver[i].module;
			*dllstatus = 5;
			saverinfo.closesemaphore = 0;
			DosSemSet(&savesem);
			DosCreateThread((PFNTHREAD)saver[i].thread,&threadid,
				MAKEP(stack,SAVERSTACK-2));
			DosSetPrty(PRTYS_THREAD,PRTYC_IDLETIME,0,threadid);
		}
		break;
	case WM_BUTTON1DBLCLK:
		if (nodlg && allread && !saveractive) {
			nodlg = FALSE;
			WinLoadDlg(HWND_DESKTOP,
				WinQueryDesktopWindow(saverinfo.deskpichab,NULL),
				deskdialog,0,666,NULL);
			return MRFROMSHORT(TRUE);
		}
		break;
	case WM_BUTTON2DOWN:
	case WM_BUTTON2UP:
	case WM_BUTTON2DBLCLK:
	case WM_BUTTON3DOWN:
	case WM_BUTTON3UP:
	case WM_BUTTON3DBLCLK:
		if (!saveractive) WinPostMsg(WinQueryWindow(framehndl,QW_PARENT,FALSE),
			message,mp1,mp2);
		break;
	case WM_BUTTON1DOWN:
	case WM_FOCUSCHANGE:
		mouse = 0;
	case WM_MOUSEMOVE:
		if (mouse) mouse--;
		if (behind != HWND_BOTTOM && !mouse) {
			behind = HWND_BOTTOM;
			if (options.pict && !saveractive)
				WinSetWindowPos(framehndl,HWND_BOTTOM,0,0,0,0,SWP_ZORDER);
		}
	default:
		return WinDefWindowProc(hwnd,message,mp1,mp2);
	}
	return FALSE;
}

MRESULT EXPENTRY _export deskframeproc(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;
{
	switch (message) {
	case WM_BUTTON1DBLCLK:
		return WinSendMsg(saverinfo.screenhwnd,message,mp1,mp2);
	case WM_SYSCOMMAND:
		switch (SHORT1FROMMP(mp1)) {
		case SC_CLOSE:
			WinSendMsg(saverinfo.screenhwnd,WM_CLOSE,0,0);
			return FALSE;
		case 1:
			WinSendMsg(saverinfo.screenhwnd,WM_BUTTON1DBLCLK,0,0);
			return FALSE;
		}
		break;
	case WM_ADJUSTWINDOWPOS:
		if (options.pict && !saveractive && behind == HWND_BOTTOM) {
  			((PSWP)mp1)->fs |= SWP_ZORDER;
 			((PSWP)mp1)->hwndInsertBehind = HWND_BOTTOM;
		}
		if (notsafe) {
			((PSWP)mp1)->fs &= ~SWP_RESTORE;
			if (ver20 && (((PSWP)mp1)->fs&SWP_SHOW)) {
				((PSWP)mp1)->fs &= ~SWP_SHOW;
				WinPostMsg(saverinfo.screenhwnd,WM_BUTTON1DBLCLK,0,0);
			}
		}
	}
	return (pfnwp)(hwnd,message,mp1,mp2);
}

void far destroypic()
{
	if (!pictype) {
		DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,0,tid);
		DosSemSet((HSEM)&init.closesemaphore);
		DosSemClear(&pause);
		DosSemWait((HSEM)&init.closesemaphore,SEM_INDEFINITE_WAIT);
	}
	if (hdcmem) {
		GpiSetBitmap(init.shadowhps,NULL);
		GpiDeleteBitmap(hbmmem);
		GpiDestroyPS(init.shadowhps);
		DevCloseDC(hdcmem);
		hdcmem = NULL;
	}
	if (!pictype) {
		(animate.close)();
		DosFreeModule(init.thismodule);
		DosFreeSeg(stacksel);
	}
	pictype = -1;
}

void far iconize(down,zorder,pnt)
BOOL down;
HWND zorder;
POINTL *pnt;
{
	if (down) {
		WinSetWindowPos(framehndl,NULL,0,0,0,0,ver20 ? SWP_MINIMIZE : SWP_HIDE);
		notsafe = TRUE;
		WinSetParent(hwndtitlebar,framehndl,FALSE);
		WinSetParent(hwndsysmenu,framehndl,FALSE);
		WinSendMsg(framehndl,WM_UPDATEFRAME,
			MPFROMSHORT(FCF_TITLEBAR|FCF_SYSMENU),0);
		if (!ver20) WinSetWindowPos(framehndl,NULL,0,0,0,0,SWP_MINIMIZE);
		if (pnt) WinSetWindowPos(framehndl,NULL,(SHORT)pnt->x,(SHORT)pnt->y,
			0,0,SWP_MOVE);
		WinSetWindowPos(framehndl,zorder,0,0,0,0,
			ver20 ? SWP_ZORDER : SWP_ZORDER|SWP_SHOW);
	}
	else {
		if (pnt) {
			pnt->x = pnt->y = 0;
			WinMapWindowPoints(framehndl,HWND_DESKTOP,pnt,1);
		}
		WinSetParent(hwndtitlebar,HWND_OBJECT,FALSE);
		WinSetParent(hwndsysmenu,HWND_OBJECT,FALSE);
		WinSendMsg(framehndl,WM_UPDATEFRAME,
			MPFROMSHORT(FCF_TITLEBAR|FCF_SYSMENU),0);
		notsafe = FALSE;
		WinSetWindowPos(framehndl,zorder,0,0,0,0,SWP_RESTORE|SWP_ZORDER|SWP_SHOW);
	}
}

BOOL far pascal _loadds screenvisible()
{
	HPS hps;
	POINTL pnt;

	pnt.x = pnt.y = 0;
	hps = WinGetScreenPS(HWND_DESKTOP);
	visible = (GpiPtVisible(hps,&pnt) == PVIS_VISIBLE);
	WinReleasePS(hps);
	if (!visible || pausing || bideing) {
		DosSemSetWait(&pause,1000L);
		return FALSE;
	}
	return TRUE;
}

void cdecl _setenvp()
{
}
