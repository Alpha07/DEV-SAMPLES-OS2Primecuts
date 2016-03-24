#pragma strings(readonly)

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINTRACKRECT
#define INCL_WINSYS
#define INCL_WININPUT
#define INCL_WINSCROLLBARS
#define INCL_WINTIMER
#define INCL_WINRECTANGLES
#define INCL_WINPOINTERS
#define INCL_GPILOGCOLORTABLE

#include <os2.h>

#include <malloc.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "ClientWindow.h"
#include "StatusbarWindow.h"


#define MAPCX  25
#define MAPCY  25
#define SQCX   32
#define SQCY   32




typedef struct MAPCELL
{
   LONG lColor;
}MAPCELL, *PMAPCELL;

typedef struct _WINDOWDATA
{
   HAB hab;
   POINTL ptlCursor;
   SIZEL sizlWindow;
   MAPCELL map[MAPCY][MAPCX];
   HWND hwndStatusbar;
   LONG lHorzScroll;
   LONG lVertScroll;
   HWND hwndVertScroll;
   HWND hwndHorzScroll;
   BOOL fShowCursor;
   USHORT fsLastFrameTrack;
}WINDOWDATA, *PWINDOWDATA;



typedef struct _FRAMESUBPROCPARAMS
{
   PFNWP pfnOldProc;
   ULONG ulMaxWidth;                     /* Maximum width of frame  */
   ULONG ulMaxHeight;                    /* Maximum height of frame */
   LONG cyStatusBar;
   LONG cyScrollBar;
   SIZEL sizlCell;                       /* Cell size            */
   HWND hwndClient;                      /* Client window handle */
   HWND hwndVertScroll;
   HWND hwndHorzScroll;
}FRAMESUBPROCPARAMS, *PFRAMESUBPROCPARAMS;


#define TID_CHECK_CURSOR                 10



static MRESULT EXPENTRY WindowProcedure(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

void _Inline invalidate_square(HWND hwnd, PWINDOWDATA wd, PPOINTL ptl);

static MRESULT EXPENTRY FrameSubProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);



#define WMU_MAX_CLIENT_SIZE              WM_USER+1


BOOL _Optlink registerClientWindow(HAB hab)
{
   return WinRegisterClass(hab, WC_APPCLIENTCLASS, WindowProcedure, CS_CLIPCHILDREN | CS_SIZEREDRAW, sizeof(PWINDOWDATA));
}


static MRESULT EXPENTRY WindowProcedure(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mReturn = 0;
   BOOL fHandled = TRUE;
   PWINDOWDATA wd = (PWINDOWDATA)WinQueryWindowPtr(hwnd, 0);
   HPS hps;
   RECTL rect;
   SHORT scxold;
   SHORT scyold;
   SHORT scxnew;
   SHORT scynew;
   POINTL ptl;
   USHORT usidentifier;
   SHORT sslider;
   USHORT uscmd;

   switch(msg)
   {
      case WM_CREATE:
         wd = (PWINDOWDATA)malloc(sizeof(WINDOWDATA));
         if(wd)
         {
            HWND hwndFrame = WinQueryWindow(hwnd, QW_PARENT);
            PFRAMESUBPROCPARAMS fspp = NULL;
            POINTL p = { 0, 0 };
            HAB hab = WinQueryAnchorBlock(hwnd);

            memset(wd, 0, sizeof(WINDOWDATA));

            wd->hab = hab;
            wd->hwndStatusbar = WinWindowFromID(hwndFrame, FID_STATUSBAR);
            wd->hwndHorzScroll = WinWindowFromID(hwndFrame, FID_HORZSCROLL);
            wd->hwndVertScroll = WinWindowFromID(hwndFrame, FID_VERTSCROLL);

            /*
             * Set up initial map
             */
            for(p.y = 0; p.y < MAPCY; p.y++)
            {
               for(p.x = 0; p.x < MAPCX; p.x++)
               {
                  wd->map[p.y][p.x].lColor = ((p.x*8)<<16)+(p.y*8);
               }
            }

            /*
             * Calculate client window size from frame window
             */
            WinQueryWindowRect(hwnd, &rect);
            wd->sizlWindow.cx = rect.xRight-rect.xLeft;
            wd->sizlWindow.cy = rect.yTop-rect.yBottom;

            sslider = 0;
            wd->lHorzScroll = sslider;
            WinSendMsg(wd->hwndHorzScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(wd->sizlWindow.cx, MAPCX*SQCX), MPVOID);
            WinSendMsg(wd->hwndHorzScroll, SBM_SETSCROLLBAR, MPFROMSHORT(sslider), MPFROM2SHORT(0, MAPCX*SQCX-wd->sizlWindow.cx));

            sslider = MAPCY*SQCY;
            wd->lVertScroll = MAPCY*SQCY-sslider;
            WinSendMsg(wd->hwndVertScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(wd->sizlWindow.cy, MAPCY*SQCY), MPVOID);
            WinSendMsg(wd->hwndVertScroll, SBM_SETSCROLLBAR, MPFROMSHORT(sslider), MPFROM2SHORT(wd->sizlWindow.cy, MAPCY*SQCY));

            /*
             * Sub allocate frame window
             */
            fspp = (PFRAMESUBPROCPARAMS)malloc(sizeof(FRAMESUBPROCPARAMS));
            if(fspp)
            {
               memset(fspp, 0, sizeof(FRAMESUBPROCPARAMS));

               WinSetWindowULong(hwndFrame, QWL_USER, (ULONG)fspp);

               fspp->cyStatusBar = WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);
               fspp->cyScrollBar = WinQuerySysValue(HWND_DESKTOP, SV_CYHSCROLL);
               fspp->pfnOldProc = WinSubclassWindow(hwndFrame, FrameSubProc);
               fspp->sizlCell.cx = SQCX;
               fspp->sizlCell.cy = SQCY;
               fspp->hwndClient = hwnd;
               if(fspp->pfnOldProc == 0L)
               {
                  free(fspp);
               }
               else
               {
                  /*
                   * Maximum size of frame from max client size
                   * This needs to be done _after_ subproc. is done since
                   * the subproc. hadles the WM_CALCFRAMERECT message.
                   */
                  WinSendMsg(hwndFrame, WMU_MAX_CLIENT_SIZE, MPFROMLONG(MAPCX*SQCX), MPFROMLONG(MAPCY*SQCY));
               }
            }

            WinStartTimer(hab, hwnd, TID_CHECK_CURSOR, 100);

            WinSetWindowPtr(hwnd, 0, wd);

            mReturn = (MRESULT)FALSE;
         }
         break;

      case WM_PAINT:
         hps = WinBeginPaint(hwnd, NULLHANDLE, &rect);
         if(hps)
         {
            RECTL rclMap = { rect.xLeft+wd->lHorzScroll,
                             rect.yBottom+wd->lVertScroll,
                             rect.xRight+wd->lHorzScroll,
                             rect.yTop+wd->lVertScroll };
            LONG xSquareMin = rclMap.xLeft / SQCX;
            LONG xSquareMax = rclMap.xRight / SQCX;
            LONG ySquareMin = rclMap.yBottom / SQCY;
            LONG ySquareMax = rclMap.yTop / SQCY;
            POINTL p = { 0, 0 };

            GpiCreateLogColorTable(hps, LCOL_RESET, LCOLF_RGB, 0UL, 0UL, NULL);

            if(rclMap.xRight % SQCX)
            {
               xSquareMax++;
            }
            if(rclMap.yTop % SQCY)
            {
               ySquareMax++;
            }

            for(p.y = ySquareMin; p.y < ySquareMax; p.y++)
            {
               for(p.x = xSquareMin; p.x < xSquareMax; p.x++)
               {
                  RECTL rclPaint = { p.x*SQCX-wd->lHorzScroll,
                                     p.y*SQCY-wd->lVertScroll,
                                     rclPaint.xLeft+SQCX,
                                     rclPaint.yBottom+SQCY };

                  WinFillRect(hps, &rclPaint, wd->map[p.y][p.x].lColor);

                  if(p.x == wd->ptlCursor.x && p.y == wd->ptlCursor.y && wd->fShowCursor)
                  {
                     POINTL ptl = { rclPaint.xLeft, rclPaint.yBottom };

                     GpiSetColor(hps, 0x0011ffff);

                     GpiMove(hps, &ptl);
                     ptl.x = rclPaint.xRight-1;
                     ptl.y = rclPaint.yTop-1;
                     GpiBox(hps, DRO_OUTLINE, &ptl, 20, 20);
                  }
               }
            }
            WinEndPaint(hps);
         }
         break;

      case WM_SIZE:
         scxold = SHORT1FROMMP(mp1);
         scyold = SHORT2FROMMP(mp1);
         scxnew = SHORT1FROMMP(mp2);
         scynew = SHORT2FROMMP(mp2);

         if(scxnew != scxold)
         {
            sslider = (SHORT)WinSendMsg(wd->hwndHorzScroll, SBM_QUERYPOS, MPVOID, MPVOID);

            wd->sizlWindow.cx = scxnew;

            /*
             * Exclude any frame movements and make sure left frame border was tracked
             */
            if((wd->fsLastFrameTrack ^ TF_MOVE) && (wd->fsLastFrameTrack & TF_LEFT))
            {
               if(scxnew < scxold)
               {
                  /*
                   * Window is shrinking horizontally
                   */
                  sslider += (scxold-scxnew);
               }
               else
               {
                  /*
                   * Window is growing horizontally
                   */
                  sslider -= (scxnew-scxold);
               }
            }

            sslider = min((MAPCX*SQCX)-scxnew, sslider);
            sslider = max(0, sslider);
            wd->lHorzScroll = sslider;

            WinSendMsg(wd->hwndHorzScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(scxnew, MAPCX*SQCX), MPVOID);
            WinSendMsg(wd->hwndHorzScroll, SBM_SETSCROLLBAR, (MPARAM)sslider, MPFROM2SHORT(0, MAPCX*SQCX-scxnew));
         }

         if(scynew != scyold)
         {
            sslider = (SHORT)WinSendMsg(wd->hwndVertScroll, SBM_QUERYPOS, MPVOID, MPVOID);

            wd->sizlWindow.cy = scynew;

            if((wd->fsLastFrameTrack ^ TF_MOVE) && (wd->fsLastFrameTrack & TF_BOTTOM))
            {
               if(scynew < scyold)
               {
                  /*
                   * Window is shrinking vertically
                   */
                  sslider -= (scyold-scynew);
               }
               else
               {
                  /*
                   * Window is growing vertically
                   */
                  sslider += (scynew-scyold);
               }
            }

            /*
             * Make sure no visible part of map is out of bound
             */
            sslider = min(MAPCY*SQCY, sslider);
            sslider = max(scynew, sslider);
            wd->lVertScroll = (MAPCY*SQCY)-sslider;

            WinSendMsg(wd->hwndVertScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(scynew, MAPCY*SQCY), MPVOID);
            WinSendMsg(wd->hwndVertScroll, SBM_SETSCROLLBAR, (MPARAM)sslider, MPFROM2SHORT(scynew, MAPCY*SQCY));
         }

         wd->fsLastFrameTrack  = 0;
         break;

      case WM_HSCROLL:
         usidentifier = SHORT1FROMMP(mp1);
         sslider = SHORT1FROMMP(mp2);
         uscmd = SHORT2FROMMP(mp2);

         switch(uscmd)
         {
            case SB_SLIDERTRACK:
               wd->lHorzScroll = sslider;
               WinSendMsg(wd->hwndHorzScroll, SBM_SETPOS, MPFROMSHORT(sslider), MPVOID);
               WinInvalidateRect(hwnd, NULL, FALSE);
               break;
         }
         break;

      case WM_VSCROLL:
         usidentifier = SHORT1FROMMP(mp1);
         sslider = SHORT1FROMMP(mp2);
         uscmd = SHORT2FROMMP(mp2);

         switch(uscmd)
         {
            case SB_SLIDERTRACK:
               wd->lVertScroll = (MAPCY*SQCY)-sslider;
               WinSendMsg(wd->hwndVertScroll, SBM_SETPOS, MPFROMSHORT(sslider), MPVOID);
               WinInvalidateRect(hwnd, NULL, FALSE);
               break;
         }
         break;

      case WM_MOUSEMOVE:
         ptl.x = (SHORT1FROMMP(mp1)+wd->lHorzScroll)/SQCX;
         ptl.y = (SHORT2FROMMP(mp1)+wd->lVertScroll)/SQCY;
         if(memcmp(&ptl, &wd->ptlCursor, sizeof(POINTL)) != 0)
         {
            char buf[256] = "";

            wd->fShowCursor = TRUE;

            invalidate_square(hwnd, wd, &wd->ptlCursor);

            memcpy(&wd->ptlCursor, &ptl, sizeof(POINTL));

            invalidate_square(hwnd, wd, &wd->ptlCursor);

            sprintf(buf, "New position (%d,%d)", ptl.x+1, ptl.y+1);
            WinSetWindowText(wd->hwndStatusbar, buf);
         }
         fHandled = FALSE;
         break;

      case WM_BUTTON1CLICK:
         {
            PPOINTS pts = (PPOINTS)&mp1; /* Not used here */
            char buf[256] = "";

            wd->map[wd->ptlCursor.y][wd->ptlCursor.x].lColor = ((rand()%0xff)<<16) | ((rand()%0xff)<<8) | (rand()%0xff);

            invalidate_square(hwnd, wd, &wd->ptlCursor);

            sprintf(buf, "Color changed to 0x%08x at (%d,%d)", wd->map[wd->ptlCursor.y][wd->ptlCursor.x].lColor, wd->ptlCursor.x+1, wd->ptlCursor.y+1);
            WinSetWindowText(wd->hwndStatusbar, buf);

            mReturn = (MRESULT)TRUE;
         }
         break;

      case WM_TIMER:
         switch(SHORT1FROMMP(mp1))
         {
            case TID_CHECK_CURSOR:
               WinQueryPointerPos(HWND_DESKTOP, &ptl);

               WinMapWindowPoints(HWND_DESKTOP, hwnd, &ptl, 1);

               WinQueryWindowRect(hwnd, &rect);
               if(WinPtInRect(wd->hab, &rect, &ptl))
               {
                  if(wd->fShowCursor == FALSE)
                  {
                     wd->fShowCursor = TRUE;
                     invalidate_square(hwnd, wd, &wd->ptlCursor);
                  }
               }
               else
               {
                  if(wd->fShowCursor == TRUE)
                  {
                     wd->fShowCursor = FALSE;
                     invalidate_square(hwnd, wd, &wd->ptlCursor);
                  }
               }
               break;

            default:
               fHandled = FALSE;
               break;
         }
         break;

      case WM_TRACKFRAME:
         /*
          * Keep track of which part of the frame is being tracked; this way certain
          * estetic adjustments can be made
          */
         #ifdef DEBUG_TERM
         puts("i Client window WM_TRACKFRAME");
         #endif
         wd->fsLastFrameTrack = (SHORT1FROMMP(mp1) & TF_MOVE);
         break;

      case WM_DESTROY:
         WinStopTimer(wd->hab, hwnd, TID_CHECK_CURSOR);
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

void _Inline invalidate_square(HWND hwnd, PWINDOWDATA wd, PPOINTL ptl)
{
   RECTL r = { ptl->x*SQCX-wd->lHorzScroll, ptl->y*SQCY-wd->lVertScroll, r.xLeft+SQCX, r.yBottom+SQCY };

   WinInvalidateRect(hwnd, &r, FALSE);
}




static MRESULT EXPENTRY FrameSubProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mReturn = 0;
   BOOL fHandled = TRUE;
   PFRAMESUBPROCPARAMS wd = (PFRAMESUBPROCPARAMS)(ULONG)WinQueryWindowULong(hwnd, QWL_USER);
   PTRACKINFO pti;
   SHORT cControls;
   PSWP pswp;
   RECTL rect;

   switch(msg)
   {
      case WM_CALCFRAMERECT:
         mReturn = (*wd->pfnOldProc)(hwnd, msg, mp1, mp2);
         if(mReturn)
         {
            PRECTL pRect;

            if(SHORT1FROMMP(mp2))
            {
               pRect = (PRECTL)mp1;
               pRect->yBottom += wd->cyStatusBar;
               mReturn = (MRESULT)TRUE;
            }
            else
            {
               pRect = (PRECTL)mp1;
               pRect->yBottom -= wd->cyStatusBar;
               mReturn = (MRESULT)TRUE;
            }
         }
         break;

      case WM_QUERYFRAMECTLCOUNT:
         cControls = SHORT1FROMMR((*wd->pfnOldProc)(hwnd, msg, mp1, mp2));
         return ((MRESULT)(cControls+1));

      case WM_FORMATFRAME:
         {
            USHORT iClient = 0;
            USHORT iStatusBar = 0;
            USHORT iHorzScroll = 0;
            USHORT iVertScroll = 0;
            PSWP swp = (PSWP)mp1;

            cControls = SHORT1FROMMR((*wd->pfnOldProc)(hwnd, WM_FORMATFRAME, mp1, mp2));

            iClient = cControls-1;

            while(swp[iHorzScroll].hwnd != WinWindowFromID(hwnd, FID_HORZSCROLL))
            {
               iHorzScroll++;
            }

            while(swp[iVertScroll].hwnd != WinWindowFromID(hwnd, FID_VERTSCROLL))
            {
               iVertScroll++;
            }

            iStatusBar = cControls;

            swp[iStatusBar].hwnd = WinWindowFromID(hwnd, FID_STATUSBAR);
            swp[iStatusBar].x = swp[iClient].x;
            swp[iStatusBar].cx = swp[iClient].cx + swp[iVertScroll].cx;
            swp[iStatusBar].y = swp[iHorzScroll].y;
            swp[iStatusBar].cy = wd->cyStatusBar;
            swp[iStatusBar].fl = SWP_SHOW | SWP_MOVE | SWP_SIZE;

            swp[iHorzScroll].y += wd->cyStatusBar;

            swp[iVertScroll].y += wd->cyStatusBar;
            swp[iVertScroll].cy -= wd->cyStatusBar;

            swp[iClient].y += wd->cyStatusBar;
            swp[iClient].cy -= wd->cyStatusBar;

            mReturn = MRFROMSHORT(cControls+1);
         }
         break;

      case WM_QUERYTRACKINFO:
         pti = (PTRACKINFO)mp2;
         mReturn = (*wd->pfnOldProc)(hwnd, WM_QUERYTRACKINFO, mp1, mp2);

         /*
          * Exclude move operations
          *
          * NOTE: Because of griding, window can not be set to maximum size(!)
          *       FIX IT!
          */
         if((pti->fs & TF_MOVE) ^ TF_MOVE)
         {
            pti->fs |= TF_GRID;

            if(pti->fs & (TF_LEFT | TF_RIGHT))
            {
               pti->cxGrid = wd->sizlCell.cx;
            }
            if(pti->fs & (TF_TOP | TF_BOTTOM))
            {
               pti->cyGrid = wd->sizlCell.cy;
            }
         }

         if(wd->ulMaxWidth)
         {
            pti->ptlMaxTrackSize.x = wd->ulMaxWidth;
         }
         if(wd->ulMaxHeight)
         {
            pti->ptlMaxTrackSize.y = wd->ulMaxHeight;
         }
         break;

      case WM_MINMAXFRAME:
         pswp = (PSWP)mp1;
         if(pswp->fl & SWP_MAXIMIZE)
         {
            if(pswp->cx > wd->ulMaxWidth)
            {
               pswp->cx = wd->ulMaxWidth;
               pswp->x = (WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN)/2)-(wd->ulMaxWidth/2);
            }

            if(pswp->cy > wd->ulMaxHeight)
            {
               pswp->cy = wd->ulMaxHeight;
               pswp->y = (WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN)/2)-(wd->ulMaxHeight/2);
            }
         }
         break;

      case WM_DESTROY:
         WinSubclassWindow(hwnd, wd->pfnOldProc);
         free(wd);
         break;

      case WMU_MAX_CLIENT_SIZE:
         rect.xLeft = rect.yBottom = 0;
         rect.xRight = LONGFROMMP(mp1);
         rect.yTop = LONGFROMMP(mp2);
         WinCalcFrameRect(hwnd, &rect, FALSE);
         wd->ulMaxWidth = rect.xRight-rect.xLeft;
         wd->ulMaxHeight = rect.yTop-rect.yBottom;
         break;

      case WM_TRACKFRAME:
         #ifdef DEBUG_TERM
         puts("i FrameWindow: WM_TRACKFRAME");
         #endif

         #ifdef DEBUG_TERM
         printf("i Tracking:");
         if(SHORT1FROMMP(mp1) & TF_MOVE)
         {
            if((SHORT1FROMMP(mp1) & TF_MOVE) ^ TF_MOVE)
            {
               if(SHORT1FROMMP(mp1) & TF_LEFT)
               {
                  printf(" Left");
               }
               if(SHORT1FROMMP(mp1) & TF_BOTTOM)
               {
                  printf(" Bottom");
               }
               if(SHORT1FROMMP(mp1) & TF_RIGHT)
               {
                  printf(" Right");
               }
               if(SHORT1FROMMP(mp1) & TF_TOP)
               {
                  printf(" Top");
               }
            }
            else
            {
               printf(" Move");
            }
            puts("");
         }
         #endif

         /*
          * Send tracking information to client
          */
         WinSendMsg(wd->hwndClient, WM_TRACKFRAME, mp1, mp2);
         fHandled = FALSE;
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