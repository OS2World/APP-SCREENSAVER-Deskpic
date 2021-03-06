/***********************************************************************\
* deskhook.c - Input queue monitor hook interface library
*
* This dynlink is needed because global input hooks must reside in a DLL.
\***********************************************************************/

#define INCL_WINHOOKS
#define INCL_WININPUT

#include <os2.h>

VOID PASCAL FAR _loadds QJournalRecordHook(HAB,PQMSG);
int volatile far * PASCAL FAR _loadds InstallDeskHook(HAB,HWND);
VOID PASCAL FAR _loadds ReleaseDeskHook(void);
VOID PASCAL near _loadds Initdll(HMODULE);

static HMODULE qhmod;              /* dynlink module handle      */
static HWND hwnd;                  /* window to send message to  */
static HAB volatile habt = NULL;   /* Anchor block               */
static int volatile status = 0;    /* global status variable     */

VOID PASCAL FAR _loadds QJournalRecordHook(hab,lpqmsg)
HAB hab;
PQMSG lpqmsg;
{
	static MPARAM mousepos = MPFROMLONG(-1);

	hab;
	if (lpqmsg->msg != WM_JOURNALNOTIFY) {
		if (lpqmsg->msg == WM_MOUSEMOVE) {
			if (mousepos == lpqmsg->mp1) return;
			mousepos = lpqmsg->mp1;
		}
		if (status == 2 || (status > 2 && lpqmsg->msg != WM_MOUSEMOVE)) {
			WinPostMsg(hwnd,WM_SEM1,0,0);
			status = 0;
		}
		else if (status) status--;
	}
}

int volatile far * PASCAL FAR _loadds InstallDeskHook(hab,thehwnd)
HAB hab;
HWND thehwnd;
{
	if (habt) {
		WinPostMsg(hwnd,WM_BUTTON1DBLCLK,0,0);
		return NULL;
	}
	hwnd = thehwnd;
	habt = hab;
	WinSetHook(hab,NULL,HK_JOURNALRECORD,(PFN)QJournalRecordHook,qhmod);
	return &status;
}

VOID PASCAL FAR _loadds ReleaseDeskHook()
{
	WinReleaseHook(habt,NULL,HK_JOURNALRECORD,(PFN)QJournalRecordHook,qhmod);
	habt = NULL;
}


VOID PASCAL near _loadds Initdll(hmod)
HMODULE hmod;
{
	qhmod = hmod;
}
