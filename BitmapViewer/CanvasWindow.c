#pragma strings(readonly)

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINSTDFILE
#define INCL_WINPOINTERS
#define INCL_WININPUT
#define INCL_WINRECTANGLES
#define INCL_WINDIALOGS
#define INCL_DOSPROCESS

#include <os2.h>

#include <memory.h>
#include <malloc.h>
#include <string.h>

#ifdef DEBUG_TERM
#include <stdio.h>
#endif

#include "CanvasWindow.h"
#include "WorkerThread.h"

#include "resources.h"


typedef struct _WINDOWDATA
{
   HAB hab;
   HMQ hmqWorker;
   RECTL rclCanvas;                      /* Area of window comtaining the bitmap */
   RECTL rclPixelBuffer;                 /* Area of pixelbuffer in canvas        */
   HPOINTER hptrCrosshair;               /* Handle of crosshair pointer          */
   int tidWorker;
}WINDOWDATA, *PWINDOWDATA;


static MRESULT EXPENTRY WindowProcedure(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);



BOOL _Optlink registerCanvasClass(HAB hab)
{
   BOOL fSuccess = FALSE;
   LONG lLength = 0;
   char achClass[256] = "";

   /*
    * Load class name from file's resources
    */
   lLength = WinLoadString(hab, (HMODULE)NULLHANDLE, IDS_CANVASCLASS, sizeof(achClass), achClass);
   if(lLength != 0L)
   {
      fSuccess = WinRegisterClass(hab, achClass, WindowProcedure, CS_SIZEREDRAW, sizeof(PWINDOWDATA));
   }
   return fSuccess;
}


static MRESULT EXPENTRY WindowProcedure(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT mReturn = 0;
   BOOL fHandled = TRUE;
   RECTL rect;
   HPS hps;
   PWINDOWDATA wd = (PWINDOWDATA)WinQueryWindowPtr(hwnd, 0);
   FILEDLG fdlg;

   switch(msg)
   {
      case WM_CREATE:
         wd = malloc(sizeof(WINDOWDATA));
         if(wd)
         {
            /* Clear window data buffer */
            memset(wd, 0, sizeof(WINDOWDATA));

            wd->hab = WinQueryAnchorBlock(hwnd);
            wd->tidWorker = -1;

            /* Load the crosshair pointer */
            wd->hptrCrosshair = WinLoadPointer(HWND_DESKTOP, (HMODULE)NULLHANDLE, PTR_CROSSHAIR);

            /* Last - but not leat - store the window buffer pointer in the window data area */
            WinSetWindowPtr(hwnd, 0, wd);
         }
         break;

      case WM_PAINT:
         hps = WinBeginPaint(hwnd, NULLHANDLE, &rect);
         if(hps)
         {
            if(wd->hmqWorker)
            {
               /*
                * Post a paint message to the worker thread; pass paint rectangle through the mp1 and mp2 paramters.
                */
               if(!WinPostQueueMsg(wd->hmqWorker, WM_PAINT, MPFROM2SHORT((SHORT)rect.xLeft, (SHORT)rect.yBottom), MPFROM2SHORT((SHORT)rect.xRight, (SHORT)rect.yTop)))
               {
                  #ifdef DEBUG_TERM
                  puts("e WinPostQueue() for WM_PAINT failed.");
                  #endif
               }
            }
            else
            {
               /*
                * In the event that the worker thread hasn't been initialized
                * by the time this window recieves a paint request, just fill with pale gray.
                */
               WinFillRect(hps, &rect, CLR_PALEGRAY);
            }
            WinEndPaint(hps);
         }
         break;

      case WM_SIZE:
      case WM_HSCROLL:
      case WM_VSCROLL:
         if(wd->hmqWorker)
         {
            WinPostQueueMsg(wd->hmqWorker, msg, mp1, mp2);
         }
         break;


      case WM_COMMAND:
         /*
          * Note: since all menuitems are disabled until worker thread is launched, no need to check
          * if the worker thread has been initialized. If we get to any point which required the worker thread
          * it should have been initialized.
          */
         switch(SHORT1FROMMP(mp1))
         {
            case IDM_FILE_LOAD:
               memset(&fdlg, 0, sizeof(fdlg));
               fdlg.cbSize = sizeof(fdlg);
               fdlg.fl = FDS_CENTER | FDS_OPEN_DIALOG;
               memcpy(fdlg.szFullFile, "*.bmp", 6);
               if(WinFileDlg(HWND_DESKTOP, hwnd, &fdlg) != NULLHANDLE)
               {
                  if(fdlg.lReturn == DID_OK)
                  {
                     LONG len = strlen(fdlg.szFullFile)+1;
                     char *tmp = malloc(len);
                     memcpy(tmp, fdlg.szFullFile, len);
                     WinPostQueueMsg(wd->hmqWorker, WTMSG_LOAD_BITMAP, (MPARAM)tmp, MPVOID);
                  }
               }
               break;

            case IDM_OPT_ALIGN_HORZ_LEFT:
            case IDM_OPT_ALIGN_HORZ_CENTER:
            case IDM_OPT_ALIGN_HORZ_RIGHT:
            case IDM_OPT_ALIGN_VERT_BOTTOM:
            case IDM_OPT_ALIGN_VERT_CENTER:
            case IDM_OPT_ALIGN_VERT_TOP:
               WinPostQueueMsg(wd->hmqWorker, WM_COMMAND, mp1, mp2);
               break;


            default:
               fHandled = FALSE;
               break;
         }
         break;

      case WM_MOUSEMOVE:
         if(wd->hmqWorker)
         {
            POINTL ptlMouse = { SHORT1FROMMP(mp1), SHORT2FROMMP(mp1) };


            /*
             * Check if the pointer is within the canvas area
             */
            if(WinPtInRect(wd->hab, &wd->rclCanvas, &ptlMouse))
            {
               /*
                * Pointer is within canvas area, set appropriate pointer
                */
               WinSetPointer(HWND_DESKTOP, wd->hptrCrosshair);
               if(wd->rclCanvas.xLeft || wd->rclCanvas.yBottom)
               {
                  mp1 = MPFROM2SHORT(SHORT1FROMMP(mp1)-wd->rclCanvas.xLeft, SHORT2FROMMP(mp1)-wd->rclCanvas.yBottom);
               }
               mp1 = MPFROM2SHORT(SHORT1FROMMP(mp1)+wd->rclPixelBuffer.xLeft, SHORT2FROMMP(mp1)+wd->rclPixelBuffer.yBottom);
               if(!WinPostQueueMsg(wd->hmqWorker, WM_MOUSEMOVE, mp1, mp2))
               {
                  #ifdef DEBUG_TERM
                  printf("e WinPostQueueMsg(%08x,%08x,%08x,%08x) failed in %s -> %s.\n", wd->hmqWorker, msg, mp1, mp2, __FILE__, __FUNCTION__);
                  #endif
               }
            }
            else
            {
               /*
                * Pointer is within window, but not within canvas area; call
                * default window procedure to get system default pointer.
                */
               return WinDefWindowProc(hwnd, WM_MOUSEMOVE, mp1, mp2);
            }
         }
         else
         {
            return WinDefWindowProc(hwnd, WM_MOUSEMOVE, mp1, mp2);
         }
         break;


      case WM_CLOSE:
         if(wd->hmqWorker)
         {
            /*
             * Make the worker thread quit its message queue. When it does, it will clean up all it's data and
             * then post a quit message to this thread, making this thread terminate.
             */
            while(!WinPostQueueMsg(wd->hmqWorker, WM_QUIT, MPVOID, MPVOID))
            {
               LONG lLength = 0;
               char achTitle[40] = "";

               lLength = WinLoadString(wd->hab, (HMODULE)NULLHANDLE, IDS_POSTWRKRERRORTITLE, sizeof(achTitle), achTitle);
               if(lLength != 0)
               {
                  char achMessage[256] = "";

                  lLength = WinLoadMessage(wd->hab, (HMODULE)NULLHANDLE, IDMSG_POSTQUITMSGFAILED, sizeof(achMessage), achMessage);
                  if(lLength != 0)
                  {
                     ULONG usResponse = 0;
                     usResponse = WinMessageBox(HWND_DESKTOP, hwnd, achMessage, achTitle, IDMSGBOX_POSTQUITERROR, MB_ABORTRETRYIGNORE | MB_ERROR | MB_DEFBUTTON2 | MB_APPLMODAL);
                     if(usResponse == MBID_YES)
                     {
                        /* Kill thread */
                        DosKillThread(wd->tidWorker);
                        WinPostMsg(hwnd, WM_QUIT, MPVOID, MPVOID);
                     }
                     else if(usResponse == MBID_NO)
                     {
                        /* Break out of loop */
                        break;
                     }
                     else
                     {
                        /* If forced termination wasn't requested and user ddoesn't wish to try
                           again, break out of while loop */
                        continue;
                     }
                  }
               }
            }
         }
         else
         {
            /*
             * No worker thread has been initialized; just terminate this thread.
             */
            WinPostMsg(hwnd, WM_QUIT, MPVOID, MPVOID);
         }
         break;

      case WM_TRACKFRAME:
         /*
          * This message is sent here by the frame SubProc, it should just be redirected to the worker thread.
          * TODO: Give the frame window the hmqWorker so it can do this by itself.
          */
         if(wd->hmqWorker)
         {
            WinPostQueueMsg(wd->hmqWorker, WM_TRACKFRAME, mp1, MPVOID);
         }
         break;

      case WM_DESTROY:
         WinDestroyPointer(wd->hptrCrosshair);
         /*
          * Release window data buffer
          */
         free(wd);
         break;

      case WMU_SET_CANVAS_RECT:
         memcpy(&wd->rclCanvas, mp1, sizeof(RECTL));
         break;

      case WMU_SET_PB_RECT:
         memcpy(&wd->rclPixelBuffer, mp1, sizeof(RECTL));
         break;

      case WMU_SET_VISIBILITY_RECTS:
         memcpy(&wd->rclCanvas, mp1, sizeof(RECTL));
         memcpy(&wd->rclPixelBuffer, mp2, sizeof(RECTL));
         break;

      case WMU_CANVAS_NOTIFICATION:
         switch(LONGFROMMP(mp1))
         {
            case WORKERTHREAD_TID:
               wd->tidWorker = LONGFROMMP(mp2);
               break;

            case WORKERTHREAD_HMQ:
               wd->hmqWorker = LONGFROMMP(mp2);
               #ifdef DEBUG_TERM
               printf("i CanvasWindow got hmqWorker: %08x\n", wd->hmqWorker);
               #endif
               break;
         }
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