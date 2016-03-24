#pragma strings(readonly)

#define INCL_DOSFILEMGR
#define INCL_DOSERRORS
#define INCL_GPIBITMAPS

#include <os2.h>


#include <malloc.h>
#include <memory.h>

#include "PixelBuffer.h"



PPIXELBUFFER _Optlink LoadPixelBuffer(PSZ pszFile)
{
   APIRET rc = NO_ERROR;
   const ULONG fsOpenFlags = OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS;
   const ULONG fsOpenMode = OPEN_SHARE_DENYWRITE | OPEN_ACCESS_READONLY;
   PVOID pAlloc = NULL;
   PPIXELBUFFER pb = NULL;
   HFILE hFile = NULLHANDLE;
   ULONG ulAction = 0;

   /*
    * Open bitmap file
    */
   if((rc = DosOpen(pszFile, &hFile, &ulAction, 0UL, FILE_NORMAL, fsOpenFlags, fsOpenMode, NULL)) == NO_ERROR)
   {
      FILESTATUS3 fstatus = { 0 };

      /*
       * Query bitmap file size
       */
      if((rc = DosQueryFileInfo(hFile, FIL_STANDARD, &fstatus, sizeof(fstatus))) == NO_ERROR)
      {
         PBYTE pBitmapFile = NULL;

         /*
          * Allocate buffer to hold file data
          */
         if((pBitmapFile = malloc(fstatus.cbFile)) != NULL)
         {
            ULONG cbRead = 0;

            /*
             * Read bitmap file
             */
            if((rc = DosRead(hFile, pBitmapFile, fstatus.cbFile, &cbRead)) == NO_ERROR)
            {
               BITMAPFILEHEADER2 *pbfh2 = NULL;
               BITMAPINFOHEADER2 *pbmp2 = NULL;

               pbfh2 = (BITMAPFILEHEADER2*)pBitmapFile;

               /*
                * Parse bitmap type from file header
                */
               switch(pbfh2->usType)
               {
                  case BFT_BITMAPARRAY:
                     pbfh2 = &(((BITMAPARRAYFILEHEADER2*)pBitmapFile)->bfh2);
                     pbmp2 = &pbfh2->bmp2;
                     break;

                  case BFT_BMAP:
                     pbmp2 = &pbfh2->bmp2;
                     break;

                  default:
                     case BFT_ICON:
                     case BFT_POINTER:
                     case BFT_COLORICON:
                     case BFT_COLORPOINTER:
                     break;
               }
               if(pbmp2)
               {
                  PBYTE pBitmap = NULL;

                  pb = malloc(sizeof(PIXELBUFFER));
                  if(pb)
                  {
                     memset(pb, 0, sizeof(PIXELBUFFER));

                     pb->bmi2.cbFix = 16;
                     pb->cx = pb->bmi2.cx = pbmp2->cx;
                     pb->cy = pb->bmi2.cy = pbmp2->cy;
                     pb->cBitCount = pbmp2->cBitCount;
                     pb->bmi2.cPlanes = pbmp2->cPlanes;
                     pb->bmi2.cBitCount = pbmp2->cBitCount;

                     pb->clip.xLeft = pb->clip.yBottom = 0;
                     pb->clip.xRight = pb->cx;
                     pb->clip.yTop = pb->cy;

                     /*
                      * Allocate bufffer to hold bitmap
                      */
                     pb->cbBitmap = (((pb->bmi2.cBitCount * pb->bmi2.cx) + 31) / 32) * 4 * pb->bmi2.cy * pb->bmi2.cPlanes;
                     if((pb->data = malloc(pb->cbBitmap)) != NULL)
                     {
                        memcpy(pb->data, pBitmapFile + pbfh2->offBits, pb->cbBitmap);
                     }
                     pb->cbLine = (((pb->bmi2.cBitCount * pb->bmi2.cx) + 31) / 32) * 4 * pb->bmi2.cPlanes;
                  }
               }
            }
            free(pBitmapFile);
            _heapmin();
         }
      }
      DosClose(hFile);
   }

   return pb;
}

void _Optlink FreePixelBuffer(PPIXELBUFFER pb)
{
   free(pb->data);
   free(pb);
   _heapmin();
}
