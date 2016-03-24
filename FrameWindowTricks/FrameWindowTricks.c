#pragma strings(readonly)

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINSYS


#ifdef DEBUG_TERM
#define INCL_DOSQUEUES
#define INCL_DOSSESMGR
#define INCL_DOSERRORS
#endif
#include <os2.h>

#ifdef DEBUG_TERM
#include <stdlib.h>
#include <stdio.h>
#include <memory.h>
static void launchDebugTerminal(void);
#endif


#include "StatusbarWindow.h"
#include "ClientWindow.h"

#include "resources.h"


/*
 * Function prototypes - external functions
 */
BOOL _Optlink registerClientWindow(HAB hab);
BOOL _Optlink registerStatusbarWindow(HAB hab);


/*
 * Function prototypes - local functions
 */
static HWND _Optlink createFrameWindow(HAB hab);
static HWND _Optlink createClientWindow(HAB hab, HWND hwndFrame);



int main(void)
{
   int iRet = 0;
   HAB hab = NULLHANDLE;
   HMQ hmq = NULLHANDLE;
   BOOL fSuccess = FALSE;
   HWND hwndFrame = NULLHANDLE;
   HWND hwndClient = NULLHANDLE;

   #ifdef DEBUG_TERM
   launchDebugTerminal();
   puts("i DebugTerminal ok.");
   #endif

   /*
    * Initialize PM
    */
   hab = WinInitialize(0UL);

   if(hab)
   {
      fSuccess = registerClientWindow(hab);
      if(fSuccess)
      {
         fSuccess = registerStatusbarWindow(hab);
      }
   }

   if(fSuccess)
   {
      hmq = WinCreateMsgQueue(hab, 0L);
   }

   if(hmq)
   {
      hwndFrame = createFrameWindow(hab);
   }

   if(hwndFrame)
   {
      hwndClient = createClientWindow(hab, hwndFrame);
   }

   if(hwndClient)
   {
      QMSG qmsg = { 0 };

      WinShowWindow(hwndFrame, TRUE);

      while(WinGetMsg(hab, &qmsg, (HWND)NULLHANDLE, 0UL, 0UL))
      {
         WinDispatchMsg(hab, &qmsg);
      }
   }

   if(hab)
   {
      if(hmq)
      {
         if(hwndFrame)
         {
            WinDestroyWindow(hwndFrame);
         }
         WinDestroyMsgQueue(hmq);
      }
      WinTerminate(hab);
   }

   return iRet;
}


static HWND _Optlink createFrameWindow(HAB hab)
{
   HWND hwndFrame = NULLHANDLE;
   LONG lLength = 0L;
   char achTitle[256] = "";

   lLength = WinLoadString(hab, NULLHANDLE, IDS_APPTITLE, sizeof(achTitle), achTitle);
   if(lLength != 0L)
   {
      FRAMECDATA fcd = { sizeof(fcd) };
      HWND hwndTmp = NULLHANDLE;

      fcd.flCreateFlags = FCF_SYSMENU | FCF_TITLEBAR | FCF_MINMAX | FCF_TASKLIST | FCF_SIZEBORDER | FCF_ICON | FCF_SHELLPOSITION | FCF_HORZSCROLL | FCF_VERTSCROLL;
      fcd.hmodResources = NULLHANDLE;
      fcd.idResources = WIN_APPFRAME;
      hwndTmp = WinCreateWindow(HWND_DESKTOP, WC_FRAME, achTitle, 0UL, 0L, 0L, 0L, 0L, (HWND)NULLHANDLE, HWND_TOP, fcd.idResources, &fcd, NULL);
      if(hwndTmp)
      {
         lLength = WinLoadString(hab, NULLHANDLE, IDS_APPTITLE, sizeof(achTitle), achTitle);
         if(lLength != 0L)
         {
            HWND hwndStatusbar = NULLHANDLE;

            lLength = WinLoadString(hab, NULLHANDLE, IDS_WELCOME_MESSAGE, sizeof(achTitle), achTitle);

            hwndStatusbar = WinCreateWindow(hwndTmp, WC_STATUSBAR, achTitle, WS_VISIBLE, 0L, 0L, 0L, 0L, hwndFrame, HWND_BOTTOM, FID_STATUSBAR, NULL, NULL);
            if(hwndStatusbar)
            {
               char font[] = "9.WarpSans";
               WinSetPresParam(hwndStatusbar, PP_FONTNAMESIZE, sizeof(font), font);
               hwndFrame = hwndTmp;
            }
         }
      }
      if(hwndFrame == NULLHANDLE)
      {
         WinDestroyWindow(hwndTmp);
      }
   }
   return hwndFrame;
}

static HWND _Optlink createClientWindow(HAB hab, HWND hwndFrame)
{
   HWND hwndClient = NULLHANDLE;

   hwndClient = WinCreateWindow(hwndFrame, WC_APPCLIENTCLASS, NULL, WS_VISIBLE, 0L, 0L, 0L, 0L, hwndFrame, HWND_TOP, FID_CLIENT, NULL, NULL);

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