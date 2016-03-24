/************************************************************************************************\
 * This file is really a part of another project. I do not think I'll make any updates to this  *
 * one, so any bugs, and incomplete functions, will probably remain that way. Feel free to use  *
 * in whichever way suits you.                                                                  *
\************************************************************************************************/
#define INCL_WINWINDOWMGR
#define INCL_WINSYS
#define INCL_GPIPRIMITIVES
#define INCL_GPIBITMAPS
#define INCL_GPILOGCOLORTABLE
#define INCL_DOSERRORS

#include <os2.h>

#include <string.h>

#include "ProgressBar.h"

// local structures
typedef struct _COLORS
{
   ULONG lOuterFrameBright;
   ULONG lOuterFrameDark;
   ULONG lInnerFrameBright;
   ULONG lInnerFrameDark;
   ULONG lBarBright;
   ULONG lBarDark;
   ULONG lBar;
   ULONG lBackground;
}COLORS, *PCOLORS;

typedef struct _PROGRESSBAR
{
   ULONG    id;                          // control's id
   HWND     hwndParent;                  // parent window
   HWND     hwndOwner;                   // owner window
   ULONG    ulMax;                       // maximum value
   ULONG    ulMin;                       // minimum value
   ULONG    ulCurrent;                   // current value
   COLORS   color;                       // the contol's colors
   ULONG    cx;                          // control width
   ULONG    cy;                          // control height
   ULONG    flState;                     // see 'state flags' below
}PROGRESSBAR, *PPROGRESSBAR;

// state flags
#define PBS_DISABLED                     0x00000001


// window data
#define QWP_CTRLDATA                     0
#define QW_EXTRA                         QWP_CTRLDATA+sizeof(PPROGRESSBAR)


/*
** Local function prototypes
** (prefix 'pbc' (ProgressBarControl))
*/
MRESULT EXPENTRY pbcProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);
BOOL pbcInit(HWND hWnd, MPARAM mp1, MPARAM mp2);
void pbcSetDefaultColors(HWND hWnd);
void pbcResize(HWND hWnd, MPARAM mp2);
void pbcPaint(HWND hWnd);
void pbcDestroy(HWND hWnd);
void pbcEnable(HWND hWnd, MPARAM mp1);
void pbcSetFrameOuterColors(HWND hWnd, long lHi, long lLo);
void pbcSetFrameInnerColors(HWND hWnd, long lHi, long lLo);
void pbcSetBarBorderColors(HWND hWnd, long lHi, long lLo);
void _System Draw3DFrame(HPS hPS, PRECTL pRectl, long lHi, long lLo, BOOL bLower);
long inline iround(float f);

void pbcQueryRange(HWND hWnd, MPARAM mp1, MPARAM mp2);
BOOL pbcSetRange(HWND hWnd, MPARAM mp1, MPARAM mp2);
MRESULT pbcQueryValue(HWND hWnd);
BOOL pbcSetValue(HWND hWnd, MPARAM mp1);


BOOL pbcRegisterClass(void)
{
   return WinRegisterClass((HAB)0, WC_PROGRESSBAR, pbcProc, CS_SYNCPAINT | CS_HITTEST | CS_SIZEREDRAW, QW_EXTRA);
}


MRESULT EXPENTRY pbcProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   BOOL     fHandled = TRUE;
   MRESULT  mReturn = 0;

   switch(msg)
   {
      case WM_CREATE:
         return (MRESULT)pbcInit(hWnd, mp1, mp2);

      case WM_PAINT:
         pbcPaint(hWnd);
         break;

      case WM_SIZE:
         pbcResize(hWnd, mp2);
         break;

      case PBM_SETFRAMEOUTERBORDERCOLORS:
         pbcSetFrameOuterColors(hWnd, (long)mp1, (long)mp2);
         break;

      case PBM_SETFRAMEINNERBORDERCOLORS:
         pbcSetFrameInnerColors(hWnd, (long)mp1, (long)mp2);
         break;

      case PBM_SETBARBORDERCOLORS:
         pbcSetBarBorderColors(hWnd, (long)mp1, (long)mp2);
         break;

      case PBM_SETBARCOLORS:
         break;

      case PBM_SETRANGE:
         if((ULONG)mp1 >= (ULONG)mp2)
            return (MPARAM)FALSE;
         return (MPARAM)pbcSetRange(hWnd, mp1, mp2);

      case PBM_SETVALUE:
         return (MPARAM)pbcSetValue(hWnd, mp1);

      case PBM_QUERYRANGE:
         pbcQueryRange(hWnd, mp1, mp2);
         break;

      case PBM_QUERYVALUE:
         mReturn = pbcQueryValue(hWnd);
         break;

      case WM_DESTROY:
         pbcDestroy(hWnd);
         break;

      case WM_ENABLE:
         pbcEnable(hWnd, mp1);
         break;

      default:
         fHandled = FALSE;
   }

   if(!fHandled)
      mReturn = WinDefWindowProc(hWnd, msg, mp1, mp2);

   return mReturn;
}


BOOL pbcInit(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   BOOL          fAbort = FALSE;
   PPBCDATA      pCtlData = (PPBCDATA)mp1;
   BOOL          fCtlData = FALSE;
   PCREATESTRUCT pCreate = (PCREATESTRUCT)mp2;
   PPROGRESSBAR  pData = NULL;

   // find out if we got some createdate from WinCreateWindow()
   if(pCtlData)
      if(pCtlData->cbSize == sizeof(PBCDATA))
         fCtlData = TRUE;

   // allocate memory for controldata
   if(DosAllocMem((PPVOID)&pData, sizeof(PROGRESSBAR), PAG_READ | PAG_WRITE | PAG_COMMIT) != NO_ERROR)
      return TRUE;

   // store pointer to controldata in windowdata
   WinSetWindowPtr(hWnd, QWP_CTRLDATA, pData);

   // clear the contoldata
   memset(pData, 0, sizeof(PROGRESSBAR));

   pData->id = pCreate->id;
   pData->hwndOwner = pCreate->hwndOwner;
   pData->hwndParent = pCreate->hwndParent;
   pData->cx = pCreate->cx;
   pData->cy = pCreate->cy;

   if((pCreate->flStyle & WS_DISABLED) == WS_DISABLED)
      pData->flState |= PBS_DISABLED;

   // set the default colors
   pbcSetDefaultColors(hWnd);

   // set the default min/max/current values for the control
   if(fCtlData)
   {
      // set them from the createdata
      // ToDo: Make sure that the values are 'valid'
      pData->ulMin = pCtlData->ulMin;
      pData->ulMax = pCtlData->ulMax;
      pData->ulCurrent = pCtlData->ulCurrent;
   }
   else
   {
      // set them to default values
      pData->ulMin = 0;
      pData->ulMax = 100;
      pData->ulCurrent = 0;
   }

   return fAbort;
}

LONG lGetPresParam(HWND hWnd, ULONG ulID1, ULONG ulID2, LONG lDefault)
{
   HPS   hPS;
   LONG  lClr;
   ULONG ulID;

   if(WinQueryPresParam(hWnd, ulID1, ulID2, &ulID, 4, (PVOID)&lClr, QPF_NOINHERIT | QPF_ID1COLORINDEX | QPF_ID2COLORINDEX | QPF_PURERGBCOLOR))
      return(lClr);
   else
      if((lDefault >= SYSCLR_SHADOWHILITEBGND) && (lDefault <= SYSCLR_HELPHILITE))
         return(WinQuerySysColor(HWND_DESKTOP, lDefault, 0L));
      else
         if((lClr = GpiQueryRGBColor(hPS = WinGetPS(hWnd), LCOLOPT_REALIZED, lDefault)) == GPI_ALTERROR )
         {
            WinReleasePS(hPS);
            return(lDefault);
         }
         else
         {
            WinReleasePS(hPS);
            return(lClr);
         }
}

LONG GetRGBColor(HPS hPS, LONG lColor)
{
   LONG  lRGBClr;

   if((lRGBClr = GpiQueryRGBColor(hPS, LCOLOPT_REALIZED, lColor)) == GPI_ALTERROR)
      return lColor;
   return lRGBClr;
}

void pbcSetDefaultColors(HWND hWnd)
{
   PPROGRESSBAR  pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   HPS   hPS = NULLHANDLE;

   pData->color.lBar        = lGetPresParam(hWnd, PP_FOREGROUNDCOLOR, PP_FOREGROUNDCOLORINDEX, CLR_DARKGRAY);
   pData->color.lBackground = lGetPresParam(hWnd, PP_BACKGROUNDCOLOR, PP_BACKGROUNDCOLORINDEX, CLR_DARKCYAN);

   hPS = WinGetPS(hWnd);

   pData->color.lOuterFrameBright = GetRGBColor(hPS, CLR_WHITE);
   pData->color.lOuterFrameDark   = GetRGBColor(hPS, CLR_DARKGRAY);
   pData->color.lInnerFrameBright = GetRGBColor(hPS, CLR_PALEGRAY);
   pData->color.lInnerFrameDark   = GetRGBColor(hPS, CLR_BLACK);
   pData->color.lBarBright        = GetRGBColor(hPS, CLR_WHITE);
   pData->color.lBarDark          = GetRGBColor(hPS, CLR_BLACK);

   WinReleasePS(hPS);
}

void pbcSetFrameOuterColors(HWND hWnd, long lHi, long lLo)
{
   PPROGRESSBAR  pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   HPS   hPS = WinGetPS(hWnd);

   pData->color.lOuterFrameBright = GetRGBColor(hPS, lHi);
   pData->color.lOuterFrameDark   = GetRGBColor(hPS, lLo);

   WinReleasePS(hPS);
}


void pbcSetFrameInnerColors(HWND hWnd, long lHi, long lLo)
{
   PPROGRESSBAR  pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   HPS   hPS = WinGetPS(hWnd);

   pData->color.lInnerFrameBright = GetRGBColor(hPS, lHi);
   pData->color.lInnerFrameDark   = GetRGBColor(hPS, lLo);

   WinReleasePS(hPS);
}

void pbcSetBarBorderColors(HWND hWnd, long lHi, long lLo)
{
   PPROGRESSBAR  pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   HPS   hPS = WinGetPS(hWnd);

   pData->color.lBarBright = GetRGBColor(hPS, lHi);
   pData->color.lBarDark   = GetRGBColor(hPS, lLo);

   WinReleasePS(hPS);
}


void pbcResize(HWND hWnd, MPARAM mp2)
{
   PPROGRESSBAR pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   pData->cx = SHORT1FROMMP(mp2);
   pData->cy = SHORT2FROMMP(mp2);

   WinInvalidateRect(hWnd, NULL, FALSE);
}

void pbcDestroy(HWND hWnd)
{
   PPROGRESSBAR  pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   DosFreeMem(pData);
}

void pbcEnable(HWND hWnd, MPARAM mp1)
{
   PPROGRESSBAR pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   if((BOOL)SHORT1FROMMP(mp1))
      pData->flState &= ~PBS_DISABLED;
   else
      pData->flState |= PBS_DISABLED;

   WinInvalidateRect(hWnd, (PRECTL)NULL, TRUE);
}

void pbcPaint(HWND hWnd)
{
   HPS      hPS = NULLHANDLE;
   RECTL    rectl;
   PPROGRESSBAR pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   hPS = WinBeginPaint(hWnd, (HPS)NULL, (PRECTL)NULL);

   GpiCreateLogColorTable(hPS, 0L, LCOLF_RGB, 0L, 0L, (PLONG)NULL);

   if((pData->flState & PBS_DISABLED) == PBS_DISABLED)
      GpiSetLineType(hPS, LINETYPE_ALTERNATE);
   else
      GpiSetLineType(hPS, LINETYPE_SOLID);

   WinQueryWindowRect(hWnd, &rectl);

//   rectl.xLeft = rectl.yBottom = 0;
   rectl.xRight = rectl.xLeft+pData->cx;
   rectl.yTop = rectl.yBottom+pData->cy;

   rectl.xRight--;
   rectl.yTop--;

   Draw3DFrame(hPS, &rectl, pData->color.lOuterFrameBright, pData->color.lOuterFrameDark, TRUE);

   rectl.xLeft++;
   rectl.yBottom++;
   rectl.xRight--;
   rectl.yTop--;
   Draw3DFrame(hPS, &rectl, pData->color.lInnerFrameBright, pData->color.lInnerFrameDark, TRUE);

   rectl.xLeft++;
   rectl.yBottom++;

   ULONG tmpCurrent = pData->ulCurrent - pData->ulMin;  // To make things easier, we set the range when drawing to 0-x
   ULONG tmpMax = pData->ulMax - pData->ulMin;
   float BlockSize = (float)tmpMax / (float)(rectl.xRight-1);
   ULONG sBarWidth = (short)iround(tmpCurrent / BlockSize);

   if(sBarWidth > 2)
   {
      RECTL rectBar;

      memcpy(&rectBar, &rectl, sizeof(RECTL));

      rectBar.yTop    -= 1;
      rectBar.xRight   = sBarWidth;

      Draw3DFrame(hPS, &rectBar, pData->color.lBarBright, pData->color.lBarDark, FALSE);

      if(sBarWidth > 3)
      {
         rectBar.yBottom += 1;
         rectBar.xLeft   += 1;

         WinFillRect(hPS, &rectBar, pData->color.lBar);
         rectl.xLeft = rectBar.xRight+1;
      }
   }

   if(rectl.xRight-rectl.xLeft != 0)
      WinFillRect(hPS, &rectl, pData->color.lBackground);


   WinEndPaint(hPS);
}


void _System Draw3DFrame(HPS hPS, PRECTL pRectl, long lHi, long lLo, BOOL bLower)
{
   POINTL      ptl;
   LINEBUNDLE  lb;

   if(bLower) lb.lColor = lLo;
   else lb.lColor = lHi;
   GpiSetAttrs(hPS, PRIM_LINE, LBB_COLOR, 0, &lb);

   ptl.x = pRectl->xLeft;
   ptl.y = pRectl->yBottom;
   GpiMove(hPS, &ptl);

   ptl.y = pRectl->yTop;
   GpiLine(hPS, &ptl);
   ptl.x = pRectl->xRight;
   GpiLine(hPS, &ptl);


   if(bLower) lb.lColor = lHi;
   else lb.lColor = lLo;
   GpiSetAttrs(hPS, PRIM_LINE, LBB_COLOR, 0, &lb);

   ptl.x = pRectl->xRight;
   ptl.y = pRectl->yTop-1;
   GpiMove(hPS, &ptl);

   ptl.y = pRectl->yBottom;
   GpiLine(hPS, &ptl);
   ptl.x = pRectl->xLeft+1;
   GpiLine(hPS, &ptl);
}


long inline iround(float f)
{
   return f - (long)f < 0.5 ? (long)f : (long)f + 1;
}

void pbcQueryRange(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   PPROGRESSBAR  pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   *(PULONG)mp1 = pData->ulMin;
   *(PULONG)mp2 = pData->ulMax;
}

BOOL pbcSetRange(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   PPROGRESSBAR   pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   ULONG ulVal = 0;

   pData->ulMin = (ULONG)mp1;
   pData->ulMax = (ULONG)mp2;

   ulVal = WinQueryPBValue(hWnd);
   if(ulVal < (ULONG)mp1)
      WinSetPBValue(hWnd, (ULONG)mp1);
   else if(ulVal > (ULONG)mp2)
      WinSetPBValue(hWnd, (ULONG)mp2);

   WinInvalidateRect(hWnd, NULL, FALSE);

   return TRUE;
}


MRESULT pbcQueryValue(HWND hWnd)
{
   PPROGRESSBAR   pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   return (MRESULT)pData->ulCurrent;
}

BOOL pbcSetValue(HWND hWnd, MPARAM mp1)
{
   ULONG ulVal = (ULONG)mp1;
   PPROGRESSBAR   pData = (PPROGRESSBAR)WinQueryWindowPtr(hWnd, QWP_CTRLDATA);

   if(ulVal < pData->ulMin)
      ulVal = pData->ulMin;
   else if(ulVal > pData->ulMax)
      ulVal = pData->ulMax;

   pData->ulCurrent = ulVal;
   WinInvalidateRect(hWnd, NULL, FALSE);

   return TRUE;
}
