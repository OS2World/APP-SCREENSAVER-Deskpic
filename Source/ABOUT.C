/***************************************************************************\
* Module Name: about.c
\***************************************************************************/

#define NUMBEROFLINES 74

#define INCL_GPILCIDS
#define INCL_WINLISTBOXES
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSYS
#define INCL_WINWINDOWMGR

#include <os2.h>
#include "deskpic.h"

MRESULT EXPENTRY _export aboutdialog(HWND, USHORT, MPARAM, MPARAM);
MRESULT EXPENTRY _export aboutlist(HWND, USHORT, MPARAM, MPARAM);

static PFNWP near oldlist;
static char far abouttext[] =

" \n\tDesktop Picture / Screen Saver\n"
"\tVersion 1.32\n\n"
"\tBy John Ridges\n\n\n"
"\x10INSTALLATION\x11\n"
"Place DESKPIC.EXE in your path, DESKPIC.DLL in your libpath. "
"Any Picture Reader extensions (.DPR files) or Screen Saver extensions "
"(.DSS files) should be placed in the same directory as DESKPIC.EXE. "
"All pictures (.BMP, .GIF, .PCX, and .MET files) and Animated Desktop "
"extensions (.ANI files) should be placed in a single directory that is "
"specified with the options dialog.\n\n"
"\x10INVOCATION\x11\n"
"Invoke DESKPIC with \"START DESKPIC [<path name>]\". The optional "
"parameter <path name> is a picture file name that will temporarily override "
"the picture path in the options dialog.\n\n"
"\x10KEYS\x11\n"
"When you click on the desktop to give DESKPIC the focus, the following keys "
"can be used:\n"
"\007F\x07 Brings the picture forward. Another F or mouse movement "
"returns it to the background.\n"
"\007P\x07 If the picture is an Animated Desktop, the animation "
"is temporarily suspended. Another P allows it to continue.\n"
"\007F3\x07 Terminates DESKPIC.\n\n"
"\x10OPTIONS DIALOG\x11";

static char far kludge[] =

"Double-click on the desktop picture to bring up the options dialog.\n"
"\007Picture path\x07 Specifies the location of the picture files and "
"Animated Desktop extensions.\n"
"\007Browse\x07 Use this button to set the picture path.\n"
"\007Info\x07 Press this button to bring up information about the current "
"picture.\n"
"\007Timeout\x07 Specifies the period of inactivity before the screen saver "
"is activated. A key or mouse movement dismisses the screen saver.\n"
"\007Saver now corner\x07 Place the mouse pointer in this corner to "
"activate the screen saver.\n"
"\007Saver never corner\x07 Place the mouse pointer in this corner to "
"keep the screen saver from being activated.\n"
"\007Saver list\x07 A check mark in front of a screen saver name indicates "
"that the screen saver is enabled.\n"
"\007Configure\x07 Press this button (or double click on the screen saver "
"name) to bring up a control dialog for the selected screen saver.\n"
"\007Test\x07 Activates the selected screen saver.\n"
"\007Single saver\x07 When checked, only the selected screen saver will be "
"activated. Otherwise an enabled screen saver is chosen at random.\n\n\n"
"DESKPIC has been released into the public domain. There is no warranty "
"either expressed or implied. Use DESKPIC at your own risk.\n\n"
"Many thanks to the Antman for help with reading picture files."

"\0\r";

MRESULT EXPENTRY _export aboutdialog(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;

{
	int i;
	char far *txt;
	HPS hps;
	BOOL centered;
	RECTL rect;
	FONTMETRICS fm;
	static int near sofar;
	static char far * near pnt;

	switch(message) {
	case WM_INITDLG:
		sofar = 0;
		pnt = abouttext;
		WinQueryWindowRect(hwnd,&rect);
		WinSetWindowPos(hwnd,NULL,
			(int)(WinQuerySysValue(HWND_DESKTOP,SV_CXSCREEN)-rect.xRight+rect.xLeft)/2,
			(int)(WinQuerySysValue(HWND_DESKTOP,SV_CYSCREEN)-rect.yTop+rect.yBottom)/2,
			0,0,SWP_MOVE);
		oldlist = WinSubclassWindow(WinWindowFromID(hwnd,IDA_LIST),aboutlist);
		for (i = 0; i < NUMBEROFLINES; i++)
			WinSendDlgItemMsg(hwnd,IDA_LIST,LM_INSERTITEM,MPFROMSHORT(LIT_END),
				MPFROMP(""));
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_DRAWITEM:
		if (((POWNERITEM)mp2)->fsState == ((POWNERITEM)mp2)->fsStateOld) {
			txt = (PSZ)(((POWNERITEM)mp2)->hItem);
			for (; sofar <= ((POWNERITEM)mp2)->idItem; sofar++) {
				if (sofar == ((POWNERITEM)mp2)->idItem) txt = pnt;
				WinSendDlgItemMsg(hwnd,IDA_LIST,LM_SETITEMHANDLE,
					MPFROMSHORT(sofar),MPFROMP(pnt));
				rect = ((POWNERITEM)mp2)->rclItem;
				rect.xLeft += 3;
				rect.xRight -= 3;
				centered = *pnt == '\t';
				pnt += centered+WinDrawText(((POWNERITEM)mp2)->hps,0xffff,
					pnt+centered,&rect,0L,0L,centered ?
					DT_CENTER|DT_BOTTOM|DT_QUERYEXTENT|DT_WORDBREAK :
					DT_LEFT|DT_BOTTOM|DT_QUERYEXTENT|DT_WORDBREAK);
				if (!*pnt) {
					if (*(pnt+1) != '\r') pnt++;
				}
				else *(pnt-1) = 0;
			}
			rect = ((POWNERITEM)mp2)->rclItem;
			rect.xLeft += 3;
			rect.xRight -= 3;
			centered = *txt == '\t';
			WinDrawText(((POWNERITEM)mp2)->hps,0xffff,txt+centered,&rect,
				SYSCLR_WINDOWSTATICTEXT,SYSCLR_WINDOW,centered ?
				DT_CENTER|DT_BOTTOM|DT_ERASERECT : DT_LEFT|DT_BOTTOM|DT_ERASERECT);
		}
		((POWNERITEM)mp2)->fsState = ((POWNERITEM)mp2)->fsStateOld = FALSE;
		return MRFROMSHORT(TRUE);
	case WM_MEASUREITEM:
		hps = WinGetPS(WinWindowFromID(hwnd,IDA_LIST));
		GpiQueryFontMetrics(hps,sizeof(FONTMETRICS),&fm);
		WinReleasePS(hps);
		return MRFROMSHORT(fm.lMaxAscender+fm.lMaxDescender);
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

MRESULT EXPENTRY _export aboutlist(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;

{
	MRESULT mr;
	
	mr = (oldlist)(hwnd,message,mp1,mp2);
	WinShowCursor(hwnd,FALSE);
	return mr;
}
