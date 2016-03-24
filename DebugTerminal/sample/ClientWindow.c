#pragma strings(readonly)

#define INCL_WINWINDOWMGR
#define INCL_WINMLE
#define INCL_WINBUTTONS
#define INCL_WINERRORS
#define INCL_GPILOGCOLORTABLE
#define INCL_DOSERRORS

#include <os2.h>

#include <memory.h>
#include <stdlib.h>

#ifdef DEBUG_TERM
#include <stdio.h>
#endif


#include "resource.h"


typedef struct _WINDOWDATA
{
   LONG lBGColor;
   HWND hwndMLE;
   HWND hwndButton1;
   HWND hwndButton2;
}WINDOWDATA, *PWINDOWDATA;


#define QWP_WINDOWDATA                   0
#define QWP_HEAP                         QWP_WINDOWDATA+sizeof(PWINDOWDATA)
#define QW_EXTRA                         QWP_HEAP+sizeof(PVOID)


static MRESULT EXPENTRY WindowProcedure(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);
static MRESULT _Optlink processCreateMessage(HWND hWnd, MPARAM mp1, MPARAM mp2);
static void _Optlink processPaintMessage(HWND hWnd);
static void _Optlink processRandomColorMessage(HWND hWnd);
static void _Optlink processRandomGrayMessage(HWND hWnd);
static void _Optlink processSizeMessage(HWND hWnd, MPARAM mp1, MPARAM mp2);
static void _Optlink processDestroyMessage(HWND hWnd);



BOOL _Optlink registerClientClass(HAB hAB)
{
   LONG lLength = 0;
   char pszClientClass[64] = "";
   BOOL fSuccess = FALSE;

   lLength = WinLoadString(hAB, (HMODULE)NULLHANDLE, IDS_CLIENTCLASS, sizeof(pszClientClass), pszClientClass);
   if(lLength != 0L)
   {
      fSuccess = WinRegisterClass(hAB, pszClientClass, WindowProcedure, CS_CLIPCHILDREN, QW_EXTRA);
   }
   return fSuccess;
}

static MRESULT EXPENTRY WindowProcedure(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mReturn = 0;
   BOOL fHandled = TRUE;

   switch(msg)
   {
      case WM_CREATE:
         processCreateMessage(hWnd, mp1, mp2);
         break;

      case WM_PAINT:
         processPaintMessage(hWnd);
         break;

      case WM_COMMAND:
         switch(SHORT1FROMMP(mp1))
         {
            case BN_RANDOM_COLOR:
               processRandomColorMessage(hWnd);
               break;

            case BN_RANDOM_GRAY:
               processRandomGrayMessage(hWnd);
               break;

            default:
               fHandled = FALSE;
               break;
         }
         break;

      case WM_SIZE:
         processSizeMessage(hWnd, mp1, mp2);
         break;

      case WM_DESTROY:
         processDestroyMessage(hWnd);
         break;

      default:
         fHandled = FALSE;
         break;
   }
   if(!fHandled)
      mReturn = WinDefWindowProc(hWnd, msg, mp1, mp2);

   return mReturn;
}

static MRESULT _Optlink processCreateMessage(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   BOOL fAbort = TRUE;
   APIRET rc = NO_ERROR;
   PWINDOWDATA pWindowData = NULL;
   PVOID pAlloc = NULL;

   rc = DosAllocMem(&pAlloc, sizeof(WINDOWDATA), PAG_READ | PAG_WRITE | PAG_COMMIT);
   if(rc == NO_ERROR)
   {
      LONG lLength = 0;
      ULONG flMLEstyle = MLS_BORDER | MLS_WORDWRAP | WS_VISIBLE;
      ULONG flBnStyle = BS_PUSHBUTTON | WS_VISIBLE;
      char pszTitle[64] = "";
      HAB hAB = WinQueryAnchorBlock(hWnd);

      pWindowData = pAlloc;

      memset(pWindowData, 0, sizeof(WINDOWDATA));

      pWindowData->hwndMLE = WinCreateWindow(hWnd, WC_MLE, NULL, flMLEstyle, 10, 10, 120, 50, hWnd, HWND_TOP, MLE_EDITOR, NULL, NULL);

      if(pWindowData->hwndMLE)
      {
         lLength = WinLoadString(hAB, (HMODULE)NULLHANDLE, IDS_BUTTON1, sizeof(pszTitle), pszTitle);
         if(lLength != 0L)
         {
            pWindowData->hwndButton1 = WinCreateWindow(hWnd, WC_BUTTON, pszTitle, flBnStyle, 170, 80, 100, 40, hWnd, HWND_TOP, BN_RANDOM_COLOR, NULL, NULL);
         }
      }

      if(pWindowData->hwndButton1)
      {
         lLength = WinLoadString(hAB, (HMODULE)NULLHANDLE, IDS_BUTTON2, sizeof(pszTitle), pszTitle);
         if(lLength != 0L)
         {
            pWindowData->hwndButton2 = WinCreateWindow(hWnd, WC_BUTTON, pszTitle, flBnStyle, 170, 50, 100, 40, hWnd, HWND_TOP, BN_RANDOM_GRAY, NULL, NULL);
         }
      }

      if(pWindowData->hwndButton2)
      {
         WinSetWindowPtr(hWnd, QWP_WINDOWDATA, pWindowData);

         fAbort = FALSE;
      }
      else
      {
#ifdef DEBUG_TERM
         puts("e processCreateMessage() failed");
#endif
         DosFreeMem(pWindowData);
      }
   }

   return (MRESULT)fAbort;
}

static void _Optlink processRandomColorMessage(HWND hWnd)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   LONG lRed = rand() % 256;
   LONG lGreen = rand() % 256;
   LONG lBlue = rand() % 256;
   LONG lColor = (lRed<<16) + (lGreen<<8) + lBlue;

#ifdef DEBUG_TERM
   printf("i New background RGB(%d,%d,%d)\n", lRed, lGreen, lBlue);
#endif

   pWindowData->lBGColor = lColor;

   WinInvalidateRect(hWnd, NULL, FALSE);
}

static void _Optlink processRandomGrayMessage(HWND hWnd)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   LONG lRed = rand() % 256;
   LONG lGreen = lRed;
   LONG lBlue = lGreen;
   LONG lColor = (lRed<<16) + (lGreen<<8) + lBlue;

#ifdef DEBUG_TERM
   printf("i New background Gray(%d)\n", lRed);
#endif

   pWindowData->lBGColor = lColor;

   WinInvalidateRect(hWnd, NULL, FALSE);
}

static void _Optlink processPaintMessage(HWND hWnd)
{
   RECTL rect = { 0, 0, 0, 0 };
   HPS hPS = WinBeginPaint(hWnd, NULLHANDLE, &rect);

   if(hPS)
   {
      PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);

      GpiCreateLogColorTable(hPS, 0UL, LCOLF_RGB, 0UL, 0UL, NULL);

      WinFillRect(hPS, &rect, pWindowData->lBGColor);

      WinEndPaint(hPS);
   }
   else
   {
#ifdef DEBUG_TERM
      printf("w WinBeginPaint() failed. PM error %u\n", LOUSHORT(WinGetLastError(WinQueryAnchorBlock(hWnd))));
#endif
   }
}

static void _Optlink processSizeMessage(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   SHORT scxold = SHORT1FROMMP(mp1);
   SHORT scyold = SHORT2FROMMP(mp1);
   SHORT scxnew = SHORT1FROMMP(mp2);
   SHORT scynew = SHORT2FROMMP(mp2);

#ifdef DEBUG_TERM
   printf("i Size changed (%d,%d) -> (%d,%d)\n", scxold, scyold, scxnew, scynew);
#endif

   WinSetWindowPos(WinWindowFromID(hWnd, MLE_EDITOR),      HWND_TOP,  10, 10, 130, 100, SWP_MOVE | SWP_SIZE);

   WinSetWindowPos(WinWindowFromID(hWnd, BN_RANDOM_COLOR), HWND_TOP, 150, 110-37, 120, 32, SWP_MOVE | SWP_SIZE);

   WinSetWindowPos(WinWindowFromID(hWnd, BN_RANDOM_GRAY),  HWND_TOP, 150, 15, 120, 32, SWP_MOVE | SWP_SIZE);
}

static void _Optlink processDestroyMessage(HWND hWnd)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);

#ifdef DEBUG_TERM
   puts("i Destroying window");
#endif

   DosFreeMem(pWindowData);
}
