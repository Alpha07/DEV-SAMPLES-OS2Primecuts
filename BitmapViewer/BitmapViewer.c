#pragma strings(readonly)

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR

#ifdef DEBUG_TERM
#define INCL_DOSQUEUES
#define INCL_DOSSESMGR
#define INCL_DOSERRORS
#endif


#include <os2.h>

#include <process.h>


#ifdef DEBUG_TERM
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
static void launchDebugTerminal(void);
#endif

#include "CanvasWindow.h"
#include "WorkerThread.h"

#include "resources.h"



/*
 * Function prototypes - external functions
 */
BOOL _Optlink registerCanvasClass(HAB hab);
BOOL _Optlink subclassFrameWindow(HWND hwndFrame);
void _Optlink WorkerThread(void *param);


/*
 * Function prototypes - local functions
 */
static HWND createFrameWindow(HAB hab);
static HWND createClientWindow(HAB hab, HWND hwndFrame);


int main(int argc, char *argv[])
{
   int iRet = 0;
   HAB hab = NULLHANDLE;
   HMQ hmq = NULLHANDLE;
   BOOL fSuccess = FALSE;
   HWND hwndFrame = NULLHANDLE;
   HWND hwndClient = NULLHANDLE;
   int tidWorkerThread = -1;

   #ifdef DEBUG_TERM
   launchDebugTerminal();
   puts("DebugTerminal ok.");
   #endif


   /*
    * Initialize PM for thread
    */
   hab = WinInitialize(0UL);

   if(hab)
   {
      /*
       * Register custom window class with PM.
       */
      fSuccess = registerCanvasClass(hab);
   }

   if(fSuccess)
   {
      /*
       * Create message queue
       */
      hmq = WinCreateMsgQueue(hab, 0L);
   }

   if(hmq)
   {
      /*
       * Create application frame window
       */
      hwndFrame = createFrameWindow(hab);
   }

   if(hwndFrame)
   {
      /*
       * Create frame client window; this is the main canvas window
       */
      hwndClient = createClientWindow(hab, hwndFrame);
   }

   fSuccess = FALSE;
   if(hwndClient)
   {
      /*
       * Subclass the application frame window
       */
      fSuccess = subclassFrameWindow(hwndFrame);
   }

   if(fSuccess)
   {
      WRKRPARAMS threadParams = { 0 };

      /*
       * Launch Worker Thread. Note: This should only be done once everything else is up and running.
       */
      threadParams.hwndAppFrame = hwndFrame;
      threadParams.hwndCanvas = hwndClient;
      threadParams.argc = argc;
      threadParams.argv = argv;
      tidWorkerThread = _beginthread(WorkerThread, NULL, 32768, &threadParams);
      if(tidWorkerThread != -1)
      {
         WinPostMsg(hwndClient, WMU_CANVAS_NOTIFICATION, MPFROMLONG(WORKERTHREAD_TID), MPFROMLONG(tidWorkerThread));
      }
   }

   if(tidWorkerThread != -1)
   {
      QMSG qmsg = { 0 };

      /*
       * Enter thread message loop
       */
      while(WinGetMsg(hab, &qmsg, (HWND)NULLHANDLE, 0UL, 0UL))
      {
         WinDispatchMsg(hab, &qmsg);
      }

      WinDestroyWindow(hwndFrame);
   }

   /*
    * Terminate message queue and PM for thread
    */
   if(hab)
   {
      if(hmq)
      {
         WinDestroyMsgQueue(hmq);
      }
      WinTerminate(hab);
   }

   return iRet;
}




static HWND createFrameWindow(HAB hab)
{
   HWND hwndFrame = NULLHANDLE;
   LONG lLength = 0;
   char achTitle[256] = "";

   /*
    * Load application frame title
    */
   lLength = WinLoadString(hab, (HMODULE)NULLHANDLE, IDS_APPTITLE, sizeof(achTitle), achTitle);
   if(lLength != 0L)
   {
      FRAMECDATA fcd = { sizeof(fcd) };

      /*
       * Create frame window
       * See #1 in notes.text
       */
      fcd.flCreateFlags = FCF_SYSMENU | FCF_TASKLIST | FCF_TITLEBAR | FCF_SIZEBORDER | FCF_MINMAX | FCF_MENU | FCF_ACCELTABLE | FCF_ICON | FCF_NOBYTEALIGN;
      fcd.hmodResources = NULLHANDLE;
      fcd.idResources = WIN_APPFRAME;
      hwndFrame = WinCreateWindow(HWND_DESKTOP, WC_FRAME, achTitle, 0UL, 0L, 0L, 0L, 0L, (HWND)NULLHANDLE, HWND_TOP, fcd.idResources, &fcd, NULL);
   }
   return hwndFrame;
}


static HWND createClientWindow(HAB hab, HWND hwndFrame)
{
   HWND hwndClient = NULLHANDLE;
   LONG lLength = 0;
   char achClass[256] = "";

   lLength = WinLoadString(hab, (HMODULE)NULLHANDLE, IDS_CANVASCLASS, sizeof(achClass), achClass);
   if(lLength != 0L)
   {
      hwndClient = WinCreateWindow(hwndFrame, achClass, NULL, WS_VISIBLE, 0L, 0L, 0L, 0L, (HWND)hwndFrame, HWND_TOP, FID_CLIENT, NULL, NULL);
   }
   return hwndClient;
}



#ifdef DEBUG_TERM
static void launchDebugTerminal(void)
{
   APIRET rc = NO_ERROR;
   HFILE pipeDebugRead = 0;
   HFILE pipeDebugWrite = 0;
   STARTDATA sd = { 0 };
   ULONG SessID = 0;
   PID pid = 0;
   HFILE hfNew = 1;                      /* stdout */
   char PgmTitle[30] = "";
   char PgmName[100] = "";
   char szCommandLine[60] = "";
   char ObjBuf[200] = "";

   rc = DosCreatePipe(&pipeDebugRead, &pipeDebugWrite, 4096);
   if(rc != NO_ERROR)
   {
      DosBeep(1000, 100);
      exit(42);
   }

   _ultoa(pipeDebugRead, szCommandLine, 10);

   memset(&sd, 0, sizeof(sd));

   sd.Length = sizeof(STARTDATA);
   sd.Related = SSF_RELATED_CHILD;
   sd.FgBg = SSF_FGBG_BACK;
   sd.TraceOpt = SSF_TRACEOPT_NONE;
   memcpy((char*) PgmTitle, "Debug terminal\0", 15);
   sd.PgmTitle = PgmTitle;
   memcpy((char*) PgmName, "PMDebugTerminal.exe\0", 20);
   sd.PgmName = PgmName;
   sd.PgmInputs = szCommandLine;
   sd.InheritOpt = SSF_INHERTOPT_PARENT;
   sd.SessionType = SSF_TYPE_PM;
   sd.PgmControl = SSF_CONTROL_VISIBLE | SSF_CONTROL_SETPOS;
   sd.ObjectBuffer = ObjBuf;
   sd.ObjectBuffLen = sizeof(ObjBuf);

   rc = DosStartSession(&sd, &SessID, &pid);
   if(rc != NO_ERROR)
   {
      DosBeep(1000, 100);
      exit(43);
   }
   DosDupHandle(pipeDebugWrite, &hfNew);
}
#endif
