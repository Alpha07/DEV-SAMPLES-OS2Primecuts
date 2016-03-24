#pragma strings(readonly)

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR

#include <os2.h>

#include <malloc.h>
#include <memory.h>
#include <string.h>
#include <stdlib.h>

#include "StatusbarWindow.h"

#define TEXTLIMIT                        512

#define DB_RAISED    0x0400
#define DB_DEPRESSED 0x0800


typedef struct _WINDOWDATA
{
   PSZ pszText;
   ULONG cchText;
}WINDOWDATA, *PWINDOWDATA;


static MRESULT EXPENTRY WindowProcedure(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);


BOOL _Optlink registerStatusbarWindow(HAB hab)
{
   return WinRegisterClass(hab, WC_STATUSBAR, WindowProcedure, CS_SIZEREDRAW | CS_CLIPSIBLINGS, sizeof(PWINDOWDATA));
}



static MRESULT EXPENTRY WindowProcedure(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mReturn = 0;
   BOOL fHandled = TRUE;
   HPS hps;
   RECTL rect;
   PWINDOWDATA wd = (PWINDOWDATA)WinQueryWindowPtr(hwnd, 0);

   switch(msg)
   {
      case WM_CREATE:
         wd = (PWINDOWDATA)malloc(sizeof(WINDOWDATA));
         if(wd)
         {
            PCREATESTRUCT pCREATE = (PCREATESTRUCT)mp2;

            memset(wd, 0, sizeof(WINDOWDATA));
            wd->pszText = malloc(TEXTLIMIT);
            if(pCREATE->pszText)
            {
               wd->cchText = min(strlen(pCREATE->pszText), TEXTLIMIT);
               memcpy(wd->pszText, pCREATE->pszText, wd->cchText);
            }
            WinSetWindowPtr(hwnd, 0, wd);
         }
         break;

      case WM_PAINT:
         hps = WinBeginPaint(hwnd, NULLHANDLE, &rect);
         if(hps)
         {
            RECTL r;

            WinQueryWindowRect(hwnd, &r);

            WinDrawBorder(hps, &r, 1, 1, CLR_WHITE, CLR_BLACK, DB_RAISED);
            r.xLeft++;
            r.yBottom++;
            r.xRight--;
            r.yTop--;
            WinDrawBorder(hps, &r, 2, 2, CLR_PALEGRAY, CLR_PALEGRAY, 0UL);

            r.xLeft+=2;
            r.yBottom+=2;
            r.xRight-=2;
            r.yTop-=2;
            WinDrawBorder(hps, &r, 1, 1, CLR_WHITE, CLR_BLACK, DB_DEPRESSED);
            r.xLeft++;
            r.yBottom++;
            r.xRight--;
            r.yTop--;

            WinDrawText(hps, wd->cchText, wd->pszText, &r, CLR_BLACK, CLR_PALEGRAY, DT_LEFT | DT_VCENTER | DT_ERASERECT);

            WinEndPaint(hps);
         }
         break;

      case WM_SETWINDOWPARAMS:
         {
            PWNDPARAMS pwndparams = (PWNDPARAMS)mp1;

            switch(pwndparams->fsStatus)
            {
               case WPM_TEXT:
                  wd->cchText = min(pwndparams->cchText, TEXTLIMIT);
                  memcpy(wd->pszText, pwndparams->pszText, wd->cchText);
                  WinInvalidateRect(hwnd, NULL, FALSE);
                  mReturn = (MRESULT)TRUE;
                  break;

               default:
                  fHandled = FALSE;
                  break;
            }
         }
         break;

      case WM_DESTROY:
         free(wd->pszText);
         free(wd);
         break;

      default:
         fHandled = FALSE;
         break;
   }
   if(!fHandled)
   {
      mReturn = WinDefWindowProc(hwnd, msg, mp1, mp2);
   }
   return mReturn;
}
