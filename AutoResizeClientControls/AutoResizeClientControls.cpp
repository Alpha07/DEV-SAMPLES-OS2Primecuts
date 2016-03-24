#define INCL_WINWINDOWMGR
#define INCL_DOSPROCESS

#include <os2.h>

#include <string.h>                      // memcpy()

#include "AutoResizeClientControls.h"


// doublelinkedlist structure
typedef struct _WNDLIST
{
   HWND           hWnd;
   RELWNDPOS      pos;
   struct _WNDLIST *next;
   struct _WNDLIST *prev;
}WNDLIST, *PWNDLIST;

// extra windowdata
#define QWP_WNDLIST                      0
#define QWP_CURRENT                      QWP_WNDLIST+sizeof(PWNDLIST)
#define QW_EXTRA                         QWP_CURRENT+sizeof(PWNDLIST)

// function prototypes
MRESULT EXPENTRY AutoSizeClientProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);
HWND AddControl(HWND hWnd, MPARAM mp1);
void ascpSize(HWND hWnd);
void ascpDestroy(HWND hWnd);


BOOL ascRegisterClass(HAB hAB)
{
   return WinRegisterClass(hAB, WC_AUTOSIZECLIENT, AutoSizeClientProc, CS_SIZEREDRAW, QW_EXTRA);
}

MRESULT EXPENTRY AutoSizeClientProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT  mReturn = 0;
   BOOL     fHandled = TRUE;

   switch(msg)
   {
      case WM_CREATE:
         WinSetWindowPtr(hWnd, QWP_WNDLIST, 0);
         WinSetWindowPtr(hWnd, QWP_CURRENT, 0);
         break;

      case WM_PAINT:
         {
            RECTL rect;
            HPS   hPS = WinBeginPaint(hWnd, NULLHANDLE, &rect);
            WinFillRect(hPS, &rect, CLR_PALEGRAY);
            WinEndPaint(hPS);
         }
         break;

      case WM_SIZE:
         ascpSize(hWnd);
         break;

      case WM_CLOSE:
         ascpDestroy(hWnd);
         WinPostMsg(hWnd, WM_QUIT, NULL, NULL);
         break;

      case WM_DESTROY:
         ascpDestroy(hWnd);
         break;

      case WMU_ARCC_ADD:
         return (MRESULT)AddControl(hWnd, mp1);

      default:
         fHandled = FALSE;
         break;
   }
   if(!fHandled)
      mReturn = WinDefWindowProc(hWnd, msg, mp1, mp2);

   return mReturn;
}

HWND AddControl(HWND hWnd, MPARAM mp1)
{
   HWND        hCntrl = NULLHANDLE;
   PWNDLIST    pList = NULL;
   PWNDLIST    pCurrent = NULL;
   PCREATEWND  pCreate = (PCREATEWND)mp1;
   RECTL       rect;

   pList = (PWNDLIST)WinQueryWindowPtr(hWnd, QWP_WNDLIST);
   pCurrent = (PWNDLIST)WinQueryWindowPtr(hWnd, QWP_CURRENT);

   WinQueryWindowRect(hWnd, &rect);

   ULONG x = ULONG(float(rect.xRight)*pCreate->pos.x);
   ULONG y = ULONG(float(rect.yTop)*pCreate->pos.y);
   ULONG x2 = ULONG(float(rect.xRight)*pCreate->pos.xx);
   ULONG y2 = ULONG(float(rect.yTop)*pCreate->pos.yy);

   hCntrl = WinCreateWindow(pCreate->hwndParent, pCreate->pszClass, pCreate->pszTitle, pCreate->flStyle, x+pCreate->pos.xAdd, y+pCreate->pos.yAdd, ((x2-x)+pCreate->pos.xxAdd)-pCreate->pos.xAdd, ((y2-y)+pCreate->pos.yyAdd)-pCreate->pos.yAdd, pCreate->hwndOwner, pCreate->hwndInsertBehind, pCreate->id, pCreate->pCtlData, pCreate->pPresParams);
   if(hCntrl == NULLHANDLE)
      return hCntrl;

   if(pList == NULL)
   {
      pCurrent = new WNDLIST;
      pCurrent->next = pCurrent;
      pCurrent->prev = pCurrent;
      WinSetWindowPtr(hWnd, QWP_WNDLIST, (PVOID)pCurrent);
   }
   else
   {
      pCurrent->next = new WNDLIST;
      pCurrent->next->prev = pCurrent;
      pCurrent->next->next = pList;
      pCurrent = pCurrent->next;
      pList->prev = pCurrent;
   }
   pCurrent->hWnd = hCntrl;
   memcpy(&pCurrent->pos, &pCreate->pos, sizeof(RELWNDPOS));
   WinSetWindowPtr(hWnd, QWP_CURRENT, (PVOID)pCurrent);

   return hCntrl;
}


void ascpSize(HWND hWnd)
{
   RECTL rect;
   ULONG x;
   ULONG y;
   ULONG x2;
   ULONG y2;

   PWNDLIST pList = (PWNDLIST)WinQueryWindowPtr(hWnd, QWP_WNDLIST);
   PWNDLIST pTmp  = pList;

   if(pList == NULL)
      return;

   WinQueryWindowRect(hWnd, &rect);

   do
   {
      x = ULONG(float(rect.xRight)*pTmp->pos.x);
      y = ULONG(float(rect.yTop)*pTmp->pos.y);
      x2 = ULONG(float(rect.xRight)*pTmp->pos.xx);
      y2 = ULONG(float(rect.yTop)*pTmp->pos.yy);

      WinSetWindowPos(pTmp->hWnd, HWND_TOP, x+pTmp->pos.xAdd, y+pTmp->pos.yAdd, ((x2-x)+pTmp->pos.xxAdd)-pTmp->pos.xAdd, ((y2-y)+pTmp->pos.yyAdd)-pTmp->pos.yAdd, SWP_SIZE | SWP_MOVE);
      pTmp = pTmp->next;
   }while(pTmp != pList);
}


void ascpDestroy(HWND hWnd)
{
   PWNDLIST pFirst = (PWNDLIST)WinQueryWindowPtr(hWnd, QWP_WNDLIST);
   PWNDLIST pEntry = pFirst;
   PWNDLIST pTmp = pEntry;

   if(pFirst == NULL)
      return;

   do
   {
      pEntry = pEntry->next;

      WinDestroyWindow(pTmp->hWnd);
      delete pTmp;

      pTmp = pEntry;

      DosSleep(0);
   }while(pTmp != pFirst);

   // reset playlist info
   WinSetWindowPtr(hWnd, QWP_WNDLIST, (PVOID)NULL);
   WinSetWindowPtr(hWnd, QWP_CURRENT, (PVOID)NULL);
}
