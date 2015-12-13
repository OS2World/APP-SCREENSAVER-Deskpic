/***************************************************************************\
* BOUNCE.C - An example Desktop Screen Saver extension by John Ridges
\***************************************************************************/

#define MAXBITMAPS 25   /* How many happy faces can be on the screen */

#define INCL_DOSMEMMGR
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_GPIBITMAPS
#define INCL_WINBUTTONS
#define INCL_WINDIALOGS
#define INCL_WINENTRYFIELDS
#define INCL_WININPUT
#define INCL_WINMESSAGEMGR
#define INCL_WINSHELLDATA
#define INCL_WINTIMER
#define INCL_WINWINDOWMGR

#include <os2.h>
#include <stdlib.h>   /* Needed because SRAND and RAND are called */

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

/*
   This is the record that describes the position and velocity of
   a happy face
*/
typedef struct {
	FIXED xvelocity,yvelocity;
	FIXED xposition,yposition;
} BITMAPREC;

/*
	This is the record that contains the options of the screen saver.
	These options are kept in the OS2.INI profile file.
*/
typedef struct {
	BOOL enabled;   /* The screen saver enabled status */
	int numbitmaps;   /* The number of happy faces on the screen */
} PROFILEREC;

static char name[] = "Bounce";  /* The name of this screen saver */
static BOOL gotprofile = FALSE;  /* Indicates that we've read the profile */
static PROFILEREC profile = { TRUE, 4 };   /* Default values */

char far * far pascal _loadds saverstatus(initptr,enabledptr)
SAVERBLOCK far *initptr;
BOOL far *enabledptr;
{
	SHORT i;
	
	/* Save the SAVERBLOCK address locally */
	saverinfo = initptr;

	/* Read the profile (but only once!) */
	if (!gotprofile) {
		i = sizeof(PROFILEREC);
		WinQueryProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,&i);
		gotprofile = TRUE;
	}
	
	/* Return the enabled status */
	*enabledptr = profile.enabled;
	
	/* Return the screen saver name */
	return name;
}

MRESULT CALLBACK saverdialog(hwnd,message,mp1,mp2)
HWND hwnd;
USHORT message;
MPARAM mp1,mp2;
{
	SHORT i;
	RECTL ourrect,desktoprect;

	switch(message) {
	case WM_INITDLG:

		/* Center the dialog inside of the desktop */
		WinQueryWindowRect(hwnd,&ourrect);
		WinQueryWindowRect(HWND_DESKTOP,&desktoprect);
		WinSetWindowPos(hwnd,NULL,
			(int)(desktoprect.xRight-ourrect.xRight+ourrect.xLeft)/2,
			(int)(desktoprect.yTop-ourrect.yTop+ourrect.yBottom)/2,
			0,0,SWP_MOVE);

		/* Check the enabled button if enabled */
		if (profile.enabled)
			WinSendDlgItemMsg(hwnd,3,BM_SETCHECK,MPFROMSHORT(1),0);

		/* Set the Quantity field */
		WinSetDlgItemShort(hwnd,4,profile.numbitmaps,TRUE);

		/* Bring up the dialog */
		WinShowWindow(hwnd,TRUE);
		return FALSE;
	case WM_COMMAND:

		/* If OK is pushed */
		if (SHORT1FROMMP(mp1) == 1) {

			/* Get the value of the Quantity field */
			WinQueryDlgItemShort(hwnd,4,&i,TRUE);

			/* Check to see if the Quantity is in bounds */
			if (i < 1 || i > MAXBITMAPS) {

				/* Bring up an error message box */
				WinMessageBox(HWND_DESKTOP,hwnd,"The number of happy faces must "
					"be between 1 and 25",NULL,0,MB_OK|MB_ICONHAND);

				/* Hilight the Quantity field */
				WinSendDlgItemMsg(hwnd,4,EM_SETSEL,MPFROM2SHORT(0,
					WinQueryDlgItemTextLength(hwnd,4)),0);

				/* Give the Quantity field the focus */
				WinSetFocus(HWND_DESKTOP,WinWindowFromID(hwnd,4));

				/* Don't exit the dialog */
				return FALSE;
			}
			/* Save the number of happy faces */
			profile.numbitmaps = i;

			/* Get the enabled status */
			profile.enabled =
				SHORT1FROMMR(WinSendDlgItemMsg(hwnd,3,BM_QUERYCHECK,0,0));

			/* Write the profile data */
			WinWriteProfileData(saverinfo->deskpichab,"Deskpic",name,&profile,
				sizeof(PROFILEREC));
		}
	}
	return WinDefDlgProc(hwnd,message,mp1,mp2);
}

void far pascal _loadds saverthread()
{
	HAB hab;
	HPS hps;
	int i;
	FIXED tempx,tempy;
	ULONG sqrt;
	POINTL aptl[4];
	SEL bitmapsel;
	BITMAPREC far *bitmaps;
	HBITMAP hbmp;

	/* Get an HAB for this thread (since we make PM calls) */
	hab = WinInitialize(0);

	/* Get an HPS of the screen */
	hps = WinGetPS(saverinfo->screenhwnd);

	/* Paint the screen black */
	WinFillRect(hps,&saverinfo->screenrectl,CLR_BLACK);

	/* Get memory for the array of BITMAPRECs */
	DosAllocSeg(sizeof(BITMAPREC)*profile.numbitmaps,&bitmapsel,SEG_NONSHARED);
	bitmaps = MAKEP(bitmapsel,0);

	/* Flag all the happy faces as 'position unknown' */
	for (i = 0; i < profile.numbitmaps; i++) bitmaps[i].xposition = -1L;

	/* Get the handle to the happy face and its dimensions */
	hbmp = GpiLoadBitmap(hps,saverinfo->thismodule,1,0L,0L);
	aptl[2].x = aptl[2].y = 0;
	aptl[3].x = aptl[3].y = 34;

	/* Randomize RAND using the time */
	srand((unsigned int)WinGetCurrentTime(hab));

	/* Loop until DESKPIC tells us to stop */
	while (!saverinfo->closesemaphore)

		/* Process for each happy face */
		for (i = 0; i < profile.numbitmaps; i++) {

		/* If the happy face's position is unknown, initialize it */
		if (bitmaps[i].xposition < 0) {

			/* Pick a random x velocity between -1 and 1 (fixed) */
			tempx = (long)rand()<<1;
			bitmaps[i].xvelocity = rand()&1 ? tempx : -tempx;

			/*
				Make the total velocity 1 by computing:
				yvelocity = sqrt(1 - xvelocity * xvelocity)
			*/
			tempy = MAKEFIXED(0,65535)-((ULONG)tempx*(ULONG)tempx>>16);

			/* Cheesy sqrt routine to avoid linking in floating point */
			sqrt = 0;
			tempx = 1L<<15;
			do {
				sqrt ^= tempx;
				if (sqrt*sqrt>>16 > (ULONG)tempy) sqrt ^= tempx;
				tempx >>= 1;
			} while (tempx);

			/* Randomly set y velocity sign */
			bitmaps[i].yvelocity = rand()&1 ? sqrt : -sqrt;

			/* Randomly choose the x and y position of the happy face */
			tempx = MAKEFIXED(saverinfo->screenrectl.xLeft,0)+(rand()*
				(saverinfo->screenrectl.xRight-saverinfo->screenrectl.xLeft-32)<<1);
			tempy = MAKEFIXED(saverinfo->screenrectl.yBottom,0)+(rand()*
				(saverinfo->screenrectl.yTop-saverinfo->screenrectl.yBottom-32)<<1);
		}
		else {
			/* Find the new position of the happy face */
			tempx = bitmaps[i].xposition+bitmaps[i].xvelocity;
			tempy = bitmaps[i].yposition+bitmaps[i].yvelocity;

			/* See if the happy face has hit an edge of the screen */
			if (FIXEDINT(tempx) < (int)saverinfo->screenrectl.xLeft) {

				/* Bounced off the left edge */
				tempx = (saverinfo->screenrectl.xLeft<<17)-tempx;
				bitmaps[i].xvelocity = -bitmaps[i].xvelocity;
			}
			else if (FIXEDINT(tempx) >= (int)saverinfo->screenrectl.xRight-32) {

				/* Bounced off the right edge */
				tempx = (saverinfo->screenrectl.xRight-32<<17)-tempx;
				bitmaps[i].xvelocity = -bitmaps[i].xvelocity;
			}
			if (FIXEDINT(tempy) < (int)saverinfo->screenrectl.yBottom) {

				/* Bounced off the bottom edge */
				tempy = (saverinfo->screenrectl.yBottom<<17)-tempy;
				bitmaps[i].yvelocity = -bitmaps[i].yvelocity;
			}
			else if (FIXEDINT(tempy) >= (int)saverinfo->screenrectl.yTop-32) {

				/* Bounced off the top edge */
				tempy = (saverinfo->screenrectl.yTop-32<<17)-tempy;
				bitmaps[i].yvelocity = -bitmaps[i].yvelocity;
			}
		}
		/* Draw the happy face in the new position */
		aptl[0].x = FIXEDINT(tempx)-1;
		aptl[0].y = FIXEDINT(tempy)-1;
		aptl[1].x = FIXEDINT(tempx)+32;
		aptl[1].y = FIXEDINT(tempy)+32;
		GpiWCBitBlt(hps,hbmp,4L,aptl,ROP_SRCCOPY,BBO_IGNORE);

		/* Save the position of the happy face */
		bitmaps[i].xposition = tempx;
		bitmaps[i].yposition = tempy;
	}
	/* Release the happy face bitmap */
	GpiDeleteBitmap(hbmp);

	/* Release the memory for the array of BITMAPRECs */
	DosFreeSeg(bitmapsel);

	/* Release the HPS of the screen */
	WinReleasePS(hps);

	/* Get rid of the HAB */
	WinTerminate(hab);

	/* Make sure the stack doesn't vanish before we're completely gone */
	DosEnterCritSec();

	/* Tell DESKPIC that we're gone */
	DosSemClear((HSEM)&saverinfo->closesemaphore);
}
