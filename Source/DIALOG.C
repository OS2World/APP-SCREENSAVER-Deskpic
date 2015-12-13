/***************************************************************************\
* Module Name: dialog.c
\***************************************************************************/

#define OURFORMATS 5
#define OURSAVERS 4

#define INCL_DOSMISC
#define INCL_GPIBITMAPS
#define INCL_GPILCIDS
#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINENTRYFIELDS
#define INCL_WININPUT
#define INCL_WINLISTBOXES
#define INCL_WINMENUS
#define INCL_WINSHELLDATA
#define INCL_WINSYS
#define INCL_WINWINDOWMGR

#include <os2.h>
#include <string.h>
#include "deskpic.h"

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
	void (far *thread)(void);
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

int far loadpic(char *, HWND);
void far destroypic(void);
void far iconize(BOOL, HWND, POINTL *);
MRESULT EXPENTRY _export aboutdialog(HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY _export deskdialog(HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY _export deskbutton(HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY _export saverdialog(HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY _export browsedialog(HWND, USHORT, MPARAM, MPARAM);
static void near scandir(HWND);

extern char near picname[];
extern DESKTOPS near animate;
extern int near pictype;
extern OPTIONS near options;
extern int oldpos;
extern SAVERBLOCK near saverinfo;
extern FORMATS * near form;
extern int near numformats;
extern SCREENSAVER near saver[];
extern int near numsavers;
extern char near deskpic[];
extern char near opt[];
extern BOOL near nodlg;

int near cursaver;
char near *ourname[] =
	{ "Flex", "Melting Flex", "Spaceflight", "Fade to black" };

static OPTIONS near optemp;
static PFNWP near oldbutton[8];

MRESULT EXPENTRY _export deskdialog(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;

{
	int i,j;
	long l;
	char temp[80];
	char far *name;
	HPS hps;
	BOOL enab;
	RECTL rect;
	POINTL pnt;
	FONTMETRICS fm;
	static int near oldgray;
	static BOOL near graygone;

	switch(message) {
	case WM_INITDLG:
		WinQueryWindowRect(hwnd,&rect);
		WinSetWindowPos(hwnd,NULL,
			(int)(WinQuerySysValue(HWND_DESKTOP,SV_CXSCREEN)-rect.xRight+rect.xLeft)/2,
			(int)(WinQuerySysValue(HWND_DESKTOP,SV_CYSCREEN)-rect.yTop+rect.yBottom)/2,
			0,0,SWP_MOVE);
		for (i = 0; i < 8; i++)
			oldbutton[i] = WinSubclassWindow(WinWindowFromID(hwnd,ID_NOW0+i),
				deskbutton);
		optemp = options;
		WinSendDlgItemMsg(hwnd,ID_PATH,EM_SETTEXTLIMIT,MPFROMSHORT(127),0);
		if (optemp.pict) WinSendDlgItemMsg(hwnd,ID_PICT,BM_SETCHECK,
			MPFROMSHORT(1),0);
		WinSendDlgItemMsg(hwnd,ID_CENTER+optemp.fit,BM_SETCHECK,
			MPFROMSHORT(1),0);
		WinSetDlgItemText(hwnd,ID_PATH,optemp.path);
		if (pictype < 0) WinEnableWindow(WinWindowFromID(hwnd,ID_INFO),FALSE);
		WinSetDlgItemShort(hwnd,ID_TIMEOUT,optemp.timeout,FALSE);
		if (optemp.saver) WinSendDlgItemMsg(hwnd,ID_SAVER,BM_SETCHECK,
			MPFROMSHORT(1),0);
		for (i = 0; i < numsavers; i++) {
			if (i < OURSAVERS) {
				name = ourname[i];
				enab = optemp.enabled[i];
			}
			else {
				saverinfo.thismodule = saver[i].module;
				name = (saver[i].status)(&saverinfo,&enab);
			}
			if (name) {
				j = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_LIST,LM_INSERTITEM,
					MPFROMSHORT(LIT_SORTASCENDING),MPFROMP(name)));
				WinSendDlgItemMsg(hwnd,ID_LIST,LM_SETITEMHANDLE,MPFROMSHORT(j),
					MPFROM2SHORT(i,(SHORT)enab));
			}
		}
		WinSendDlgItemMsg(hwnd,ID_LIST,LM_SELECTITEM,MPFROMSHORT(oldpos),
			MPFROMSHORT(TRUE));
		cursaver = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_LIST,LM_QUERYITEMHANDLE,
			MPFROMSHORT(oldpos),0));
		WinSendDlgItemMsg(hwnd,ID_NOW0+optemp.now,BM_SETCHECK,
			MPFROMSHORT(1),0);
		WinSendDlgItemMsg(hwnd,ID_NEVER0+optemp.never,BM_SETCHECK,
			MPFROMSHORT(1),0);
		if (optemp.single) WinSendDlgItemMsg(hwnd,ID_SINGLE,BM_SETCHECK,
			MPFROMSHORT(1),0);
		oldgray = ID_NEVER0+optemp.now;
		graygone = (BOOL)SHORT1FROMMR(WinSendDlgItemMsg(hwnd,oldgray,
			BM_SETCHECK,MPFROMSHORT(0),0));
		WinEnableWindow(WinWindowFromID(hwnd,oldgray),FALSE);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_CONTROL:
		if (SHORT1FROMMP(mp1) >= ID_NOW0 && SHORT1FROMMP(mp1) <= ID_NOW3) {
			WinEnableWindow(WinWindowFromID(hwnd,oldgray),TRUE);
			if (graygone) WinSendDlgItemMsg(hwnd,oldgray,BM_SETCHECK,
				MPFROMSHORT(1),0);
			oldgray = SHORT1FROMMP(mp1)-ID_NOW0+ID_NEVER0;
			graygone = (BOOL)SHORT1FROMMR(WinSendDlgItemMsg(hwnd,oldgray,
				BM_SETCHECK,MPFROMSHORT(0),0));
			WinEnableWindow(WinWindowFromID(hwnd,oldgray),FALSE);
			return FALSE;
		}
		else if (SHORT1FROMMP(mp1) >= ID_NEVER0 &&
			SHORT1FROMMP(mp1) <= ID_NEVER3) {
			graygone = FALSE;
			return FALSE;
		}
		else if (SHORT1FROMMP(mp1) == ID_SINGLE) {
			WinInvalidateRect(WinWindowFromID(hwnd,ID_LIST),NULL,TRUE);
			break;
		}
		else if (SHORT1FROMMP(mp1) != ID_LIST ||
			(SHORT2FROMMP(mp1) != LN_SELECT && SHORT2FROMMP(mp1) != LN_ENTER)) break;
		oldpos = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_LIST,LM_QUERYSELECTION,
			MPFROMSHORT(LIT_FIRST),0));
		cursaver = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_LIST,LM_QUERYITEMHANDLE,
			MPFROMSHORT(oldpos),0));
		if (SHORT2FROMMP(mp1) != LN_ENTER) return FALSE;
	configure:
		saverinfo.thismodule = saver[cursaver].module;
		WinDlgBox(HWND_DESKTOP,hwnd,saver[cursaver].dialog,saverinfo.thismodule,
			42,NULL);
		if (cursaver < OURSAVERS) enab = optemp.enabled[cursaver];
		else (saver[cursaver].status)(&saverinfo,&enab);
		WinSendDlgItemMsg(hwnd,ID_LIST,LM_SETITEMHANDLE,MPFROMSHORT(oldpos),
			MPFROM2SHORT(cursaver,(SHORT)enab));
		WinInvalidateRect(WinWindowFromID(hwnd,ID_LIST),NULL,TRUE);
		return FALSE;
	case WM_MEASUREITEM:
		hps = WinGetPS(WinWindowFromID(hwnd,ID_LIST));
		GpiQueryFontMetrics(hps,sizeof(FONTMETRICS),&fm);
		WinReleasePS(hps);
		return MRFROMSHORT(fm.lMaxAscender+fm.lMaxDescender);
	case WM_DRAWITEM:
		if (((POWNERITEM)mp2)->fsState == ((POWNERITEM)mp2)->fsStateOld) {
			rect = ((POWNERITEM)mp2)->rclItem;
			WinFillRect(((POWNERITEM)mp2)->hps,&rect,SYSCLR_WINDOW);
			enab = (BOOL)SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_SINGLE,BM_QUERYCHECK,0,0));
			l = rect.yTop-rect.yBottom-4;
			if (!enab && SHORT2FROMMP(((POWNERITEM)mp2)->hItem)) {
				GpiSetColor(((POWNERITEM)mp2)->hps,CLR_NEUTRAL);
				for (i = 0; i < 2; i++) {
					pnt.x = rect.xLeft+2;
					pnt.y = (rect.yTop+rect.yBottom>>1)-2+i;
					GpiMove(((POWNERITEM)mp2)->hps,&pnt);
					pnt.x += l/3-1;
					pnt.y -= l/3-1;
					GpiLine(((POWNERITEM)mp2)->hps,&pnt);
					pnt.x += l*2/3-2;
					pnt.y += l*2/3-2;
					GpiLine(((POWNERITEM)mp2)->hps,&pnt);
				}
			}
			rect.xLeft += l+2;
			WinSendDlgItemMsg(hwnd,ID_LIST,LM_QUERYITEMTEXT,
				MPFROM2SHORT(((POWNERITEM)mp2)->idItem,sizeof(temp)),MPFROMP(temp));
			WinDrawText(((POWNERITEM)mp2)->hps,0xffff,temp,&rect,
				SYSCLR_WINDOWTEXT,SYSCLR_WINDOW,DT_LEFT|DT_VCENTER);
		}
		return MRFROMSHORT(TRUE);
	case WM_CHAR:
		if (SHORT1FROMMP(mp2) == 13 && WinQueryFocus(HWND_DESKTOP,FALSE) ==
			WinWindowFromID(hwnd,ID_LIST)) return FALSE;
		break;
	case WM_COMMAND:
		switch (SHORT1FROMMP(mp1)) {
		case ID_INFO:
			if (pictype) {
				if (pictype >= OURFORMATS && form[pictype].info)
					(form[pictype].info)(picname,hwnd);
				else WinMessageBox(HWND_DESKTOP,hwnd,picname,"Current Picture:",0,
					MB_OK|MB_NOICON);
			}
			else {
				WinEnableWindow(WinWindowFromID(hwnd,ID_INFO),FALSE);
				WinEnableWindow(WinWindowFromID(hwnd,ID_OK),FALSE);
				WinEnableWindow(WinWindowFromID(hwnd,ID_CANCEL),FALSE);
				(animate.dblclk)(0);
				WinEnableWindow(WinWindowFromID(hwnd,ID_INFO),TRUE);
				WinEnableWindow(WinWindowFromID(hwnd,ID_OK),TRUE);
				WinEnableWindow(WinWindowFromID(hwnd,ID_CANCEL),TRUE);
			}
			return FALSE;
		case ID_BROWSE:
			WinDlgBox(HWND_DESKTOP,hwnd,browsedialog,0,668,NULL);
			return FALSE;
		case ID_ABOUT:
			WinDlgBox(HWND_DESKTOP,hwnd,aboutdialog,0,667,NULL);
			return FALSE;
		case ID_TEST:
			WinPostMsg(saverinfo.screenhwnd,WM_USER+1,0,0);
			return FALSE;
		case ID_CONFIGURE:
			goto configure;
		case ID_OK:
			optemp.pict = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_PICT,BM_QUERYCHECK,0,0));
			optemp.fit = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_CENTER,BM_QUERYCHECKINDEX,0,0));
			WinQueryDlgItemText(hwnd,ID_PATH,128,optemp.path);
			if (!nodlg) {
				enab = strcmp(options.path,optemp.path);
				if (optemp.pict) {
					if (enab || pictype < 0 || options.fit != optemp.fit) {
						i = loadpic(enab || pictype < 0 ? optemp.path : NULL,hwnd);
						if (i == MBID_OK) {
							WinSendDlgItemMsg(hwnd,ID_PATH,EM_SETSEL,MPFROM2SHORT(0,
								WinQueryDlgItemTextLength(hwnd,ID_PATH)),0);
							WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,ID_PATH));
							return FALSE;
						}
						if (i) {
							WinSendMsg(saverinfo.screenhwnd,WM_CLOSE,0,0);
							break;
						}
					}
				}
				else destroypic();
				if (options.pict != optemp.pict) iconize(options.pict,HWND_BOTTOM,NULL);
				if (enab || options.fit != optemp.fit || 
					options.pict != optemp.pict)
					WinInvalidateRect(saverinfo.screenhwnd,NULL,FALSE);
			}
			WinQueryDlgItemShort(hwnd,ID_TIMEOUT,(PSHORT)&optemp.timeout,FALSE);
			optemp.saver = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_SAVER,BM_QUERYCHECK,0,0));
			WinEnableWindow(WinWindowFromID(hwnd,oldgray),TRUE);
			if (graygone) WinSendDlgItemMsg(hwnd,oldgray,BM_SETCHECK,
				MPFROMSHORT(1),0);
			optemp.now = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_NOW0,BM_QUERYCHECKINDEX,0,0));
			optemp.never = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_NEVER0,BM_QUERYCHECKINDEX,0,0));
			optemp.single = (BOOL)SHORT1FROMMR(WinSendDlgItemMsg(hwnd,ID_SINGLE,BM_QUERYCHECK,0,0));
			optemp.listpos = oldpos;
			if (memcmp(&options,&optemp,sizeof(options))) {
				options = optemp;
				WinWriteProfileData(saverinfo.deskpichab,deskpic,opt,&options,
					sizeof(options));
			}
		}
		if (!nodlg) {
			WinDestroyWindow(hwnd);
			nodlg = TRUE;
			return FALSE;
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

MRESULT EXPENTRY _export deskbutton(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;

{
	int id;
	HWND phwnd;
	RECTL rectl;

	id = WinQueryWindowUShort(hwnd,QWS_ID);
	if (message == WM_SETFOCUS) {
		phwnd = WinWindowFromID(WinQueryWindow(hwnd,QW_OWNER,FALSE),
			id < ID_NEVER0 ? ID_NOW : ID_NEVER);
		if (SHORT1FROMMP(mp2)) {
			WinQueryWindowRect(phwnd,&rectl);
			WinCreateCursor(phwnd,0,0,(SHORT)rectl.xRight,
				(SHORT)rectl.yTop,CURSOR_FRAME|CURSOR_HALFTONE,NULL);
			WinShowCursor(phwnd,TRUE);
		}
		else WinDestroyCursor(phwnd);
		return FALSE;
	}
	return oldbutton[id-ID_NOW0](hwnd,message,mp1,mp2);
}

MRESULT EXPENTRY _export saverdialog(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;

{
	SHORT i;
	char temp[80];
	static int minval[] = { 1, 1, 15 };
	static int maxval[] = { 200, 200, 125 };
	static char near *valname[] = { "Length", "Length", "Speed" };
	static char near *valrange[] = { "1 and 200", "1 and 200", "15 and 125" };

	switch(message) {
	case WM_INITDLG:
		WinSetWindowText(WinWindowFromID(hwnd,IDS_TITLE),ourname[cursaver]);
		if (optemp.enabled[cursaver])
			WinSendDlgItemMsg(hwnd,IDS_ENABLED,BM_SETCHECK,MPFROMSHORT(1),0);
		if (cursaver == 3) {
			WinShowWindow(WinWindowFromID(hwnd,IDS_VALTEXT),FALSE);
			WinShowWindow(WinWindowFromID(hwnd,IDS_VALUE),FALSE);
		}
		else {
			strcpy(temp,valname[cursaver]);
			strcat(temp,":");
			WinSetWindowText(WinWindowFromID(hwnd,IDS_VALTEXT),temp);
			WinSetDlgItemShort(hwnd,IDS_VALUE,optemp.params[cursaver],TRUE);
		}
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == IDS_OK) {
			if (cursaver != 3) {
				WinQueryDlgItemShort(hwnd,IDS_VALUE,&i,TRUE);
				if (i < minval[cursaver] || i > maxval[cursaver]) {
					strcpy(temp,valname[cursaver]);
					strcat(temp," must be between ");
					strcat(temp,valrange[cursaver]);
					WinMessageBox(HWND_DESKTOP,hwnd,temp,NULL,0,MB_OK|MB_ICONHAND);
					WinSendDlgItemMsg(hwnd,IDS_VALUE,EM_SETSEL,MPFROM2SHORT(0,
						WinQueryDlgItemTextLength(hwnd,IDS_VALUE)),0);
					WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,IDS_VALUE));
					return FALSE;
				}
				optemp.params[cursaver] = options.params[cursaver] = i;
			}
			optemp.enabled[cursaver] = options.enabled[cursaver] =
				SHORT1FROMMR(WinSendDlgItemMsg(hwnd,IDS_ENABLED,BM_QUERYCHECK,0,0));
			WinWriteProfileData(saverinfo.deskpichab,deskpic,opt,&options,
				sizeof(options));
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

MRESULT EXPENTRY _export browsedialog(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;

{
	char temp[255];
	SHORT sel,val,i;
	static char near temppath[128];

	switch(message) {
	case WM_INITDLG:
		DosError(HARDERROR_DISABLE);
		WinQueryDlgItemText(WinQueryWindow(hwnd,QW_OWNER,FALSE),ID_PATH,128,temppath);
		scandir(hwnd);
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_CONTROL:
		if (SHORT1FROMMP(mp1) != IDB_LIST ||
			(SHORT2FROMMP(mp1) != LN_SELECT && SHORT2FROMMP(mp1) != LN_ENTER)) break;
		sel = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,IDB_LIST,LM_QUERYSELECTION,
			MPFROMSHORT(LIT_FIRST),0));
		val = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,IDB_LIST,LM_QUERYITEMHANDLE,
			MPFROMSHORT(sel),0));
		if (val < 2) {
			WinQueryWindowText(WinWindowFromID(hwnd,IDB_PATH),sizeof(temp),temp);
			i = strlen(temp);
			if (temp[i-1] == '\\') i--;
			WinSendDlgItemMsg(hwnd,IDB_LIST,LM_QUERYITEMTEXT,
				MPFROM2SHORT(sel,sizeof(temp)-i-!val),MPFROMP(temp+i+!val));
			temp[i] = '\\';
			i = strlen(temp)-4;
			if (val && !strcmp(temp+i,"\\..]")) {
				temp[i] = 0;
				*(strrchr(temp,'\\')+1) = 0;
			}
			if (val) strcpy(temp+strlen(temp)-1,"\\*.*");
		}
		else {
			strcpy(temp," :\\");
			temp[0] = (char)(val-2+'A');
			i = sizeof(temp)-3;
			DosQCurDir(val-1,temp+3,&i);
			if (temp[3]) strcat(temp,"\\");
			strcat(temp,"*.*");
		}
		WinSetDlgItemText(WinQueryWindow(hwnd,QW_OWNER,FALSE),ID_PATH,temp);
		if (SHORT2FROMMP(mp1) != LN_ENTER) return FALSE;
		switch (val)  {
		case 0:
			DosError(HARDERROR_ENABLE);
			WinDismissDlg(hwnd,IDB_OK);
			return FALSE;
		case 1:
			WinSendDlgItemMsg(hwnd,IDB_LIST,LM_QUERYITEMTEXT,
				MPFROM2SHORT(sel,sizeof(temp)),MPFROMP(temp));
			temp[strlen(temp)-1] = 0;
			DosChDir(temp+1,0L);
			break;
		default:
			DosSelectDisk(val-1);
		}
		WinEnableWindowUpdate(WinWindowFromID(hwnd,IDB_LIST),FALSE);
		WinSendDlgItemMsg(hwnd,IDB_LIST,LM_DELETEALL,0,0);
		scandir(hwnd);
		WinShowWindow(WinWindowFromID(hwnd,IDB_LIST),TRUE);
		return FALSE;
	case WM_CHAR:
		if (SHORT1FROMMP(mp2) == 13 && WinQueryFocus(HWND_DESKTOP,FALSE) ==
			WinWindowFromID(hwnd,IDB_LIST)) return FALSE;
		break;
	case WM_COMMAND:
		if (SHORT1FROMMP(mp1) == IDB_CANCEL)
			WinSetDlgItemText(WinQueryWindow(hwnd,QW_OWNER,FALSE),ID_PATH,temppath);
		DosError(HARDERROR_ENABLE);
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

static void near scandir(hwnd)
HWND hwnd;
{
	int err,n;
	char *srchname;
	char temp[255];
	BOOL root;
	USHORT curdrv;
	ULONG drives;
	FILEFINDBUF findbuf;
	HDIR hdir = HDIR_SYSTEM;

	strcpy(temp," :\\");
	DosQCurDisk(&curdrv,&drives);
	temp[0] = (char)(curdrv-1+'A');
	DosQCurDir(curdrv,temp+3,&err);
	root = strlen(temp) == 3;
	WinSetWindowText(WinWindowFromID(hwnd,IDB_PATH),temp);
	if (temp[3]) strcat(temp,"\\");
	strcat(temp,"*.*");
	curdrv = 1;
	err = DosFindFirst(temp,&hdir,FILE_NORMAL|FILE_DIRECTORY,&findbuf,
			sizeof(FILEFINDBUF),&curdrv,0L);
	while (!err) {
		n = 0;
		if (!(findbuf.attrFile&FILE_DIRECTORY)) for (; n < numformats; n++) {
			srchname = (form[n].extension)(form[n].module);
			if (!stricmp(findbuf.achName+strlen(findbuf.achName)-
				strlen(srchname),srchname)) break;
		}
		if (n < numformats) {
			if (findbuf.attrFile&FILE_DIRECTORY) {
				if (!strcmp(findbuf.achName,".") ||
					(root && !strcmp(findbuf.achName,".."))) goto nodot;
				temp[0] = '[';
				strcpy(temp+1,findbuf.achName);
				strcat(temp,"]");
				curdrv = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,IDB_LIST,LM_INSERTITEM,
					MPFROMSHORT(LIT_SORTASCENDING),MPFROMP(temp)));
			}
			else curdrv = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,IDB_LIST,LM_INSERTITEM,
				MPFROMSHORT(LIT_SORTASCENDING),MPFROMP(findbuf.achName)));
			WinSendDlgItemMsg(hwnd,IDB_LIST,LM_SETITEMHANDLE,MPFROMSHORT(curdrv),
				MPFROMSHORT(!!(findbuf.attrFile&FILE_DIRECTORY)));
		}
	nodot:
		curdrv = 1;
		err = DosFindNext(hdir,&findbuf,sizeof(FILEFINDBUF),&curdrv);
	}
	DosFindClose(hdir);
	strcpy(temp,"[- -]");
	for (n = 0; n < 26; n++) {
		if (drives&1) {
			temp[2] = (char)(n+'A');
			curdrv = SHORT1FROMMR(WinSendDlgItemMsg(hwnd,IDB_LIST,LM_INSERTITEM,
				MPFROMSHORT(LIT_END),MPFROMP(temp)));
			WinSendDlgItemMsg(hwnd,IDB_LIST,LM_SETITEMHANDLE,MPFROMSHORT(curdrv),
				MPFROMSHORT(n+2));
			}
		drives >>= 1;
	}
}
