#pragma strings(readonly)
/*---------------------------------------------------------------------------*\
 *    Title: Kernel Debugger Terminal Window Procedure                       *
 * Filename: TerminalWindow.c                                                *
 *     Date: 2000-02-26                                                      *
 *   Author: Jan M. Danielsson                                               *
 *           Note: This source is heavily based on the PM Terminal written   *
 *           by Peter Fitzsimmons.                                           *
 *                                                                           *
 *  Notes: This source is - apart from being based on pmtermsr.zip - also    *
 *         ripped out from a kernel debugging terminal I'm working on.       *
 *         Expect odd comments in odd places.                                *
\*---------------------------------------------------------------------------*/
#define INCL_WINWINDOWMGR
#define INCL_WINSYS
#define INCL_WINSHELLDATA
#define INCL_WINFRAMEMGR
#define INCL_WINSCROLLBARS
#define INCL_WINTRACKRECT
#define INCL_WININPUT                    /* WM_CHAR */
#define INCL_WINENTRYFIELDS
#define INCL_WINDIALOGS
#define INCL_WINSTDSPIN
#define INCL_WINLISTBOXES
#define INCL_WINBUTTONS
#define INCL_WINMENUS
#define INCL_GPILCIDS
#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSERRORS

#include <os2.h>

#ifdef DEBUG
#include <stdio.h>
#endif

#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>

#include <res.h>

#include <TerminalWindow.h>


#ifdef USE_UNDOC
/* Undocumented event semaphore flags */
#define DCE_POSTONE                      0x0800   /* Post only to one waiting thread (in case there are multiple) */
#define DCE_AUTORESET                    0x1000   /* Automatically reset semaphore after a post.                  */
#endif




/*
 * Internal structures
 */
typedef struct _TERMLINE
{
   USHORT cbChars;                       /* Characters used in line */
   PBYTE buf;
   BOOL fEOL;                            /* Line has recieved a valid EOL */
   LONG lTextColor;
}TERMLINE, *PTERMLINE;

typedef struct _TERMINFO
{
   HWND hWnd;
   HFILE hPipe;                          /* Pipe handle */
   LONG rows;                            /* Terminal height */
   LONG columns;                         /* Terminal width  */
   POINTL cursor;
   BOOL fLogFile;
   HFILE hfLogFile;
   PTERMLINE lines;
}TERMINFO, *PTERMINFO;

typedef struct _SCROLLBARVAL
{
   LONG h;
   LONG v;
}SCROLLBARVAL, *PSCROLLBARVAL;

typedef struct _WINDOWDATA
{
   SIZEL sizlClient;                     /* Client size in pels                          */
   SIZEL sizlChar;                       /* Size of characters in pels                   */
   SIZEL sizlChars;                      /* Characters visible on screen (client/char)   */
   LONG yDesc;
   FONTMETRICS fm;
   SCROLLBARVAL scrollMax;
   SCROLLBARVAL scrollPos;
   TERMINFO ti;
   HWND hwndHorzScroll;
   HWND hwndVertScroll;
}WINDOWDATA, *PWINDOWDATA;




/*
 * Structure stored at QWL_USER in framewindow dataarea for mapwindow frame
 */
typedef struct _FRAMEWINDOWUSERDATA
{
   PFNWP pfnOldProc;
   ULONG ulMaxWidth;                     /* Maximum width of frame */
   ULONG ulMaxHeight;                    /* Maximum height of frame */
   HWND hwndClient;
}FRAMEWINDOWUSERDATA, *PFRAMEWINDOWUSERDATA;




#define QWP_WINDOWDATA                   0
#define QW_EXTRA                         QWP_WINDOWDATA+sizeof(PWINDOWDATA)


/* Client user messages */
#define WMU_CLEAR                        WM_USER+1
#define WMU_PAINTLINES                   WM_USER+2
#define WMU_PRESCROLL                    WM_USER+3


/* Frame user messages */
#define WMU_RECALCMAXSIZE                WM_USER+1

/*
 * Function prototypes - Local functions
 */
static MRESULT EXPENTRY WindowProcedure(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);
static MRESULT _Optlink processCreateMessage(HWND hWnd, MPARAM mp1, MPARAM mp2);
static void _Optlink processAdjustWindowPosMessage(HWND hWnd, MPARAM mp1, MPARAM mp2);
static void _Optlink processSizeMessage(HWND hWnd, MPARAM mp1, MPARAM mp2);
static void _Optlink processPaintMessage(HWND hWnd);
static void _Optlink processHScrollMessage(HWND hWnd, MPARAM mp1, MPARAM mp2);
static void _Optlink processVScrollMessage(HWND hWnd, MPARAM mp1, MPARAM mp2);
static void _Optlink processPresParamsChangedMessage(HWND hWnd, MPARAM mp1);
static void _Optlink processDestroyMessage(HWND hWnd);

static void _Optlink processClearMessage(HWND hWnd);

static void _Optlink processPaintLinesMessage(HWND hWnd, LONG startline, LONG endline);
static void _Optlink processPreScrollMessage(HWND hWnd, MPARAM mp1, MPARAM mp2);


static BOOL _Optlink init_terminfo(PTERMINFO ti);
static BOOL _Optlink term_terminfo(PTERMINFO ti);

static void _Optlink term_print(HWND hWnd, char *pchBuf, ULONG cbBuf);


static BOOL _Optlink vScrollParms(HWND hWnd, ULONG row, ULONG rows);
static BOOL _Optlink hScrollParms(HWND hWnd, ULONG col, ULONG cols);

static void Adjust(HWND hWnd, LONG line1, LONG line2);


/* Frame window subclassing functions */
static BOOL _Optlink subclassFrameProc(HWND hwndClient);
static MRESULT EXPENTRY FrameSubProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);
static void _Optlink processFrameAdjustWindowPosMessage(HWND hWnd, MPARAM mp1, MPARAM mp2);
static MRESULT _Optlink processFrameQueryTrackInfo(HWND hWnd, MPARAM mp1, MPARAM mp2);
static void _Optlink processFrameRecalcMaxSize(HWND hwndFrame);


static void _Optlink rcvthread(void *param);




static MRESULT EXPENTRY TerminalSettingsDlgProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);
static MRESULT EXPENTRY AboutDlgProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);




BOOL _Optlink registerTerminalWindow(HAB hAB)
{
   BOOL fSuccess = FALSE;

   fSuccess = WinRegisterClass(hAB, WC_TERMINALWINDOW, WindowProcedure, CS_SIZEREDRAW, QW_EXTRA);

   return fSuccess;
}





static MRESULT EXPENTRY WindowProcedure(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mReturn = NULL;
   BOOL fHandled = TRUE;

   switch(msg)
   {
      case WM_CREATE:
         mReturn = processCreateMessage(hWnd, mp1, mp2);
         break;

      case WM_ADJUSTWINDOWPOS:
         processAdjustWindowPosMessage(hWnd, mp1, mp2);
         break;

      case WM_SIZE:
         processSizeMessage(hWnd, mp1, mp2);
         break;

      case WM_PAINT:
         processPaintMessage(hWnd);
         break;

      case WM_COMMAND:
         switch(SHORT1FROMMP(mp1))
         {
            case IDM_TERMINAL_ABOUT:
               WinDlgBox(HWND_DESKTOP, hWnd, AboutDlgProc, (HMODULE)NULLHANDLE, DLG_ABOUT, NULL);
               break;

            case IDM_TERMINAL_CLEAR:
               WinPostMsg(hWnd, WMU_CLEAR, MPVOID, MPVOID);
               break;

            case IDM_TERMINAL_EXIT:
               WinPostMsg(hWnd, WM_CLOSE, MPVOID, MPVOID);
               break;

            case IDM_OPTIONS_SAVEWINPOS:
               {
                  SWP swp = { 0 };
                  WinQueryWindowPos(WinQueryWindow(hWnd, QW_PARENT), &swp);
                  PrfWriteProfileData(HINI_USERPROFILE, "PMDebugTerminal", "Window Position", &swp, sizeof(swp));
               }
               break;

            default:
               fHandled = FALSE;
               break;
         }
         break;

      case WM_HSCROLL:
         processHScrollMessage(hWnd, mp1, mp2);
         break;

      case WM_VSCROLL:
         processVScrollMessage(hWnd, mp1, mp2);
         break;

      case WM_PRESPARAMCHANGED:
         processPresParamsChangedMessage(hWnd, mp1);
         break;

      case WM_DESTROY:
         processDestroyMessage(hWnd);
         break;

      case WM_ERASEBACKGROUND:
         mReturn = (MRESULT)TRUE;
         break;

      case WMU_CLEAR:
         processClearMessage(hWnd);
         break;

      case WMU_PAINTLINES:
         processPaintLinesMessage(hWnd, LONGFROMMP(mp1), LONGFROMMP(mp2));
         break;

      case WMU_PRESCROLL:
         processPreScrollMessage(hWnd, mp1, mp2);
         break;

      default:
         fHandled = FALSE;
         break;
   }
   if(!fHandled)
   {
      mReturn = WinDefWindowProc(hWnd, msg, mp1, mp2);
   }
   return mReturn;
}


static MRESULT _Optlink processCreateMessage(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   APIRET rc = NO_ERROR;
   BOOL fAbort = TRUE;
   PWINDOWDATA pWindowData = NULL;
   PTERMCDATA ctldata = (PTERMCDATA)mp1;
   PCREATESTRUCT pCREATE = (PCREATESTRUCT)mp2;

   pWindowData = (PWINDOWDATA)malloc(sizeof(WINDOWDATA));
   if(pWindowData)
   {
      char font[100] = "10.System VIO";
      HPS hPS = NULLHANDLE;
      ULONG ulAction = 0;
      ULONG ulLength = 0;

      memset(pWindowData, 0, sizeof(WINDOWDATA));

      WinSetWindowPtr(hWnd, QWP_WINDOWDATA, pWindowData);

      pWindowData->ti.hPipe = ctldata->hDebugPipe;

      pWindowData->ti.fLogFile = ctldata->fLogFile;
      pWindowData->ti.hfLogFile = ctldata->hfLogFile;


      /*
       * Setup window data
       */
      pWindowData->ti.rows = ctldata->rows;
      pWindowData->ti.columns = ctldata->columns;
      pWindowData->ti.hWnd = hWnd;

      ulLength = PrfQueryProfileString(HINI_USERPROFILE, "PMDebugTerminal", "Font", "10.System VIO", font, sizeof(font));

      WinSetPresParam(hWnd, PP_FONTNAMESIZE, ulLength, font);

      hPS = WinGetPS(hWnd);
      GpiQueryFontMetrics(hPS, sizeof(pWindowData->fm), &pWindowData->fm);
      WinReleasePS(hPS);

      pWindowData->sizlChar.cx = pWindowData->fm.lAveCharWidth;
      pWindowData->sizlChar.cy = pWindowData->fm.lMaxBaselineExt;
      pWindowData->yDesc = pWindowData->fm.lMaxDescender;

      vScrollParms(hWnd, 0, pWindowData->ti.rows);
      hScrollParms(hWnd, 0, pWindowData->ti.columns);

      subclassFrameProc(hWnd);

      init_terminfo(&pWindowData->ti);

      _beginthread(rcvthread, NULL, 16384, &pWindowData->ti);

      fAbort = FALSE;
   }

   return (MRESULT)fAbort;
}


static void _Optlink processAdjustWindowPosMessage(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   PSWP swp = (PSWP)mp1;

   if(swp->fl & SWP_SIZE)
   {
      swp->cx += (swp->cx % pWindowData->sizlChar.cx);
      swp->cy += (swp->cy % pWindowData->sizlChar.cy);
   }
}


static void _Optlink processSizeMessage(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   SHORT scxold = SHORT1FROMMP(mp1);
   SHORT scyold = SHORT2FROMMP(mp1);
   SHORT scxnew = SHORT1FROMMP(mp2);
   SHORT scynew = SHORT2FROMMP(mp2);

   if(scxold != scxnew)
   {
      pWindowData->sizlClient.cx = scxnew;
      pWindowData->sizlChars.cx = pWindowData->sizlClient.cx / pWindowData->sizlChar.cx;
      hScrollParms(hWnd, pWindowData->scrollPos.h, pWindowData->ti.columns);
   }

   if(scyold != scynew)
   {
      pWindowData->sizlClient.cy = scynew;
      pWindowData->sizlChars.cy = pWindowData->sizlClient.cy / pWindowData->sizlChar.cy;
      vScrollParms(hWnd, pWindowData->scrollPos.v, pWindowData->ti.rows);
   }
}

static void _Optlink processPaintMessage(HWND hWnd)
{
   RECTL rect = { 0L, 0L, 0L, 0L };
   HPS hPS = WinBeginPaint(hWnd, NULLHANDLE, &rect);

   if(hPS)
   {
      POINTL pt = { 0, 0 };
      LONG paintFirst = 0, paintLast = 0;
      LONG ofs;
      PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
      ULONG i = 0;

      GpiErase(hPS);

      paintFirst = max(0, pWindowData->scrollPos.v + (pWindowData->sizlClient.cy - (SHORT)rect.yTop) / pWindowData->sizlChar.cy);
      paintLast = min(pWindowData->ti.rows, pWindowData->scrollPos.v + (pWindowData->sizlClient.cy - (SHORT)rect.yBottom) / pWindowData->sizlChar.cy + 1);

      ofs = pWindowData->scrollPos.h;

      for(i = paintFirst; i < paintLast; i++)
      {
         char *str = pWindowData->ti.lines[i].buf;
         LONG len = pWindowData->ti.lines[i].cbChars;

         pt.y = pWindowData->sizlClient.cy - pWindowData->sizlChar.cy * (i+1 - pWindowData->scrollPos.v) + pWindowData->yDesc;
         if(len > ofs)
         {
            LONG cols = len - ofs;
            if(cols)
            {
               GpiSetColor(hPS, pWindowData->ti.lines[i].lTextColor);
               GpiCharStringAt(hPS, &pt, (ULONG)cols, str + ofs);
            }
         }
      }

      WinEndPaint(hPS);
   }
}


static void _Optlink processHScrollMessage(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   USHORT cmd = SHORT2FROMMP(mp2);
   BOOL fSetpos = TRUE;
   LONG lScroll = 0;

   switch(cmd)
   {
      case SB_LINELEFT:
         lScroll = -1;
         break;

      case SB_LINERIGHT:
         lScroll = 1;
         break;

      case SB_PAGELEFT:
         lScroll = -max(8, pWindowData->scrollMax.h / 10);
         break;

      case SB_PAGERIGHT:
         lScroll = max(8, pWindowData->scrollMax.h / 10);
         break;

      case SB_SLIDERTRACK:
         lScroll = SHORT1FROMMP(mp2) - pWindowData->scrollPos.h;
         fSetpos = FALSE;
         break;

      case SB_SLIDERPOSITION:
         lScroll = SHORT1FROMMP(mp2) - pWindowData->scrollPos.h;
         break;

      case SB_ENDSCROLL:
         lScroll = 0;
         break;

      default:
         lScroll = 0;
         fSetpos = FALSE;
         break;
   }
   if(0 != (lScroll = max(-pWindowData->scrollPos.h, min(lScroll, pWindowData->scrollMax.h - pWindowData->scrollPos.h))))
   {
      pWindowData->scrollPos.h += lScroll;
      WinScrollWindow(hWnd, -pWindowData->sizlChar.cx * lScroll, 0, NULL, NULL, NULLHANDLE, NULL, SW_INVALIDATERGN);
   }

   WinUpdateWindow(hWnd);

   if(fSetpos)
   {
      HWND hwndBar = WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), SHORT1FROMMP(mp2));

      WinSendMsg(hwndBar, SBM_SETPOS, MPFROMSHORT(pWindowData->scrollPos.h), 0L);
   }
}


static void _Optlink processVScrollMessage(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   USHORT cmd = SHORT2FROMMP(mp2);
   BOOL fSetpos = TRUE;
   LONG lScroll = 0;

   switch(cmd)
   {
      case SB_LINEUP:
         lScroll = -1;
         break;

      case SB_LINEDOWN:
         lScroll = 1;
         break;

      case SB_PAGEUP:
         lScroll = -pWindowData->sizlChars.cy;
         break;

      case SB_PAGEDOWN:
         lScroll = pWindowData->sizlChars.cy;
         break;

      case SB_SLIDERTRACK:
         lScroll = SHORT1FROMMP(mp2) - pWindowData->scrollPos.v;
         fSetpos = FALSE;
         break;

      case SB_ENDSCROLL:
         lScroll = 0;
         break;

      case SB_SLIDERPOSITION:
         lScroll = SHORT1FROMMP(mp2) - pWindowData->scrollPos.v;
         break;

      default:
         lScroll = 0;
         fSetpos = FALSE;
         break;
   }
   if(0 != (lScroll = max(-pWindowData->scrollPos.v, min(lScroll, pWindowData->scrollMax.v - pWindowData->scrollPos.v))))
   {
      pWindowData->scrollPos.v += lScroll;
      WinScrollWindow(hWnd, 0, pWindowData->sizlChar.cy * lScroll, NULL, NULL, NULLHANDLE, NULL, SW_INVALIDATERGN);
      WinUpdateWindow(hWnd);
   }
   if(fSetpos)
   {
      HWND hwndBar = WinWindowFromID(WinQueryWindow(hWnd, QW_PARENT), FID_VERTSCROLL);

      WinSendMsg(hwndBar, SBM_SETPOS, MPFROMSHORT(pWindowData->scrollPos.v), MPVOID);
   }
}



static void _Optlink processPresParamsChangedMessage(HWND hWnd, MPARAM mp1)
{
   ULONG idAttrType = LONGFROMMP(mp1);

   if(idAttrType == PP_FONTNAMESIZE)
   {
      PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
      char tmp[100];

      HPS hPS = WinGetPS(hWnd);

      GpiQueryFontMetrics(hPS, sizeof(pWindowData->fm), &pWindowData->fm);

      if(WinQueryPresParam(hWnd, PP_FONTNAMESIZE, 0UL, NULL, sizeof(tmp), tmp, QPF_NOINHERIT))
      {
         PrfWriteProfileString(HINI_USERPROFILE, "PMDebugTerminal", "Font", tmp);
      }


      WinReleasePS(hPS);

      pWindowData->sizlChar.cx = pWindowData->fm.lAveCharWidth;
      pWindowData->sizlChar.cy = pWindowData->fm.lMaxBaselineExt;
      pWindowData->yDesc = pWindowData->fm.lMaxDescender;

      pWindowData->sizlChars.cx = pWindowData->sizlClient.cx / pWindowData->sizlChar.cx;
      pWindowData->sizlChars.cy = pWindowData->sizlClient.cy / pWindowData->sizlChar.cy;

      vScrollParms(hWnd, pWindowData->scrollPos.v, pWindowData->ti.rows);
      hScrollParms(hWnd, pWindowData->scrollPos.h, pWindowData->ti.columns);

      WinPostMsg(WinQueryWindow(hWnd, QW_PARENT), WMU_RECALCMAXSIZE, MPVOID, MPVOID);

      WinInvalidateRect(WinQueryWindow(hWnd, QW_PARENT), NULL, TRUE);
   }
}



static void _Optlink processDestroyMessage(HWND hWnd)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);

   if(pWindowData)
   {
      free(pWindowData);
   }
}


static void _Optlink processClearMessage(HWND hWnd)
{
   ULONG i = 0;
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);

   for(i = 0; i < pWindowData->ti.rows; i++)
   {
      pWindowData->ti.lines[i].cbChars = 0;
      pWindowData->ti.lines[i].fEOL = 0;
      pWindowData->ti.lines[i].buf[0] = '\0';
   }
   pWindowData->ti.cursor.x = pWindowData->ti.cursor.y = 0;

   WinInvalidateRect(hWnd, NULL, FALSE);
}




static void _Optlink processPaintLinesMessage(HWND hWnd, LONG startline, LONG endline)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   POINTL pt = { 0, 0 };
   LONG ofs = 0;
   HPS hPS = WinGetPS(hWnd);

   if(startline < 0)
   {
      startline = 0;
   }
   Adjust(hWnd, startline, endline);

   if(hPS)
   {
      POINTL pt = { 0, 0 };
      PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
      ULONG i = 0;

      ofs = pWindowData->scrollPos.h;

      for(i = startline; i <= endline; i++)
      {
         char *str = pWindowData->ti.lines[i].buf;
         LONG len = pWindowData->ti.lines[i].cbChars;

         pt.y = pWindowData->sizlClient.cy - pWindowData->sizlChar.cy * (i+1 - pWindowData->scrollPos.v) + pWindowData->yDesc;
         if(len > ofs)
         {
            LONG cols = len - ofs;
            if(cols)
            {
               GpiSetColor(hPS, pWindowData->ti.lines[i].lTextColor);
               GpiCharStringAt(hPS, &pt, (ULONG)cols, str + ofs);
            }
         }
      }
      WinReleasePS(hPS);
   }
}

static void _Optlink processPreScrollMessage(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);

   WinScrollWindow(hWnd, 0, pWindowData->sizlChar.cy * LONGFROMMP(mp1), NULL, NULL, NULLHANDLE, NULL, SW_INVALIDATERGN);
}




static BOOL _Optlink init_terminfo(PTERMINFO ti)
{
   ULONG i = 0;
   PTERMLINE termlines = NULL;
   BOOL fSuccess = FALSE;

   termlines = (PTERMLINE)calloc(ti->rows, sizeof(TERMLINE));
   if(termlines)
   {
      ti->lines = termlines;

      for(i = 0; i < ti->rows; i++)
      {

         ti->lines[i].cbChars = 0UL;
         ti->lines[i].fEOL = FALSE;
         ti->lines[i].lTextColor = CLR_BLACK;
         ti->lines[i].buf = malloc(ti->columns);

         if(ti->lines[i].buf == NULL)
         {
            break;
         }
      }

      if(i < ti->rows)
      {
         term_terminfo(ti);
      }
      else
      {
         fSuccess = TRUE;
      }
   }

   return fSuccess;
}

static BOOL _Optlink term_terminfo(PTERMINFO ti)
{
   ULONG i = 0;

   for(i = 0; i < ti->rows; i++)
   {
      if(ti->lines[i].buf)
      {
         free(ti->lines[i].buf);
      }
   }
   free(ti->lines);

   return TRUE;
}

void shift1(PTERMINFO ti)
{
   TERMLINE tl;
   memcpy(&tl, ti->lines, sizeof(TERMLINE));
   memmove(ti->lines, &ti->lines[1], sizeof(TERMLINE)*(ti->rows-1));
   memcpy(&ti->lines[ti->rows-1], &tl, sizeof(tl));
   ti->lines[ti->rows-1].cbChars = 0;
   ti->lines[ti->rows-1].fEOL = FALSE;
}

static void _Optlink term_print(HWND hWnd, char *pchBuf, ULONG cbBuf)
{
   ULONG i = 0;
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   PTERMINFO ti = &pWindowData->ti;
   LONG pre_scroll = 0;
   LONG startline = ti->cursor.y;

   for(i = 0; i < cbBuf; i++)
   {
      switch(pchBuf[i])
      {
         case '\r':
            ti->cursor.x = 0;
            break;

         case '\f':
            while(!WinPostMsg(hWnd, WMU_CLEAR, MPVOID, MPVOID))
            {
               DosSleep(0);
            }
            break;

         case '\b':
            #ifdef DEBUG
            puts("Backspace!");
            #endif
            if(ti->lines[ti->cursor.y].cbChars)
            {
               ti->lines[ti->cursor.y].cbChars--;
               ti->lines[ti->cursor.y].buf[--ti->cursor.x] = ' ';
            }
            break;

         case '\n':
            ti->lines[ti->cursor.y].fEOL = TRUE;
            ti->cursor.y++;
            if(ti->cursor.y == ti->rows)
            {
               shift1(ti);
               ti->cursor.y--;
               pre_scroll++;
            }
            ti->lines[ti->cursor.y].fEOL = FALSE;
            ti->lines[ti->cursor.y].cbChars = 0;
            ti->lines[ti->cursor.y].lTextColor = CLR_BLACK;
            break;

         case ' ':
            if(ti->cursor.x == 1)
            {
               switch(ti->lines[ti->cursor.y].buf[0])
               {
                  case 'i':              /* 'Infromational' */
                     ti->lines[ti->cursor.y].lTextColor = CLR_BLACK;
                     break;

                  case 'w':              /* 'Warning' */
                     ti->lines[ti->cursor.y].lTextColor = CLR_BLUE;
                     break;

                  case 'e':              /* 'Error' */
                     ti->lines[ti->cursor.y].lTextColor = CLR_RED;
                     break;

                  default:               /* Anything else */
                     ti->lines[ti->cursor.y].lTextColor = CLR_BLACK;
                     break;
               }
            }
         default:
            ti->lines[ti->cursor.y].fEOL = FALSE;
            ti->lines[ti->cursor.y].buf[ti->cursor.x++] = pchBuf[i];
            if(ti->cursor.x >= ti->lines[ti->cursor.y].cbChars)
            {
               ti->lines[ti->cursor.y].cbChars++;
               if(ti->lines[ti->cursor.y].cbChars < ti->cursor.x)
               {
                  memset(ti->lines[ti->cursor.y].buf + ti->cursor.x, ' ', ti->cursor.x-ti->lines[ti->cursor.y].cbChars);
                  ti->lines[ti->cursor.y].cbChars = ti->cursor.x;
               }
            }
            if(ti->cursor.x == ti->columns)
            {
               ti->cursor.x = 0;
               ti->cursor.y++;
               if(ti->cursor.y == ti->rows)
               {
                  shift1(ti);
                  ti->cursor.y--;
                  pre_scroll++;
               }
               ti->lines[ti->cursor.y].fEOL = FALSE;
               ti->lines[ti->cursor.y].cbChars = 0;
            }
            break;
      }
   }
   if(pre_scroll)
   {
      while(!WinPostMsg(hWnd, WMU_PRESCROLL, (MPARAM)pre_scroll, MPVOID))
      {
         DosSleep(0);
      }
   }
   while(!WinPostMsg(hWnd, WMU_PAINTLINES, (MPARAM)(startline-pre_scroll), (MPARAM)ti->cursor.y))
   {
      DosSleep(0);
   }
}


static BOOL _Optlink vScrollParms(HWND hWnd, ULONG row, ULONG rows)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   HWND hwndFrame = WinQueryWindow(hWnd, QW_PARENT);
   HWND hwndBar = WinWindowFromID(hwndFrame, FID_VERTSCROLL);

   if(row >= rows)
   {
      return FALSE;
   }

   pWindowData->scrollMax.v = max(0, pWindowData->ti.rows - pWindowData->sizlChars.cy);
   pWindowData->scrollPos.v = min(row, pWindowData->scrollMax.v);

   WinSendMsg(hwndBar, SBM_SETSCROLLBAR, MPFROMSHORT(pWindowData->scrollPos.v), MPFROM2SHORT(0, pWindowData->scrollMax.v));
   WinSendMsg(hwndBar, SBM_SETTHUMBSIZE, MPFROM2SHORT(pWindowData->sizlChars.cy, pWindowData->ti.rows), 0);

   WinEnableWindow(hwndBar, pWindowData->scrollMax.v ? TRUE : FALSE);

   return TRUE;
}

static BOOL _Optlink hScrollParms(HWND hWnd, ULONG col, ULONG cols)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   HWND hwndFrame = WinQueryWindow(hWnd, QW_PARENT);
   HWND hwndBar = WinWindowFromID(hwndFrame, FID_HORZSCROLL);

   if(col >= cols)
   {
      return FALSE;
   }

   pWindowData->scrollMax.h = max(0, pWindowData->ti.columns - pWindowData->sizlChars.cx);
   pWindowData->scrollPos.h = min(col, pWindowData->scrollMax.h);

   WinSendMsg(hwndBar, SBM_SETSCROLLBAR, MPFROMSHORT(pWindowData->scrollPos.h), MPFROM2SHORT(0, pWindowData->scrollMax.h));
   WinSendMsg(hwndBar, SBM_SETTHUMBSIZE, MPFROM2SHORT(pWindowData->sizlChars.cx, pWindowData->ti.columns), 0);

   WinEnableWindow(hwndBar, pWindowData->scrollMax.h ? TRUE : FALSE);
}



static void Adjust(HWND hWnd, LONG line1, LONG line2)
{
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hWnd, QWP_WINDOWDATA);
   LONG y = 0;

   y = pWindowData->sizlClient.cy - pWindowData->sizlChar.cy * (line2 + 1 - pWindowData->scrollPos.v) + pWindowData->yDesc;
   if(y < 0)
   {
      LONG n = y/pWindowData->sizlChar.cy - 1;
      WinSendMsg(hWnd, WM_VSCROLL, (MPARAM)FID_VERTSCROLL, MPFROM2SHORT(pWindowData->scrollPos.v - n, SB_SLIDERPOSITION));
   }
   else
   {
      y = pWindowData->sizlClient.cy - pWindowData->sizlChar.cy * (line1 + 1 - pWindowData->scrollPos.v) + pWindowData->yDesc;
      if(y > pWindowData->sizlClient.cy)
      {
         LONG n = 1 + (y - pWindowData->sizlClient.cy) / pWindowData->sizlChar.cy;
         WinSendMsg(hWnd, WM_VSCROLL, (MPARAM)FID_VERTSCROLL, MPFROM2SHORT(pWindowData->scrollPos.v - n, SB_SLIDERPOSITION));
      }
   }
}









static BOOL _Optlink subclassFrameProc(HWND hwndClient)
{
   HWND hwndFrame = WinQueryWindow(hwndClient, QW_PARENT);
   PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(hwndClient, QWP_WINDOWDATA);
   PFRAMEWINDOWUSERDATA fwud = NULL;
   RECTL rect = { 0, 0, 0, 0 };
   BOOL fSuccess = FALSE;

   /*
    * Maximum size of client
    */
   rect.xLeft = 0;
   rect.xRight = pWindowData->ti.columns*pWindowData->sizlChar.cx;
   rect.yBottom = 0;
   rect.yTop = pWindowData->ti.rows*pWindowData->sizlChar.cy;

   /*
    * Calculate the size of the frame when the client is at its maximum size
    */
   WinCalcFrameRect(hwndFrame, &rect, FALSE);

   fwud = (PFRAMEWINDOWUSERDATA)malloc(sizeof(FRAMEWINDOWUSERDATA));
   if(fwud)
   {
      WinSetWindowULong(hwndFrame, QWL_USER, (ULONG)fwud);
      fwud->ulMaxWidth = rect.xRight-rect.xLeft;
      fwud->ulMaxHeight = rect.yTop-rect.yBottom;
      fwud->hwndClient = hwndClient;
      fwud->pfnOldProc = WinSubclassWindow(hwndFrame, FrameSubProc);
      if(fwud->pfnOldProc)
      {
         fSuccess = TRUE;
      }
      else
      {
         free(fwud);
         WinSetWindowULong(hwndFrame, QWL_USER, (ULONG)NULL);
      }
   }
   else
   {
      /* Failed to allocate buffer for windowdata */
      fSuccess = FALSE;
   }

   return fSuccess;
}

static MRESULT EXPENTRY FrameSubProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mReturn = 0;
   BOOL fHandled = TRUE;
   PFRAMEWINDOWUSERDATA pParams = (PFRAMEWINDOWUSERDATA)(ULONG)WinQueryWindowULong(hWnd, QWL_USER);

   switch(msg)
   {
/*
      case WM_ADJUSTWINDOWPOS:
         processFrameAdjustWindowPosMessage(hWnd, mp1, mp2);
         break;
*/
      case WM_QUERYTRACKINFO:
         mReturn = processFrameQueryTrackInfo(hWnd, mp1, mp2);
         break;

      case WM_DESTROY:
         free(pParams);
         break;

      case WMU_RECALCMAXSIZE:
         processFrameRecalcMaxSize(hWnd);
         break;

      default:
         fHandled = FALSE;
         break;
   }
   if(!fHandled)
      mReturn = (*pParams->pfnOldProc)(hWnd, msg, mp1, mp2);

   return mReturn;
}

static void _Optlink processFrameAdjustWindowPosMessage(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   PFRAMEWINDOWUSERDATA pFrameData = (PFRAMEWINDOWUSERDATA)(ULONG)WinQueryWindowULong(hWnd, QWL_USER);
   PSWP swp = (PSWP)mp1;

/*
   if(swp->cx > pFrameData->ulMaxWidth)
   {
      swp->cx = pFrameData->ulMaxWidth;
   }

   if(swp->cy > pFrameData->ulMaxHeight)
   {
      swp->cy = pFrameData->ulMaxHeight;
   }
*/
}

static MRESULT _Optlink processFrameQueryTrackInfo(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   MRESULT mReturn = 0;
   PTRACKINFO pti = (PTRACKINFO)mp2;
   PFRAMEWINDOWUSERDATA pParams = NULL;

   pParams = (PFRAMEWINDOWUSERDATA)(ULONG)WinQueryWindowULong(hWnd, QWL_USER);

   /*
    * Let PM initialize the structure
    */
   mReturn = (*pParams->pfnOldProc)(hWnd, WM_QUERYTRACKINFO, mp1, mp2);

   /*
    * Store the maximum size in TRACKINFO structure
    */
   pti->ptlMaxTrackSize.x = pParams->ulMaxWidth;
   pti->ptlMaxTrackSize.y = pParams->ulMaxHeight;

   return mReturn;
}


static void _Optlink processFrameRecalcMaxSize(HWND hwndFrame)
{
   PFRAMEWINDOWUSERDATA pFrameData = (PFRAMEWINDOWUSERDATA)WinQueryWindowULong(hwndFrame, QWL_USER);
   PWINDOWDATA pClientData = (PWINDOWDATA)WinQueryWindowPtr(pFrameData->hwndClient, QWP_WINDOWDATA);
   RECTL rect = { 0, 0, 0, 0 };
   BOOL fSuccess = FALSE;

   /*
    * Maximum size of client
    */
   rect.xLeft = 0;
   rect.xRight = pClientData->ti.columns*pClientData->sizlChar.cx;
   rect.yBottom = 0;
   rect.yTop = pClientData->ti.rows*pClientData->sizlChar.cy;

   /*
    * Calculate the size of the frame when the client is at its maximum size
    */
   fSuccess = WinCalcFrameRect(hwndFrame, &rect, FALSE);

   /*
    * Store the maximum size
    */
   if(fSuccess)
   {
      SWP swp = { 0 };

      pFrameData->ulMaxWidth = rect.xRight-rect.xLeft;
      pFrameData->ulMaxHeight = rect.yTop-rect.yBottom;

      WinQueryWindowPos(hwndFrame, &swp);

      WinSetWindowPos(hwndFrame, swp.hwndInsertBehind, swp.x, swp.y, swp.cx, swp.cy, SWP_SIZE);
   }
}






static void _Optlink rcvthread(void *param)
{
   PTERMINFO ti = (PTERMINFO)param;
   char buf[512] = "";
   ULONG cbRead = 0;
   APIRET rc = NO_ERROR;

   #ifdef DEBUG
   puts("rcvthread running..");
   #endif

   while(1)
   {
      rc = DosRead(ti->hPipe, buf, sizeof(buf), &cbRead);
      if(rc)
      {
         break;
      }
      if(cbRead)
      {
         term_print(ti->hWnd, buf, cbRead);
         if(ti->fLogFile)
         {
            ULONG cbWritten = 0;
            DosWrite(ti->hfLogFile, buf, cbRead, &cbWritten);
         }
      }
      else
      {
         PWINDOWDATA pWindowData = (PWINDOWDATA)WinQueryWindowPtr(ti->hWnd, QWP_WINDOWDATA);

         /* Can't remember what I was planning to do here. Probably something paramount
            that would end all suffering throughout the world. Too bad I can't remember what. */

         DosSleep(0);
      }
   }

   #ifdef DEBUG
   puts("rcvthread terminated!");
   #endif

   _endthread();
}



static MRESULT EXPENTRY AboutDlgProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mReturn = 0;
   BOOL fHandled = TRUE;


   switch(msg)
   {
      case WM_COMMAND:
         switch(SHORT1FROMMP(mp1))
         {
            case DID_OK:
               WinDismissDlg(hWnd, SHORT1FROMMP(mp1));
               break;

            default:
               fHandled = FALSE;
               break;
         }
         break;

      default:
         fHandled = FALSE;
   }
   if(!fHandled)
   {
      mReturn = WinDefDlgProc(hWnd, msg, mp1, mp2);
   }
   return mReturn;
}
