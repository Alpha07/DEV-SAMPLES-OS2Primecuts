#pragma strings(readonly)

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINSCROLLBARS

#include <os2.h>

#include <malloc.h>
#include <memory.h>


#include "FrameSubProc.h"


typedef struct _WINDOWDATA
{
   PFNWP pfnOldProc;
   ULONG ulMaxWidth;                     /* Maximum width of frame */
   ULONG ulMaxHeight;                    /* Maximum height of frame */
   HWND hwndClient;
   HWND hwndVertScroll;
   HWND hwndHorzScroll;
}WINDOWDATA, *PWINDOWDATA;


/*
 * Function prototypes - Local
 */
static MRESULT EXPENTRY SubProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);



BOOL _Optlink subclassFrameWindow(HWND hwndFrame)
{
   BOOL fSuccess = FALSE;
   PWINDOWDATA wd = NULL;

   /*
    * Sub allocate frame window
    */
   wd = (PWINDOWDATA)malloc(sizeof(WINDOWDATA));
   if(wd)
   {
      memset(wd, 0, sizeof(WINDOWDATA));

      WinSetWindowULong(hwndFrame, QWL_USER, (ULONG)wd);

      wd->hwndClient = WinWindowFromID(hwndFrame, FID_CLIENT);

      /*
       * Create scrollbars. See #2 in notes.text
       */
      wd->hwndHorzScroll = WinCreateWindow(HWND_OBJECT, WC_SCROLLBAR, NULL, WS_VISIBLE | SBS_HORZ, 0L, 0L, 0L, 0L, hwndFrame, HWND_TOP, FID_HORZSCROLL, NULL, NULL);
      wd->hwndVertScroll = WinCreateWindow(HWND_OBJECT, WC_SCROLLBAR, NULL, WS_VISIBLE | SBS_VERT, 0L, 0L, 0L, 0L, hwndFrame, HWND_TOP, FID_VERTSCROLL, NULL, NULL);

      wd->pfnOldProc = WinSubclassWindow(hwndFrame, SubProc);
      if(wd->pfnOldProc != 0L)
      {
         fSuccess = TRUE;
      }
      else
      {
         free(wd);
      }
   }
   return fSuccess;
}


static MRESULT EXPENTRY SubProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mReturn = 0;
   BOOL fHandled = TRUE;
   PWINDOWDATA wd = (PWINDOWDATA)WinQueryWindowPtr(hwnd, 0);

   switch(msg)
   {
      case WM_TRACKFRAME:
         /*
          * In a perfect world, message is posted directly to the worker thread.
          */
         WinSendMsg(wd->hwndClient, WM_TRACKFRAME, mp1, MPVOID);

         /*
          * Must call old window procedure or tracking will be disabled.
          */
         return (*wd->pfnOldProc)(hwnd, msg, mp1, mp2);

      case WMU_QUERY_SCROLLBAR_HANDLES:
         /*
          * Since the scrollbars are created with HWND_OBJECT as their parents, the worker thread can not simply use
          * WinWindowFromID() to get the scrollbar handles. This messages allows the worker thread to get the
          * scrollbar handles in a secure fashion.
          */
         *(PULONG)mp1 = wd->hwndHorzScroll;
         *(PULONG)mp2 = wd->hwndVertScroll;
         break;

      case WM_DESTROY:
         (*wd->pfnOldProc)(hwnd, msg, mp1, mp2);

         /*
          * Reset subclassing and free window buffer
          */
         WinSubclassWindow(hwnd, wd->pfnOldProc);
         free(wd);
         break;

      default:
         fHandled = FALSE;
         break;
   }
   if(!fHandled)
   {
      mReturn = (*wd->pfnOldProc)(hwnd, msg, mp1, mp2);
   }
   return mReturn;
}
