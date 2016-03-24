#pragma strings(readonly)

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINSYS

#ifdef DEBUG_TERM
#define INCL_DOSPROCESS
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


#include <memory.h>

#include "resource.h"



extern BOOL _Optlink registerClientClass(HAB hAB);

/*
 * Function prototypes - local functions
 */
static HWND _Optlink createFrame(HAB hAB);
static HWND _Optlink createClient(HAB hAB, HWND hwndFrame);



int main(void)
{
   HAB hAB = NULLHANDLE;
   HMQ hMQ = NULLHANDLE;

#ifdef DEBUG_TERM
   launchDebugTerminal();
#endif

#ifdef DEBUG_TERM
   puts("i Initialize PM");
#endif

   hAB = WinInitialize(0);
   if(hAB)
   {
      hMQ = WinCreateMsgQueue(hAB, 0L);
   }

   registerClientClass(hAB);

   if(hMQ)
   {
      HWND hwndFrame = NULLHANDLE;
      HWND hwndClient = NULLHANDLE;

#ifdef DEBUG_TERM
      printf("i AnchorBlock handle: %08x\ni MessageQueue handle: %08x\n", hAB, hMQ);
#endif

      if(WinLoadString(hAB, NULLHANDLE, 0xdeadface, 0, NULL) == FALSE)
      {
#ifdef DEBUG_TERM
      printf("w Couldn't load resource string 0x%08x\n", 0xdeadface);
#endif
      }

      if(WinLoadString(hAB, NULLHANDLE, 0xdeadbeef, 0, NULL) == FALSE)
      {
#ifdef DEBUG_TERM
      printf("e Couldn't load resource string 0x%08x\n", 0xdeadbeef);
#endif
      }

      hwndFrame = createFrame(hAB);
      if(hwndFrame)
      {
         hwndClient = createClient(hAB, hwndFrame);
      }
      else
      {
#ifdef DEBUG_TERM
         puts("e createFrame() failed");
#endif
      }

      if(hwndClient)
      {
         QMSG qmsg = { 0 };

#ifdef DEBUG_TERM
         puts("i Frame and Client windows created successfully");
#endif

         WinSetWindowPos(hwndFrame, HWND_TOP, 42L, 100L, 290L, 160L, SWP_SIZE | SWP_MOVE | SWP_ZORDER | SWP_ACTIVATE | SWP_SHOW);

         while(WinGetMsg(hAB, &qmsg, (HWND)NULLHANDLE, 0UL, 0UL))
            WinDispatchMsg(hAB, &qmsg);

         WinDestroyWindow(hwndFrame);
      }
   }
   else
   {
#ifdef DEBUG_TERM
      puts("e Couldn't create messagequeue");
#endif
   }

#ifdef DEBUG_TERM
   puts("- end");
#endif

   if(hAB)
   {
      if(!WinTerminate(hAB))
      {
#ifdef DEBUG_TERM
         printf("e WinTerminate(%08x) failed in %s -> %s\n", hAB, __FILE__, __FUNCTION__);
#endif
      }
   }

   /*
    * Give application some time to empty the pipe
    */
#ifdef DEBUG_TERM
   DosSleep(100);
#endif

   return 0;
}


static HWND _Optlink createFrame(HAB hAB)
{
   LONG lLength = 0;
   char pszTitle[64] = "";
   HWND hWnd = NULLHANDLE;

   lLength = WinLoadString(hAB, (HMODULE)NULLHANDLE, IDS_APPTITLE, sizeof(pszTitle), pszTitle);
   if(lLength != 0L)
   {
      FRAMECDATA fcd = { 0 };

#ifdef DEBUG_TERM
      printf("i Resource string '%s' loaded successfully in %s -> %s\n", pszTitle, __FILE__, __FUNCTION__);
#endif

      fcd.cb = sizeof(fcd);
      fcd.flCreateFlags = FCF_SYSMENU | FCF_TITLEBAR | FCF_TASKLIST | FCF_DLGBORDER | FCF_MINMAX | FCF_ICON;
      fcd.hmodResources = NULLHANDLE;
      fcd.idResources = WIN_MAIN;

      hWnd = WinCreateWindow(HWND_DESKTOP, WC_FRAME, pszTitle, 0UL, 0L, 0L, 0L, 0L, (HWND)HWND_DESKTOP, HWND_TOP, fcd.idResources, &fcd, NULL);
   }
   return hWnd;
}


static HWND _Optlink createClient(HAB hAB, HWND hwndFrame)
{
   HWND hWnd = NULLHANDLE;
   LONG lLength = 0;
   char pszClientClass[64] = "";

   lLength = WinLoadString(hAB, (HMODULE)NULLHANDLE, IDS_CLIENTCLASS, sizeof(pszClientClass), pszClientClass);
   if(lLength != 0L)
   {
      BYTE buf[512] = "";
      PRESPARAMS *pp = buf;

      pp->cb = 4+4+10;
      pp->aparam[0].id = PP_FONTNAMESIZE;
      pp->aparam[0].cb = 10;
      memcpy(pp->aparam->ab, "9.WarpSans", 10);

      hWnd = WinCreateWindow(hwndFrame, pszClientClass, (PSZ)NULL, WS_VISIBLE, 0L, 0L, 0L, 0L, (HWND)hwndFrame, HWND_TOP, FID_CLIENT, NULL, pp);
   }
   else
   {
#ifdef DEBUG_TERM
      puts("e Couldn't load client classname string from resource");
#endif
   }

   return hWnd;
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
   memcpy((char*) PgmName, "DebugTerminal.exe\0", 18);

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