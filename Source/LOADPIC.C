/***************************************************************************\
* Module Name: loadpic.c
\***************************************************************************/

#define OURSAVERS 4

#define INCL_BITMAPFILEFORMAT
#define INCL_DOSMEMMGR
#define INCL_DOSMODULEMGR
#define INCL_DOSPROCESS
#define INCL_GPIBITMAPS
#define INCL_GPIMETAFILES
#define INCL_WINRECTANGLES
#define INCL_WINSYS

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

void far destroypic(void);
BOOL far pascal _loadds screenvisible(void);
int far loadpic(char *, HWND);
static void far readthread(void);
char far * far pascal aniextension(HMODULE);
char far * far pascal metextension(HMODULE);
char far * far pascal bmpextension(HMODULE);

extern OPTIONS near options;
extern HWND near framehndl;
extern SAVERBLOCK near saverinfo;
extern FORMATS * near form;
extern int near numformats;

char near picname[255];
HBITMAP near hbmmem;
SEL near stacksel;
TID near tid;
RECTL near picrect;
DESKTOPS near animate;
int near pictype = -1;
HDC hdcmem = NULL;
BOOL allread = TRUE;
INITBLOCK init = { NULL, NULL, NULL, { 0, 0, 0, 0 }, 0, 0, 0, screenvisible };

static long near totalblocksize;
static SEL near sel;
static HFILE near hf;
static HMF near hmf;
static ULONG near aldata[2];

int far loadpic(file,hwnd)
char *file;
HWND hwnd;

{
	int n,files,temptype;
	char *srchname;
	char temp[255];
	HPS hps;
	FILESTATUS fsts;
	HMODULE tempmodule;
	FILEFINDBUF findbuf,foundbuf;
	USHORT usaction,err,numfiles2find;
	SIZEL sizl;
	DEVOPENSTRUC dop;
	BITMAPINFOHEADER bmi;
	HDIR hdir;

	if (file) {
		if (file[0]) strcpy(temp,file);
		else strcpy(temp,".\\");
		if (!(srchname = strrchr(temp,'\\'))) srchname = temp;
		if (temp[strlen(temp)-1] != '\\' && !strchr(srchname,'.'))
			strcat(temp,"\\");
		if (temp[strlen(temp)-1] == '\\') strcat(temp,"*.*");

		files = 0;
		numfiles2find = 1;
		hdir = HDIR_SYSTEM;
		err = DosFindFirst(temp,&hdir,FILE_NORMAL,&findbuf,
			sizeof(FILEFINDBUF),&numfiles2find,0L);
		while (!err) {
			for (n = 0; n < numformats; n++) {
				srchname = (form[n].extension)(form[n].module);
				if (!stricmp(findbuf.achName+strlen(findbuf.achName)-
					strlen(srchname),srchname)) {
					files++;
					if (!((long)rand()*files>>15)) foundbuf = findbuf;
				}
			}
			numfiles2find = 1;
			err = DosFindNext(hdir,&findbuf,sizeof(FILEFINDBUF),
				&numfiles2find);
		}
		DosFindClose(hdir);

		if (files) {
			if (!(srchname = strrchr(temp,'\\'))) srchname = temp;
			else srchname++;
			strcpy(srchname,foundbuf.achName);

			temptype = 0;
			for (n = numformats; n--; ) {
				srchname = (form[n].extension)(form[n].module);
				if (!stricmp(foundbuf.achName+strlen(foundbuf.achName)-
					strlen(srchname),srchname)) {
					temptype = n;
					break;
				}
			}
		}
	}
	else {
		strcpy(temp,picname);
		temptype = pictype;
		files = 1;
	}

	if (files) {
		switch (temptype) {
		case 0:
			if (DosLoadModule(NULL,0,temp,&tempmodule)) goto abort;
			break;
		case 1:
			if ((hmf = GpiLoadMetaFile(saverinfo.deskpichab,temp)) ==
				GPI_ERROR) goto abort;
			break;
		default:
			if (DosOpen(temp,&hf,&usaction,0L,FILE_READONLY,FILE_OPEN,
				OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,0L)) goto abort;
			if (temptype != 2) {
				if (!(totalblocksize = (form[temptype].getsize)(hf)))
					totalblocksize = -1;
			}
			else {
				DosQFileInfo(hf,1,&fsts,sizeof(FILESTATUS));
				totalblocksize = fsts.cbFile;
			}
		}
		destroypic();
		strcpy(picname,temp);
		pictype = temptype;
		if (!pictype) {
			DosGetProcAddr(tempmodule,(PSZ)1,(PPFN)&animate.name);
			DosGetProcAddr(tempmodule,(PSZ)2,(PPFN)&animate.init);
			DosGetProcAddr(tempmodule,(PSZ)3,&animate.keychar);
			DosGetProcAddr(tempmodule,(PSZ)4,&animate.dblclk);
			DosGetProcAddr(tempmodule,(PSZ)5,&animate.paint);
			DosGetProcAddr(tempmodule,(PSZ)6,&animate.close);
			DosGetProcAddr(tempmodule,(PSZ)7,&animate.thread);
			(animate.name)();
			init.thismodule = tempmodule;
			init.closesemaphore = 0;
		}

		if (pictype || (animate.init)(&init)) {
			hps = WinGetPS(saverinfo.screenhwnd);
			GpiQueryDeviceBitmapFormats(hps,2L,aldata);
			WinReleasePS(hps);
			dop.pszLogAddress = NULL;
			dop.pszDriverName = "DISPLAY";
			dop.pdriv = NULL;
			dop.pszDataType = NULL;
			hdcmem = DevOpenDC(saverinfo.deskpichab,OD_MEMORY,"*",4L,
				(PDEVOPENDATA)&dop,NULL);
			sizl.cx = sizl.cy = 0;
			init.shadowhps = GpiCreatePS(saverinfo.deskpichab,hdcmem,&sizl,
				PU_PELS|GPIF_DEFAULT|GPIT_MICRO|GPIA_ASSOC);
			if (!pictype) {
				bmi.cbFix = sizeof(BITMAPINFOHEADER);
				bmi.cx = (SHORT)(saverinfo.screenrectl.xRight-saverinfo.screenrectl.xLeft);
				bmi.cy = (SHORT)(saverinfo.screenrectl.yTop-saverinfo.screenrectl.yBottom);
				bmi.cPlanes = (USHORT)aldata[0];
				bmi.cBitCount = (USHORT)aldata[1];
				hbmmem = GpiCreateBitmap(init.shadowhps,&bmi,0L,NULL,NULL);
				GpiSetBitmap(init.shadowhps,hbmmem);
			}
		}
		picrect = saverinfo.screenrectl;
		WinOffsetRect(saverinfo.deskpichab,&picrect,-(SHORT)picrect.xLeft,
			-(SHORT)picrect.yBottom);
		if (DosAllocSeg(pictype ? 20480 : 4096,&stacksel,SEG_NONSHARED) ||
			(pictype > 1 && DosAllocHuge((USHORT)(totalblocksize>>16),
				(USHORT)totalblocksize,&sel,0,SEG_NONSHARED))) {
			strcpy(temp,"DESKPIC can't load picture file ");
			strcat(temp,picname);
			WinMessageBox(HWND_DESKTOP,hwnd,temp,"FATAL ERROR!",0,
				MB_CANCEL|MB_ICONHAND);
			return -1;
		}
		else {
			allread = FALSE;
			if (pictype) {
				DosCreateThread(readthread,&tid,MAKEP(stacksel,20478));
				DosSetPrty(PRTYS_THREAD,PRTYC_REGULAR,PRTYD_MINIMUM,tid);
			}
			else {
				DosCreateThread((PFNTHREAD)animate.thread,&tid,MAKEP(stacksel,4094));
				DosSetPrty(PRTYS_THREAD,PRTYC_IDLETIME,0,tid);
			}
		}
	}
	else {
	abort:
		strcpy(temp,"DESKPIC can't find any picture files.\n\n"
			"You must specify a path for your ");
		for (n = 1; n < numformats; n++) {
			strcat(temp,(form[n].extension)(form[n].module));
			strcat(temp,", ");
		}
		strcat(temp,"or .ANI files.");
		return WinMessageBox(HWND_DESKTOP,hwnd,temp,"Whoops!",0,
			hwnd == framehndl ? MB_OKCANCEL|MB_ICONEXCLAMATION :
			MB_OK|MB_ICONEXCLAMATION);
	}
	return 0;
}

static void far readthread()

{
	HAB dhab;
	HBITMAP hbm;
	HPS hps;
	ULONG i;
	SIZEL sizl;
	BOOL err;
	POINTL aptl[4];
	LONG mfoptions[8];
	USHORT hshift,usaction;
	BITMAPFILEHEADER *pbfh;
	BITMAPINFOHEADER bmi;
	COLORMAPINFO cmi;

	dhab = WinInitialize(0); 

	if (pictype > 1) {
		if (pictype != 2) {
			err = (form[pictype].read)(hf,sel,&cmi);
			cmi.cbFix = sizeof(BITMAPINFOHEADER);
			sizl.cx = cmi.cx;
			sizl.cy = cmi.cy;
		}
		else {
			err = FALSE;
			DosGetHugeShift(&hshift);
			for (i = 0; i < (ULONG)totalblocksize>>16; i++) {
				DosRead(hf,MAKEP(sel+(i<<hshift),0),0x8000,&usaction);
				DosRead(hf,MAKEP(sel+(i<<hshift),0x8000),0x8000,&usaction);
			}
			if (totalblocksize&0xffff)
				DosRead(hf,MAKEP(sel+(i<<hshift),0),(USHORT)totalblocksize,&usaction);
			pbfh = MAKEP(sel,0);
			sizl.cx = pbfh->bmp.cx;
			sizl.cy = pbfh->bmp.cy;
		}
		DosClose(hf);
		if (options.fit != 1) {
			if (picrect.xRight > sizl.cx) {
				i = options.fit ? 1L : WinQuerySysValue(HWND_DESKTOP,SV_CXBYTEALIGN);
				if (i != 1L) i <<= 1;
				picrect.xRight = ((sizl.cx+i-1)/i)*i;
			}
			if (picrect.yTop > sizl.cy) {
				i = options.fit ? 1L : WinQuerySysValue(HWND_DESKTOP,SV_CYBYTEALIGN);
				if (i != 1L) i <<= 1;
				picrect.yTop = ((sizl.cy+i-1)/i)*i;
			}
		}
	}
	bmi.cbFix = sizeof(BITMAPINFOHEADER);
	bmi.cPlanes = (USHORT)aldata[0];
	bmi.cBitCount = (USHORT)aldata[1];
	bmi.cx = (SHORT)picrect.xRight;
	bmi.cy = (SHORT)picrect.yTop;
	hbmmem = GpiCreateBitmap(init.shadowhps,&bmi,0L,NULL,NULL);
	GpiSetBitmap(init.shadowhps,hbmmem);
	WinFillRect(init.shadowhps,&picrect,SYSCLR_BACKGROUND);
	if (pictype > 1) {
		if (!err) {
			hps = GpiCreatePS(dhab,NULL,&sizl,
				PU_PELS|GPIF_DEFAULT|GPIT_NORMAL|GPIA_NOASSOC);

			if (pictype != 2) hbm = GpiCreateBitmap(hps,(PBITMAPINFOHEADER)&cmi,
				CBM_INIT,MAKEP(sel,0),(PBITMAPINFO)&cmi);
			else hbm = GpiCreateBitmap(hps,&(pbfh->bmp),CBM_INIT,
				(PBYTE)MAKEP(sel,0)+pbfh->offBits,(PBITMAPINFO)&(pbfh->bmp));

			switch (options.fit) {
			case 0:
				aptl[0].x = picrect.xRight-sizl.cx>>1;
				aptl[0].y = picrect.yTop-sizl.cy>>1;
				aptl[1].x = aptl[0].x+sizl.cx-1;
				aptl[1].y = aptl[0].y+sizl.cy-1;
				break;
			case 1:
				aptl[0].x = aptl[0].y = 0;
				aptl[1].x = picrect.xRight-1;
				aptl[1].y = picrect.yTop-1;
				break;
			case 2:
				aptl[0].x = 0;
				aptl[0].y = picrect.yTop-sizl.cy;
				aptl[1].x = sizl.cx-1;
				aptl[1].y = picrect.yTop-1;
			}
			aptl[2].x = aptl[2].y = 0;
			aptl[3].x = sizl.cx;
			aptl[3].y = sizl.cy;

			GpiWCBitBlt(init.shadowhps,hbm,4L,aptl,ROP_SRCCOPY,BBO_IGNORE);
			GpiDeleteBitmap(hbm);
			GpiDestroyPS(hps);
		}
		DosFreeSeg(sel);
	}
	else {
		mfoptions[PMF_SEGBASE] = 0;
		mfoptions[PMF_LOADTYPE] = LT_ORIGINALVIEW;
		mfoptions[PMF_RESOLVE] = RS_DEFAULT;
		mfoptions[PMF_LCIDS] = LC_LOADDISC;
		mfoptions[PMF_RESET] = options.fit == 1 ? RES_RESET : RES_NORESET;
		mfoptions[PMF_SUPPRESS] = SUP_NOSUPPRESS;
		mfoptions[PMF_COLORTABLES] = CTAB_NOMODIFY;
		mfoptions[PMF_COLORREALIZABLE] = CREA_NOREALIZE;
		GpiPlayMetaFile(init.shadowhps,hmf,8L,mfoptions,0L,0L,NULL);
		GpiDeleteMetaFile(hmf);
	}
	if (!options.fit) WinOffsetRect(dhab,&picrect,
		(SHORT)(saverinfo.screenrectl.xRight-
		saverinfo.screenrectl.xLeft-picrect.xRight+1>>1),
		(SHORT)(saverinfo.screenrectl.yTop-
		saverinfo.screenrectl.yBottom-picrect.yTop+1>>1));

	DosEnterCritSec();
	WinPostMsg(saverinfo.screenhwnd,WM_USER,0,0);
	WinTerminate(dhab); 
}

char far * far pascal aniextension(thismodule)
HMODULE thismodule;

{
	thismodule;
	return ".ANI";
}

char far * far pascal metextension(thismodule)
HMODULE thismodule;

{
	thismodule;
	return ".MET";
}

char far * far pascal bmpextension(thismodule)
HMODULE thismodule;

{
	thismodule;
	return ".BMP";
}
