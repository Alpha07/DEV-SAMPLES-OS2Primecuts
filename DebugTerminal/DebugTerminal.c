#pragma strings(readonly)

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINSHELLDATA
#define INCL_DOSFILEMGR
#define INCL_DOSPROCESS
#define INCL_DOSERRORS

#include <os2.h>

#include "TerminalWindow.h"
#include "res.h"


#include <stdlib.h>
#include <string.h>

#define privateStore (*_threadstore())


static HWND _Optlink createFrameWindow(HAB hAB);
static HWND _Optlink createClientWindow(HWND hwndFrame, int argc, char *argv[]);


VOID APIENTRY cleanup(ULONG ulVal);


typedef struct _THREADDATA
{
   HFILE hfLogFile;
   BOOL fLogFile;
}THREADDATA, *PTHREADDATA;

/*
 * argv[1] - pipe handle (required)
 * argv[2] - log filename (optional, default: debug terminal.log)
 */
int main(int argc, char *argv[])
{
   int iRet = 0;
   APIRET rc = NO_ERROR;
   HAB hAB = NULLHANDLE;
   HMQ hMQ = NULLHANDLE;
   HWND hwndFrame = NULLHANDLE;
   HWND hwndClient = NULLHANDLE;
   BOOL fSuccess = FALSE;
   PTHREADDATA threadData = NULL;
   char default_log[] = "debug terminal.log";
   char *pLog = default_log;

   if(argc < 2)
   {
      return 1;
   }

   privateStore = NULL;

   rc = DosExitList(EXLST_ADD, cleanup);
   if(rc == NO_ERROR)
   {
      PVOID pAlloc = NULL;
      HFILE hFile = NULLHANDLE;
      ULONG fsOpenFlags = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS;
      ULONG fsOpenMode = OPEN_SHARE_DENYNONE | OPEN_ACCESS_WRITEONLY;
      ULONG ulAction = 0;

      rc = DosAllocMem(&pAlloc, sizeof(THREADDATA), PAG_READ | PAG_WRITE | PAG_COMMIT);
      if(rc == NO_ERROR)
      {
         threadData = pAlloc;

         privateStore = threadData;
      }

      /* Open&Replace/Create debug file */
      if(threadData && argc > 2)
      {
         pLog = argv[2];
      }

      rc = DosOpen(pLog, &hFile, &ulAction, 0UL, FILE_NORMAL, fsOpenFlags, fsOpenMode, (PEAOP2)NULL);
      if(rc == NO_ERROR)
      {
         threadData->hfLogFile = hFile;
         threadData->fLogFile = TRUE;
      }
   }

   if(threadData == NULL)
   {
      return 1;
   }

   hAB = WinInitialize(0UL);

   if(hAB)
   {
      fSuccess = registerTerminalWindow(hAB);
   }

   if(fSuccess)
   {
      hMQ = WinCreateMsgQueue(hAB, 0L);
   }

   if(hMQ)
   {
      hwndFrame = createFrameWindow(hAB);
   }

   if(hwndFrame)
   {
      hwndClient = createClientWindow(hwndFrame, argc, argv);
   }

   if(hwndClient)
   {
      QMSG qmsg = { 0 };
      SWP swp = { 0 };
      ULONG cbSWP = sizeof(swp);

      if(PrfQueryProfileData(HINI_USERPROFILE, "PMDebugTerminal", "Window Position", &swp, &cbSWP))
      {
         WinSetWindowPos(hwndFrame, HWND_TOP, swp.x, swp.y, swp.cx, swp.cy, SWP_SIZE | SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
      }
      else
      {
         WinShowWindow(hwndFrame, TRUE);
      }

      while(WinGetMsg(hAB, &qmsg, (HWND)NULLHANDLE, 0UL, 0UL))
      {
         WinDispatchMsg(hAB, &qmsg);
      }
   }

   if(hAB)
   {
      if(hMQ)
      {
         if(hwndFrame)
         {
            WinDestroyWindow(hwndFrame);
         }
         WinDestroyMsgQueue(hMQ);
      }
      WinTerminate(hAB);
   }

   return iRet;
}


static HWND _Optlink createFrameWindow(HAB hAB)
{
   LONG lLength = 0;
   HWND hwndFrame = NULLHANDLE;
   char pszTitle[256] = "";

   lLength = WinLoadString(hAB, (HMODULE)NULLHANDLE, IDS_APPTITLE, sizeof(pszTitle), pszTitle);
   if(lLength != 0L)
   {
      FRAMECDATA fcd = { 0 };

      fcd.cb = sizeof(fcd);
      fcd.flCreateFlags = FCF_SIZEBORDER | FCF_SHELLPOSITION | FCF_TASKLIST | FCF_TITLEBAR | FCF_SYSMENU | FCF_MINMAX | FCF_HORZSCROLL | FCF_VERTSCROLL | FCF_MENU;
      fcd.hmodResources = NULLHANDLE;
      fcd.idResources = WIN_APPFRAME;
      hwndFrame = WinCreateWindow(HWND_DESKTOP, WC_FRAME, pszTitle, 0UL, 0L, 0L, 0L, 0L, (HWND)NULLHANDLE, HWND_TOP, fcd.idResources, &fcd, NULL);
   }
   return hwndFrame;
}


static HWND _Optlink createClientWindow(HWND hwndFrame, int argc, char *argv[])
{
   HWND hwndClient = NULLHANDLE;
   TERMCDATA tcd = { 0 };
   PTHREADDATA threadData = privateStore;

   tcd.cb = sizeof(tcd);
   tcd.rows = 1000;
   tcd.columns = 132;
   tcd.hDebugPipe = (HFILE)atol(argv[1]);
   tcd.fLogFile = threadData->fLogFile;
   tcd.hfLogFile = threadData->hfLogFile;

   hwndClient = WinCreateWindow(hwndFrame, WC_TERMINALWINDOW, NULL, 0UL, 0L, 0L, 0L, 0L, (HWND)hwndFrame, HWND_TOP, FID_CLIENT, &tcd, NULL);

   return hwndClient;
}


VOID APIENTRY cleanup(ULONG ulVal)
{
   PTHREADDATA threadData = privateStore;

   if(threadData)
   {
      if(threadData->fLogFile)
      {
         DosClose(threadData->hfLogFile);
      }
      DosFreeMem(threadData);
   }

   DosExitList(EXLST_EXIT, (PVOID)NULL);

   return;
}
