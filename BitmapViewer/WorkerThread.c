#pragma strings(readonly)

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINSCROLLBARS
#define INCL_WINMENUS
#define INCL_WINSYS
#define INCL_WINTRACKRECT
#define INCL_WININPUT
#define INCL_WINERRORS
#define INCL_GPIBITMAPS
#define INCL_GPIREGIONS
#define INCL_DOSPROCESS

#include <os2.h>

#include <process.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>

#ifdef DEBUG_TERM
#include <stdio.h>
#endif

#include "WorkerThread.h"
#include "CanvasWindow.h"
#include "FrameSubProc.h"

#include "PixelBuffer.h"

#include "resources.h"


/*
 * Internal workerthread messages.
 */
#define WTMSG_UPDATE                     WM_USER+0x2000



enum BitmapAlignment
{
   Uninitialized = 0,
   Lowest,
   Center,
   Highest,
};

typedef struct _CANVASWNDDATA
{
   HWND hwnd;                            /* Canvas window handle (the applicatino client)                */
   HWND hwndFrame;                       /* Canvas window frame handle (the application frame)           */
   HWND hwndHorzScroll;                  /* Scrollbar handle (See #2 in notes.text)                      */
   HWND hwndVertScroll;                  /* Scrollbar handle (See #2 in notes.text)                      */
   LONG dxScroll;                        /* How much horizontal scrolling is needed                      */
   LONG dyScroll;                        /* How much vertical scrolling is needed                        */
   RECTL rclCanvas;                      /* Area of window containing the bitmap (window coordinates)    */
   RECTL rclPixelBuffer;                 /* Area of bitmap visible (bitmap coordinates)                  */
   HRGN hrgnPaint;                       /* Invalidated region of canvas window                          */
   enum BitmapAlignment horzAlign;       /* How to align bitmap if canvas area is wider than bitmap      */
   enum BitmapAlignment vertAlign;       /* How to align vertically if canvas area is higher than bitmap */
   SIZEL sizlWindow;                     /* Size of canvas window                                        */
   ULONG flFrame;                        /* Use to keep track of which frame controls have been added/deleted.
                                            Variable sent to frame through the WM_UPDATEFRAME message    */
   USHORT fsTrackFlags;                  /* TF_* flags of the TRACKINFO structure                        */
}CANVASWNDDATA, *PCANVASWNDDATA;



static void _Optlink setDefaultWindowPosition(HWND hwndAppFrame);

LONG _Inline AddRegionRect(HPS hps, HRGN hrgnTemp, HRGN hrgn, LONG lcount, PRECTL arclRectangles);



void _Inline horizontal_adjustments(PCANVASWNDDATA wnd, PPIXELBUFFER pb, SHORT scxold, SHORT scxnew);
void _Inline vertical_adjustments(PCANVASWNDDATA wnd, PPIXELBUFFER pb, SHORT scyold, SHORT scynew);

void _Inline init_scrollbars(PCANVASWNDDATA wnd, PPIXELBUFFER pb);

void _Inline setup_gray_region(HRGN hrgnGray, PCANVASWNDDATA wnd);




void _Inline EnableMenuItems(HWND hwndMenu, ULONG idFirst, ULONG idLast, BOOL fEnable);


void _Optlink WorkerThread(void *param)
{
   PWRKRPARAMS threadParams = (PWRKRPARAMS)param;
   HAB hab = NULLHANDLE;
   HMQ hmq = NULLHANDLE;
   CANVASWNDDATA canvasWnd = { 0 };

   /*
    * Run the worker thread on idle priority
    */
   DosSetPriority(PRTYS_THREAD, PRTYC_IDLETIME, PRTYD_MAXIMUM, 0UL);

   /* Clear the canvas window data buffer */
   memset(&canvasWnd, 0, sizeof(canvasWnd));

   /*
    * Store the canvas and canvas frame window handles
    */
   canvasWnd.hwnd = threadParams->hwndCanvas;
   canvasWnd.hwndFrame = threadParams->hwndAppFrame;

   /*
    * Initialize PM for thread
    */
   hab = WinInitialize(0UL);

   if(hab)
   {
      /*
       * Create the message queue -- without this, the worker thread becomes
       * useless.
       */
      hmq = WinCreateMsgQueue(hab, 0L);
   }

   if(hmq)
   {
      QMSG qmsg = { 0 };                 /* Queue message structure           */
      QMSG q2 = { 0 };                   /* Used to remove redundant messages */

      HRGN hrgnGray = NULLHANDLE;        /* Gray region surrounding canvas         */
      HRGN hrgnTemp = NULLHANDLE;        /* Temporary region; miscellaneous usage  */

      PPIXELBUFFER pb = NULL;            /* The bitmap */
      PPIXELBUFFER pbLoad = NULL;        /* Temporary bitmap used while loading */

      BOOL fNewPixelBuffer = FALSE;      /* Flag indicating if a LoadPixelBuffer was successful or not */

      HWND hwndAppMenu = WinWindowFromID(canvasWnd.hwndFrame, FID_MENU);

      ULONG idMenuItem = 0;              /* Temporary variable used while using menus */

      RECTL rect = { 0, 0, 0, 0 };       /* General purpose rectangle structure */
      SWP swp = { 0, 0 };                /* General purpose swp structure */

      HDC hdcMem = NULLHANDLE;
      HPS hpsMem = NULLHANDLE;
      DEVOPENSTRUC dop = { 0L, "DISPLAY", NULL, 0L, 0L, 0L, 0L, 0L, 0L };

      #ifdef DEBUG_TERM
      printf("i WorkerThread hmq:%08x\n", hmq);
      #endif

      /*
       * Get the scrollbar window handles from frame.
       * This can not be done the conventional way since scrollbars are owned by
       * HWND_OBJECT at application startup. See #2 in notes.text
       */
      WinSendMsg(canvasWnd.hwndFrame, WMU_QUERY_SCROLLBAR_HANDLES, (MPARAM)&canvasWnd.hwndHorzScroll, (MPARAM)&canvasWnd.hwndVertScroll);

      /*
       * Create a presentation space to be used for the GPI region calls.
       */
      hdcMem = DevOpenDC(hab, OD_MEMORY, "*", 5L, (PDEVOPENDATA)&dop, NULLHANDLE);
      if(hdcMem != DEV_ERROR)
      {
         SIZEL size = { 0, 0 };
         hpsMem = GpiCreatePS(hab, hdcMem, &size, PU_PELS | GPIT_MICRO | GPIA_ASSOC);
      }

      if(hpsMem)
      {
         /*
          * Used to keep track of invalidated region
          */
         canvasWnd.hrgnPaint = GpiCreateRegion(hpsMem, 0, NULL);

         /*
          * Area outside of the canvas is grayed; store it in hrgnGray.
          */
         WinQueryWindowRect(canvasWnd.hwnd, &rect);
         hrgnGray = GpiCreateRegion(hpsMem, 1, &rect);

         /*
          * Temporary region; miscellaneous usage.
          */
         hrgnTemp = GpiCreateRegion(hpsMem, 0, NULL);
      }

      /*
       * Set initial window size.
       */
      WinQueryWindowRect(canvasWnd.hwnd, &rect);
      canvasWnd.sizlWindow.cx = rect.xRight-rect.xLeft;
      canvasWnd.sizlWindow.cy = rect.yTop-rect.yBottom;


      /*
       * Notify canvas window about successful initialization
       */
      WinSendMsg(canvasWnd.hwnd, WMU_CANVAS_NOTIFICATION, MPFROMLONG(WORKERTHREAD_HMQ), MPFROMLONG(hmq));

      /*
       * Set default alignments
       */
      WinPostQueueMsg(hmq, WTMSG_SET_HORZ_ALIGNMENT, MPFROMLONG(Center), MPVOID);
      WinPostQueueMsg(hmq, WTMSG_SET_VERT_ALIGNMENT, MPFROMLONG(Center), MPVOID);

      /*
       * If a file was specified on the command line, make sure file is loaded
       * This should be done last in the worker thread initialization since some
       * options which are set earlier are used after load completes.
       */
      if(threadParams->argc > 1)
      {
         LONG len = strlen(threadParams->argv[1])+1;
         char *tmp = malloc(len);
         memcpy(tmp, threadParams->argv[1], len);
         WinPostQueueMsg(hmq, WTMSG_LOAD_BITMAP, (MPARAM)tmp, MPVOID);
      }

      WinEnableMenuItem(hwndAppMenu, IDM_FILE, TRUE);
      WinEnableMenuItem(hwndAppMenu, IDM_FILE_LOAD, TRUE);

      setDefaultWindowPosition(canvasWnd.hwndFrame);

      while(WinGetMsg(hab, &qmsg, (HWND)NULLHANDLE, 0UL, 0UL))
      {
         SHORT scxold;
         SHORT scyold;
         SHORT scxnew;
         SHORT scynew;
         SHORT sslider;
         USHORT usidentifier;
         USHORT uscmd;

         switch(qmsg.msg)
         {
            case WM_PAINT:
               /*
                * mp1
                *   SHORT xLeft
                *   SHORT yBottom
                * mp2
                *   SHORT xRight
                *   SHORT yTop
                */
               rect.xLeft = SHORT1FROMMP(qmsg.mp1);
               rect.yBottom = SHORT2FROMMP(qmsg.mp1);
               rect.xRight = SHORT1FROMMP(qmsg.mp2);
               rect.yTop = SHORT2FROMMP(qmsg.mp2);
               AddRegionRect(hpsMem, hrgnTemp, canvasWnd.hrgnPaint, 1, &rect);

               #ifdef DEBUG_TERM
               printf("i WM_PAINT(%d,%d)-(%d,%d)\n", rect.xLeft, rect.yBottom, rect.xRight, rect.yTop);
               #endif

               /*
                * Make sure window gets updated.
                */
               WinPostQueueMsg(hmq, WTMSG_UPDATE, MPVOID, MPVOID);
               break;

            case WM_SIZE:
               scxold = SHORT1FROMMP(qmsg.mp1);
               scyold = SHORT2FROMMP(qmsg.mp1);
               scxnew = SHORT1FROMMP(qmsg.mp2);
               scynew = SHORT2FROMMP(qmsg.mp2);

               canvasWnd.sizlWindow.cx = scxnew;
               canvasWnd.sizlWindow.cy = scynew;

               if(pb)
               {
                  /* Reset frame flags before doing any horizontal or vertical adjustments */
                  canvasWnd.flFrame = 0;

                  if(scxnew != scxold)
                  {
                     horizontal_adjustments(&canvasWnd, pb, scxold, scxnew);
                  }

                  if(scynew != scyold)
                  {
                     vertical_adjustments(&canvasWnd, pb, scyold, scynew);
                  }

                  if(canvasWnd.flFrame)
                  {
                     WinSendMsg(canvasWnd.hwndFrame, WM_UPDATEFRAME, MPFROMLONG(canvasWnd.flFrame), MPVOID);
                  }

                  /*
                   * Notifty the canvas window about the new canvas area
                   */
                  WinSendMsg(canvasWnd.hwnd, WMU_SET_VISIBILITY_RECTS, (MPARAM)&canvasWnd.rclCanvas, (MPARAM)&canvasWnd.rclPixelBuffer);

                  #ifdef DEBUG_TERM
                  printf("i rclPixelBuffer: (%d,%d)-(%d,%d)\n", canvasWnd.rclPixelBuffer.xLeft, canvasWnd.rclPixelBuffer.yBottom, canvasWnd.rclPixelBuffer.xRight, canvasWnd.rclPixelBuffer.yTop);
                  printf("i rclCanvas: (%d,%d)-(%d,%d)\n", canvasWnd.rclCanvas.xLeft, canvasWnd.rclCanvas.yBottom, canvasWnd.rclCanvas.xRight, canvasWnd.rclCanvas.yTop);
                  #endif
               }
               else
               {
                  memset(&canvasWnd.rclCanvas, 0, sizeof(RECTL));
                  memset(&canvasWnd.rclPixelBuffer, 0, sizeof(RECTL));
               }
               setup_gray_region(hrgnGray, &canvasWnd);
               break;

            case WM_COMMAND:
               switch(SHORT1FROMMP(qmsg.mp1))
               {
                  case IDM_OPT_ALIGN_HORZ_LEFT:
                     WinPostQueueMsg(hmq, WTMSG_SET_HORZ_ALIGNMENT, MPFROMLONG(Lowest), MPVOID);
                     break;

                  case IDM_OPT_ALIGN_HORZ_CENTER:
                     WinPostQueueMsg(hmq, WTMSG_SET_HORZ_ALIGNMENT, MPFROMLONG(Center), MPVOID);
                     break;

                  case IDM_OPT_ALIGN_HORZ_RIGHT:
                     WinPostQueueMsg(hmq, WTMSG_SET_HORZ_ALIGNMENT, MPFROMLONG(Highest), MPVOID);
                     break;

                  case IDM_OPT_ALIGN_VERT_BOTTOM:
                     WinPostQueueMsg(hmq, WTMSG_SET_VERT_ALIGNMENT, MPFROMLONG(Lowest), MPVOID);
                     break;

                  case IDM_OPT_ALIGN_VERT_CENTER:
                     WinPostQueueMsg(hmq, WTMSG_SET_VERT_ALIGNMENT, MPFROMLONG(Center), MPVOID);
                     break;

                  case IDM_OPT_ALIGN_VERT_TOP:
                     WinPostQueueMsg(hmq, WTMSG_SET_VERT_ALIGNMENT, MPFROMLONG(Highest), MPVOID);
                     break;
               }
               break;

            case WM_HSCROLL:
               /* Remove redundant messages. */
               while(WinPeekMsg(hab, &q2, NULLHANDLE, WM_HSCROLL, WM_HSCROLL, PM_REMOVE))
               {
                  memcpy(&qmsg, &q2, sizeof(QMSG));           /* See #3 in notes.text */
               }

               usidentifier = SHORT1FROMMP(qmsg.mp1);
               sslider = SHORT1FROMMP(qmsg.mp2);
               uscmd = SHORT2FROMMP(qmsg.mp2);

               switch(uscmd)
               {
                  case SB_SLIDERTRACK:

                     canvasWnd.dxScroll = canvasWnd.rclPixelBuffer.xLeft - sslider;

                     #ifdef DEBUG_TERM
                     printf("i dxScroll=%d\n", canvasWnd.dxScroll);
                     #endif

                     WinPostQueueMsg(hmq, WTMSG_UPDATE, MPVOID, MPVOID);
                     break;
               }
               break;


            case WM_VSCROLL:
               /* Remove redundant messages. */
               while(WinPeekMsg(hab, &q2, NULLHANDLE, WM_VSCROLL, WM_VSCROLL, PM_REMOVE))
               {
                  memcpy(&qmsg, &q2, sizeof(QMSG));           /* See #3 in notes.text */
               }

               usidentifier = SHORT1FROMMP(qmsg.mp1);
               sslider = SHORT1FROMMP(qmsg.mp2);
               uscmd = SHORT2FROMMP(qmsg.mp2);

               switch(uscmd)
               {
                  case SB_SLIDERTRACK:
                     canvasWnd.dyScroll = canvasWnd.rclPixelBuffer.yBottom - (pb->cy-sslider);

                     #ifdef DEBUG_TERM
                     printf("i dyScroll=%d\n", canvasWnd.dyScroll);
                     #endif

                     WinPostQueueMsg(hmq, WTMSG_UPDATE, MPVOID, MPVOID);
                     break;
               }
               break;

            case WM_MOUSEMOVE:
               #ifdef DEBUG_TERM
               printf("i Cursor position(%d,%d)\n", (SHORT)SHORT1FROMMP(qmsg.mp1), (SHORT)SHORT2FROMMP(qmsg.mp1));
               #endif
               break;

            case WM_TRACKFRAME:
               /* Keep track of which borders where used to size frame. See special processing in the
                  horizontal_adjustments and vertical adjustments functions. */
               canvasWnd.fsTrackFlags = SHORT1FROMMP(qmsg.mp1);
               #ifdef DEBUG_TERM
               if(canvasWnd.fsTrackFlags & TF_MOVE)
               {
                  if((canvasWnd.fsTrackFlags & TF_MOVE) ^ TF_MOVE)
                  {
                     printf("i Track");
                     if(canvasWnd.fsTrackFlags & TF_LEFT)
                     {
                        printf(" Left");
                     }
                     if(canvasWnd.fsTrackFlags & TF_TOP)
                     {
                        printf(" Top");
                     }
                     if(canvasWnd.fsTrackFlags & TF_RIGHT)
                     {
                        printf(" Right");
                     }
                     if(canvasWnd.fsTrackFlags & TF_BOTTOM)
                     {
                        printf(" Bottom");
                     }
                     puts("");
                  }
                  else
                  {
                     puts("i Track Move");
                  }
               }
               #endif
               break;

            case WTMSG_LOAD_BITMAP:
               /*
                * Filename provided in mp1 is (hopefully?) allocated on heap; must free it here
                */

               /* Optimism is for weaklings; real men default to failiure */
               fNewPixelBuffer = FALSE;

               /* Attempt to load pixel buffer */
               pbLoad = LoadPixelBuffer(qmsg.mp1);

               /* Free the filename buffer */
               free(qmsg.mp1);
               _heapmin();
               if(pbLoad)
               {
                  /*
                   * Load was successful
                   */
                  if(pb)
                  {
                     /*
                      * Free previous buffer
                      */
                     FreePixelBuffer(pb);
                     pb = NULL;
                  }
                  pb = pbLoad;
                  pbLoad = NULL;

                  fNewPixelBuffer = TRUE;
               }
               if(fNewPixelBuffer)
               {
                  /*
                   * Since a new file was loaded, set up scrollbar, visiblity and all that schtick.
                   */

                  /* Temporary code; should not be needed */
                  WinQueryWindowPos(canvasWnd.hwnd, &swp);
                  canvasWnd.sizlWindow.cx = swp.cx;
                  canvasWnd.sizlWindow.cy = swp.cy;

                  /* Reset WM_UPDATEFRAME flag before calling adjustments functions */
                  canvasWnd.flFrame = 0;

                  /* Initalize scrollbars and adjust visibility paramters */
                  init_scrollbars(&canvasWnd, pb);
                  horizontal_adjustments(&canvasWnd, pb, (SHORT)canvasWnd.sizlWindow.cx, (SHORT)canvasWnd.sizlWindow.cx);
                  vertical_adjustments(&canvasWnd, pb, (SHORT)canvasWnd.sizlWindow.cy, (SHORT)canvasWnd.sizlWindow.cy);
                  if(canvasWnd.flFrame)
                  {
                     /*
                      * If frame controls where modified, WM_UPDATEFRAME must be sent to the frame window
                      */
                     WinSendMsg(canvasWnd.hwndFrame, WM_UPDATEFRAME, MPFROMLONG(canvasWnd.flFrame), MPVOID);
                  }

                  /* Notify the client window about which area is the canvas area */
                  WinSendMsg(canvasWnd.hwnd, WMU_SET_VISIBILITY_RECTS, (MPARAM)&canvasWnd.rclCanvas, (MPARAM)&canvasWnd.rclPixelBuffer);

                  #ifdef DEBUG_TERM
                  printf("i rclPixelBuffer: (%d,%d)-(%d,%d)\n", canvasWnd.rclPixelBuffer.xLeft, canvasWnd.rclPixelBuffer.yBottom, canvasWnd.rclPixelBuffer.xRight, canvasWnd.rclPixelBuffer.yTop);
                  printf("i rclCanvas: (%d,%d)-(%d,%d)\n", canvasWnd.rclCanvas.xLeft, canvasWnd.rclCanvas.yBottom, canvasWnd.rclCanvas.xRight, canvasWnd.rclCanvas.yTop);
                  #endif

                  setup_gray_region(hrgnGray, &canvasWnd);

                  /*
                   * If a bitmap was successfully loaded, enable the menuitems
                   */
                  EnableMenuItems(hwndAppMenu, IDM_OPTIONS, IDM_OPT_ALIGN_VERT_BOTTOM, TRUE);

                  /*
                   * Paint entire window
                   */
                  WinInvalidateRect(canvasWnd.hwnd, NULL, FALSE);
               }
               break;

            case WTMSG_SET_HORZ_ALIGNMENT:
               if(canvasWnd.horzAlign != LONGFROMMP(qmsg.mp1))
               {
                  switch(canvasWnd.horzAlign)
                  {
                     case Lowest:
                        idMenuItem = IDM_OPT_ALIGN_HORZ_LEFT;
                        break;

                     case Center:
                        idMenuItem = IDM_OPT_ALIGN_HORZ_CENTER;
                        break;

                     case Highest:
                        idMenuItem = IDM_OPT_ALIGN_HORZ_RIGHT;
                        break;

                     default:
                        idMenuItem = 0;
                        break;
                  }
                  if(WinIsMenuItemChecked(hwndAppMenu, idMenuItem) && idMenuItem)
                  {
                     AddRegionRect(hpsMem, hrgnTemp, canvasWnd.hrgnPaint, 1, &canvasWnd.rclCanvas);
                     WinCheckMenuItem(hwndAppMenu, idMenuItem, FALSE);
                  }

                  canvasWnd.horzAlign = LONGFROMMP(qmsg.mp1);

                  switch(canvasWnd.horzAlign)
                  {
                     case Lowest:
                        idMenuItem = IDM_OPT_ALIGN_HORZ_LEFT;
                        break;

                     case Center:
                        idMenuItem = IDM_OPT_ALIGN_HORZ_CENTER;
                        break;

                     case Highest:
                        idMenuItem = IDM_OPT_ALIGN_HORZ_RIGHT;
                        break;
                  }
                  WinCheckMenuItem(hwndAppMenu, idMenuItem, TRUE);

                  if(pb)
                  {
                     /*
                      * Redraw window
                      */
                     horizontal_adjustments(&canvasWnd, pb, (SHORT)canvasWnd.sizlWindow.cx, (SHORT)canvasWnd.sizlWindow.cx);
                     AddRegionRect(hpsMem, hrgnTemp, canvasWnd.hrgnPaint, 1, &canvasWnd.rclCanvas);
                     if(canvasWnd.sizlWindow.cx > pb->cx)
                     {
                        setup_gray_region(hrgnGray, &canvasWnd);
                     }
                     WinPostQueueMsg(hmq, WTMSG_UPDATE, MPVOID, MPVOID);

                     /* Notify the client window about which area is the canvas area */
                     WinSendMsg(canvasWnd.hwnd, WMU_SET_CANVAS_RECT, (MPARAM)&canvasWnd.rclCanvas, MPVOID);
                  }
               }
               break;

            case WTMSG_SET_VERT_ALIGNMENT:
               if(canvasWnd.vertAlign != LONGFROMMP(qmsg.mp1))
               {
                  switch(canvasWnd.vertAlign)
                  {
                     case Lowest:
                        idMenuItem = IDM_OPT_ALIGN_VERT_BOTTOM;
                        break;

                     case Center:
                        idMenuItem = IDM_OPT_ALIGN_VERT_CENTER;
                        break;

                     case Highest:
                        idMenuItem = IDM_OPT_ALIGN_VERT_TOP;
                        break;

                     default:
                        idMenuItem = 0;
                        break;
                  }
                  if(WinIsMenuItemChecked(hwndAppMenu, idMenuItem) && idMenuItem)
                  {
                     AddRegionRect(hpsMem, hrgnTemp, canvasWnd.hrgnPaint, 1, &canvasWnd.rclCanvas);
                     WinCheckMenuItem(hwndAppMenu, idMenuItem, FALSE);
                  }

                  canvasWnd.vertAlign = LONGFROMMP(qmsg.mp1);

                  switch(canvasWnd.vertAlign)
                  {
                     case Lowest:
                        idMenuItem = IDM_OPT_ALIGN_VERT_BOTTOM;
                        break;

                     case Center:
                        idMenuItem = IDM_OPT_ALIGN_VERT_CENTER;
                        break;

                     case Highest:
                        idMenuItem = IDM_OPT_ALIGN_VERT_TOP;
                        break;
                  }
                  WinCheckMenuItem(hwndAppMenu, idMenuItem, TRUE);

                  if(pb)
                  {
                     /*
                      * Redraw window
                      */
                     vertical_adjustments(&canvasWnd, pb, (SHORT)canvasWnd.sizlWindow.cy, (SHORT)canvasWnd.sizlWindow.cy);
                     AddRegionRect(hpsMem, hrgnTemp, canvasWnd.hrgnPaint, 1, &canvasWnd.rclCanvas);
                     if(canvasWnd.sizlWindow.cy > pb->cy)
                     {
                        setup_gray_region(hrgnGray, &canvasWnd);
                     }
                     WinPostQueueMsg(hmq, WTMSG_UPDATE, MPVOID, MPVOID);

                     /* Notify the client window about which area is the canvas area */
                     WinSendMsg(canvasWnd.hwnd, WMU_SET_CANVAS_RECT, (MPARAM)&canvasWnd.rclCanvas, MPVOID);
                  }
               }

               break;

            case WTMSG_UPDATE:
               /*
                * Since no paramters are passed with this message, and only one update event is interresting,
                * remove any redundant update-messages.
                */
               while(WinPeekMsg(hab, &qmsg, NULLHANDLE, WTMSG_UPDATE, WTMSG_UPDATE, PM_REMOVE))
               {
                  /* Do nothing. See #3 in notes.text */
               }

               /*
                * Paint invalidated regions
                */
               if(pb)
               {
                  HPS hpsCanvas = NULLHANDLE;

                  /*
                   * Scroll if needed. This must be done *after* painting since invalidated areas may be scrolled
                   * out of invalidated region before getting painted. This will result in the WTMSG_UPDATE
                   * message being called *twice* for each time window is updated in this function.
                   */
                  if(canvasWnd.dxScroll || canvasWnd.dyScroll)
                  {
                     /* Scroll the window canvas */
                     WinScrollWindow(canvasWnd.hwnd, canvasWnd.dxScroll, canvasWnd.dyScroll, &canvasWnd.rclCanvas, NULL, canvasWnd.hrgnPaint, NULL, 0UL);

                     /* change the visible area of the pixelbuffer according to scrolling information */
                     canvasWnd.rclPixelBuffer.xLeft -= canvasWnd.dxScroll;
                     canvasWnd.rclPixelBuffer.yBottom -= canvasWnd.dyScroll;
                     canvasWnd.rclPixelBuffer.xRight -= canvasWnd.dxScroll;
                     canvasWnd.rclPixelBuffer.yTop -= canvasWnd.dyScroll;

                     #ifdef DEBUG_TERM
                     printf("i UPDATE: rclPixelBuffer(%d,%d)-(%d-%d)\n", canvasWnd.rclPixelBuffer.xLeft, canvasWnd.rclPixelBuffer.yBottom, canvasWnd.rclPixelBuffer.xRight, canvasWnd.rclPixelBuffer.yTop);
                     #endif

                     /* Reset scrolling information */
                     canvasWnd.dxScroll = canvasWnd.dyScroll = 0;

                     WinSendMsg(canvasWnd.hwnd, WMU_SET_PB_RECT, (MPARAM)&canvasWnd.rclPixelBuffer, MPVOID);

                     /* Update the window - again */
                     /* WinPostQueueMsg(hmq, WTMSG_UPDATE, MPVOID, MPVOID); */
                  }

                  #ifdef DEBUG_TERM
                  puts("i Pre-paint:");
                  printf("i rclPixelBuffer: (%d,%d)-(%d,%d)\n", canvasWnd.rclPixelBuffer.xLeft, canvasWnd.rclPixelBuffer.yBottom, canvasWnd.rclPixelBuffer.xRight, canvasWnd.rclPixelBuffer.yTop);
                  printf("i rclCanvas: (%d,%d)-(%d,%d)\n", canvasWnd.rclCanvas.xLeft, canvasWnd.rclCanvas.yBottom, canvasWnd.rclCanvas.xRight, canvasWnd.rclCanvas.yTop);
                  #endif


                  /*
                   * Paint invalidated regions
                   */
                  hpsCanvas = WinGetPS(canvasWnd.hwnd);
                  if(hpsCanvas)
                  {
                     RECTL arcl[10];
                     RGNRECT rgnrcControl;
                     LONG lComplex;

                     /*
                      * Loop though all rectangles in region, bound to visible part pixel buffer
                      */
                     rgnrcControl.ircStart = 1;
                     rgnrcControl.crc = sizeof(arcl)/sizeof(RECTL);
                     rgnrcControl.crcReturned = 0;
                     rgnrcControl.ulDirection = RECTDIR_LFRT_BOTTOP;

                     while(GpiQueryRegionRects(hpsMem, canvasWnd.hrgnPaint, &canvasWnd.rclCanvas, &rgnrcControl, arcl))
                     {
                        ULONG i = 0;
                        if(rgnrcControl.crcReturned == 0UL)
                        {
                           /*
                            * No (more) rectangles to paint - break out of while-loop
                            */
                           break;
                        }
                        for(; i < rgnrcControl.crcReturned; i++)
                        {
                           POINTL aptl[4] = { { arcl[i].xLeft,      arcl[i].yBottom },
                                              { (arcl[i].xRight-1), (arcl[i].yTop-1) },
                                              { canvasWnd.rclPixelBuffer.xLeft+arcl[i].xLeft-canvasWnd.rclCanvas.xLeft, canvasWnd.rclPixelBuffer.yBottom+arcl[i].yBottom-canvasWnd.rclCanvas.yBottom },
                                              { canvasWnd.rclPixelBuffer.xLeft+arcl[i].xRight-canvasWnd.rclCanvas.xLeft, canvasWnd.rclPixelBuffer.yBottom+arcl[i].yTop-canvasWnd.rclCanvas.yBottom } };


                           #ifdef DEBUG_TERM
                           /* printf("i blit to(%d,%d)-(%d,%d) from(%d,%d)-(%d,%d)\n", aptl[0].x, aptl[0].y, aptl[1].x, aptl[1].y, aptl[2].x, aptl[2].y, aptl[3].x, aptl[3].y); */
                           #endif

                           LONG lHits = GpiDrawBits(hpsCanvas, pb->data, &pb->bmi2, 4L, aptl, ROP_SRCCOPY, BBO_IGNORE);
                           if(lHits != GPI_OK)
                           {
                              ERRORID erridErrorCode = WinGetLastError(hab);

                              #ifdef DEBUG_TERM
                              printf("e GpiDrawBits() failed in %s -> %s. WinGetLastError() returns %04x (%d).\n", __FILE__, __FUNCTION__, LOUSHORT(erridErrorCode), LOUSHORT(erridErrorCode));
                              #endif
                           }
                        }
                        rgnrcControl.ircStart+=i;
                     }

                     /*
                      * Any area which is OUTSIDE the canvas should be filled with palegray
                      */
                     lComplex = GpiCombineRegion(hpsMem, hrgnTemp, hrgnGray, canvasWnd.hrgnPaint, CRGN_AND);
                     switch(lComplex)
                     {
                        case RGN_RECT:
                        case RGN_COMPLEX:
                           GpiSetColor(hpsCanvas, CLR_PALEGRAY);
                           GpiPaintRegion(hpsCanvas, hrgnTemp);
                           break;
                     }
                     WinReleasePS(hpsCanvas);

                     /*
                      * Clear painted region so it doesn't keep getting painted over and over
                      */
                     GpiSetRegion(hpsMem, canvasWnd.hrgnPaint, 0, NULL);
                  }
               }
               else
               {
                  HPS hpsCanvas = WinGetPS(canvasWnd.hwnd);

                  /* No pixelbuffer is loaded; just paint gray */
                  if(hpsCanvas)
                  {
                     LONG lComplex = GpiCombineRegion(hpsMem, hrgnTemp, hrgnGray, canvasWnd.hrgnPaint, CRGN_AND);
                     switch(lComplex)
                     {
                        case RGN_RECT:
                        case RGN_COMPLEX:
                           GpiSetColor(hpsCanvas, CLR_PALEGRAY);
                           GpiPaintRegion(hpsCanvas, hrgnTemp);
                           break;
                     }
                     WinReleasePS(hpsCanvas);
                  }

                  /*
                   * Clear pained region so it doesn't keep getting painted
                   */
                  GpiSetRegion(hpsMem, canvasWnd.hrgnPaint, 0, NULL);
               }
               break;

            default:
               break;
         }
      }

      /*
       * WM_QUIT has been posted -- time to clean up
       */

      if(pb)
      {
         /*
          * Destroy pixelbuffer
          */
         FreePixelBuffer(pb);
      }


      if(hdcMem)
      {
         if(hpsMem)
         {
            /*
             * Destroy regions before destroying presentation space.
             * Note: If region is NULLHANDLE, the call returns with no error, so there is no need to check if the
             *       regions are allocated.
             */
            GpiDestroyRegion(hpsMem, hrgnTemp);
            GpiDestroyRegion(hpsMem, hrgnGray);
            GpiDestroyRegion(hpsMem, canvasWnd.hrgnPaint);

            GpiDestroyPS(hpsMem);
         }
         DevCloseDC(hdcMem);
      }
   }

   /*
    * Terminate UI thread message loop
    */
   WinPostMsg(canvasWnd.hwnd, WM_QUIT, MPVOID, MPVOID);

   if(hab)
   {
      if(hmq)
      {
         WinDestroyMsgQueue(hmq);
      }
      WinTerminate(hab);
   }

   _endthread();
}


static void _Optlink setDefaultWindowPosition(HWND hwndAppFrame)
{
   ULONG ulDesktopWidth = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
   ULONG ulDesktopHeight = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
   LONG cx = 640;
   LONG cy = 480;
   LONG x = (ulDesktopWidth/2)-(cx/2);
   LONG y = (ulDesktopHeight/2)-(cy/2);

   WinSetWindowPos(hwndAppFrame, HWND_TOP, x, y, cx, cy, SWP_SHOW | SWP_ACTIVATE | SWP_MOVE | SWP_SIZE | SWP_ZORDER);
}


/*
 * Add one or more rectangles to a region; needs a temporary region.
 */
LONG _Inline AddRegionRect(HPS hps, HRGN hrgnTemp, HRGN hrgn, LONG lcount, PRECTL arclRectangles)
{
   if(GpiSetRegion(hps, hrgnTemp, lcount, arclRectangles))
   {
      return GpiCombineRegion(hps, hrgn, hrgn, hrgnTemp, CRGN_OR);
   }
   return RGN_ERROR;
}



/*
 * Initialize scrollbars to bottom left corner of bitmap.
 */
void _Inline init_scrollbars(PCANVASWNDDATA wnd, PPIXELBUFFER pb)
{
   SHORT sPos = 0;

   WinSendMsg(wnd->hwndHorzScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(wnd->sizlWindow.cx, pb->cx), MPVOID);
   WinSendMsg(wnd->hwndHorzScroll, SBM_SETSCROLLBAR, MPFROMSHORT(sPos), MPFROM2SHORT(0, pb->cx-wnd->sizlWindow.cx));

   sPos = pb->cy;
   WinSendMsg(wnd->hwndVertScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(wnd->sizlWindow.cy, pb->cy), MPVOID);
   WinSendMsg(wnd->hwndVertScroll, SBM_SETSCROLLBAR, MPFROMSHORT(sPos), MPFROM2SHORT(wnd->sizlWindow.cy, pb->cy));
}


void _Inline horizontal_adjustments(PCANVASWNDDATA wnd, PPIXELBUFFER pb, SHORT scxold, SHORT scxnew)
{
   /*
    * Check if pixebuffer (canvas) is larger than window
    */
   if(pb->cx > wnd->sizlWindow.cx)
   {
      SHORT sPos = (SHORT)WinSendMsg(wnd->hwndHorzScroll, SBM_QUERYPOS, MPVOID, MPVOID);

      /*
       * Entire pixelbuffer doesn't fit in window -- Scrollbars are needed.
       * Check is scrollbar is owned by HWND_OBJECT.
       */
      if(WinIsChild(wnd->hwndHorzScroll, HWND_OBJECT))
      {
         /* show scrollbar  */
         WinSetParent(wnd->hwndHorzScroll, wnd->hwndFrame, TRUE);
         wnd->flFrame |= FCF_HORZSCROLL;
      }
      else
      {
         /*
          * See #4 in notes.text
          */
         if((wnd->fsTrackFlags ^ TF_MOVE) && (wnd->fsTrackFlags & TF_LEFT))
         {
            if(scxnew < scxold)
            {
               /*
                * Window is shrinking horizontally
                */
               sPos += (scxold-scxnew);
            }
            else if(scxnew > scxold)
            {
               /*
                * Window is growing horizontally
                */
               sPos -= (scxnew-scxold);
            }
         }
      }

      /* Reset track-left-border flag */
      wnd->fsTrackFlags &= ~TF_LEFT;

      /*
       * Make sure scroll bar isn't out of bound
       */
      sPos = max(0, sPos);
      sPos = min(pb->cx-wnd->sizlWindow.cx, sPos);
      wnd->rclPixelBuffer.xLeft = sPos;
      wnd->rclPixelBuffer.xRight = wnd->rclPixelBuffer.xLeft + wnd->sizlWindow.cx;

      WinSendMsg(wnd->hwndHorzScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(wnd->sizlWindow.cx, pb->cx), MPVOID);
      WinSendMsg(wnd->hwndHorzScroll, SBM_SETSCROLLBAR, MPFROMSHORT(sPos), MPFROM2SHORT(0, pb->cx-wnd->sizlWindow.cx));

      wnd->rclCanvas.xLeft = 0;
      wnd->rclCanvas.xRight = wnd->rclCanvas.xLeft + wnd->sizlWindow.cx;
   }
   else
   {
      /*
       * Entire pixelbuffer does fit in window -- horizontal scrollbar is not needed.
       * Check if horizontal scrollbar's parent is frame window.
       */
      if(WinIsChild(wnd->hwndHorzScroll, wnd->hwndFrame))
      {
         /*
          * Hide horizontal scrollbar
          */
         WinSetParent(wnd->hwndHorzScroll, HWND_OBJECT, TRUE);
         wnd->flFrame |= FCF_HORZSCROLL;
      }

      wnd->rclPixelBuffer.xLeft = 0;
      wnd->rclPixelBuffer.xRight = pb->cx;

      switch(wnd->horzAlign)
      {
         case Lowest:
            wnd->rclCanvas.xLeft = 0;
            break;

         case Center:
            wnd->rclCanvas.xLeft = (wnd->sizlWindow.cx/2)-(pb->cx/2);
            break;

         case Highest:
            wnd->rclCanvas.xLeft = wnd->sizlWindow.cx-pb->cx;
            break;
      }
      wnd->rclCanvas.xRight = wnd->rclCanvas.xLeft + pb->cx;

      wnd->dxScroll = 0;
   }
}


void _Inline vertical_adjustments(PCANVASWNDDATA wnd, PPIXELBUFFER pb, SHORT scyold, SHORT scynew)
{
   if(pb->cy > wnd->sizlWindow.cy)
   {
      SHORT sPos = (SHORT)WinSendMsg(wnd->hwndVertScroll, SBM_QUERYPOS, MPVOID, MPVOID);

      /* entire pixelbuffer doesn't fit in window */
      if(WinIsChild(wnd->hwndVertScroll, HWND_OBJECT))
      {
         /* show vertical scrollbar */
         WinSetParent(wnd->hwndVertScroll, wnd->hwndFrame, TRUE);
         wnd->flFrame |= FCF_VERTSCROLL;
      }
      else
      {
         /*
          * See #4 in notes.text
          */
         if((wnd->fsTrackFlags ^ TF_MOVE) && (wnd->fsTrackFlags & TF_BOTTOM))
         {
            if(scynew < scyold)
            {
               /*
                * Window is shrinking vertically
                */
               sPos -= (scyold-scynew);
            }
            else
            {
               /*
                * Window is growing vertically
                */
               sPos += (scynew-scyold);
            }
         }
      }

      wnd->fsTrackFlags &= ~TF_BOTTOM;

      /*
       * Make sure scrollbar isn't out of bound
       */
      sPos = max(wnd->sizlWindow.cy, sPos);
      sPos = min(pb->cy, sPos);
      wnd->rclPixelBuffer.yBottom = pb->cy-sPos;
      wnd->rclPixelBuffer.yTop = wnd->rclPixelBuffer.yBottom + wnd->sizlWindow.cy;

      WinSendMsg(wnd->hwndVertScroll, SBM_SETTHUMBSIZE, MPFROM2SHORT(wnd->sizlWindow.cy, pb->cy), MPVOID);
      WinSendMsg(wnd->hwndVertScroll, SBM_SETSCROLLBAR, MPFROMSHORT(sPos), MPFROM2SHORT(wnd->sizlWindow.cy, pb->cy));

      wnd->rclCanvas.yBottom = 0;
      wnd->rclCanvas.yTop = wnd->rclCanvas.yBottom + wnd->sizlWindow.cy;
   }
   else
   {
      /* entire pixelbuffer does fit in window */
      if(WinIsChild(wnd->hwndVertScroll, wnd->hwndFrame))
      {
         /* hide vertical scrollbar */
         WinSetParent(wnd->hwndVertScroll, HWND_OBJECT, TRUE);
         wnd->flFrame |= FCF_VERTSCROLL;
      }

      wnd->rclPixelBuffer.yBottom = 0;
      wnd->rclPixelBuffer.yTop = pb->cy;

      switch(wnd->vertAlign)
      {
         case Lowest:
            wnd->rclCanvas.yBottom = 0;
            break;

         case Center:
            wnd->rclCanvas.yBottom = (wnd->sizlWindow.cy/2)-(pb->cy/2);
            break;

         case Highest:
            wnd->rclCanvas.yBottom = wnd->sizlWindow.cy-pb->cy;
            break;
      }
      wnd->rclCanvas.yTop = wnd->rclCanvas.yBottom + pb->cy;

      wnd->dyScroll = 0;
   }
}




void _Inline setup_gray_region(HRGN hrgnGray, PCANVASWNDDATA wnd)
{
   HPS hps = WinGetPS(wnd->hwnd);
   if(hps)
   {
      HWND hrgnCanvas = GpiCreateRegion(hps, 1, &wnd->rclCanvas);
      RECTL rect;

      /*
       * Create gray region
       *   1) Destroy previous gray region
       *   2) Create a region containing all of window in hrgnGray
       *   3) Subtract canvas region from hrgnTemp and stire in hrgnGray
       *   4) Destroy hrgnTemp
       */
      WinQueryWindowRect(wnd->hwnd, &rect);
      GpiSetRegion(hps, hrgnGray, 1, &rect);
      if(hrgnGray != RGN_ERROR)
      {
         LONG lComplex = GpiCombineRegion(hps, hrgnGray, hrgnGray, hrgnCanvas, CRGN_DIFF);
      }

      GpiDestroyRegion(hps, hrgnCanvas);

      WinReleasePS(hps);
   }
}


/*
 * Enable a range of menu items.
 */
void _Inline EnableMenuItems(HWND hwndMenu, ULONG idFirst, ULONG idLast, BOOL fEnable)
{
   ULONG id = idFirst;
   for(; id <= idLast; id++)
   {
      WinEnableMenuItem(hwndMenu, id, fEnable);
   }
}
