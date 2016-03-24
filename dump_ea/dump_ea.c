#pragma strings(readonly)

#define INCL_DOSFILEMGR
#define INCL_DOSERRORS

#include <os2.h>

#include <stdio.h>
#include <malloc.h>
#include <memory.h>
#include <string.h>


void _Optlink dump_ea(PSZ pszFile, PSZ pszEAFile, char **apszNames, ULONG ulNames);

ULONG _Inline queryEASize(PSZ pszFile);

USHORT GetFEA2Size(PFEA2LIST pFEA2List);

/*
 * argv[1] datafile (input)
 * argv[2] ea buffer file (output)
 * argv[>=3] EA name to filter out
 */
int main(int argc, char *argv[])
{
   if(argc > 2)
   {
      int i;
      char **filter = NULL;

      if(argc > 3)
      {
         filter = calloc(argc-3, sizeof(char*));
         if(filter)
         {
            for(i = 3; i < argc; i++)
            {
               int len = strlen(argv[i])+1;
               filter[i-3] = malloc(len);
               memcpy(filter[i-3], argv[i], len);
            }
         }
      }
      dump_ea(argv[1], argv[2], filter, argc-3);

      free(filter);
   }

   return 0;
}



void _Optlink dump_ea(PSZ pszFile, PSZ pszEAFile, char **apszNames, ULONG ulNames)
{
   APIRET rc = NO_ERROR;
   ULONG cbList = 0;
   EAOP2 eaop2 = { NULL, NULL, 0UL };
   ULONG cbEABuf = 0;

   /*
    * Get a buffer size which will be used to ensure that no EA:s are lost.
    */
   cbList = queryEASize(pszFile);
   if(cbList == 0)
   {
      return;
   }

   eaop2.fpFEA2List = malloc(cbList);
   if(eaop2.fpFEA2List)
   {
      ULONG ulCount = (ULONG)-1;

      /*
       * Get list of EA names. This does not get a real FEA2 list, it gets something called a
       * DENA2, which should be used for two things only: 1) Get EA names, 2) Get EA sizes.
       */
      rc = DosEnumAttribute(ENUMEA_REFTYPE_PATH, pszFile, 1UL, &eaop2.fpFEA2List->list[0], cbList, &ulCount, ENUMEA_LEVEL_NO_VALUE);
      if(rc == NO_ERROR)
      {
         FEA2 *pfea2 = (FEA2*)&eaop2.fpFEA2List->list[0];
         PBYTE p = NULL;
         ULONG i = 0;
         GEA2 *pGEA2Cursor = NULL;       /* Pointer used while adding GEA2 entries */

         /*
          * Init and set up Get EA list
          */
         eaop2.fpGEA2List = malloc(cbList);
         memset(eaop2.fpGEA2List, 0, cbList);  /* Important! */
         eaop2.fpGEA2List->cbList = sizeof(eaop2.fpGEA2List->cbList);

         /*
          * Set up GEA2 cursor for adding GEA2 entries
          */
         pGEA2Cursor = &eaop2.fpGEA2List->list[0];

         /*
          * Loop through all EA names fetched with DosEnumAttribute() and add them
          * to the GEA2 list, unless they are in the filter-list ofcourse.
          */
         for(i = 0; i < ulCount; i++)
         {
            ULONG iFilter = 0;
            BOOL fFilter = FALSE;        /* Don't filter by default */

            /*
             * Ugh, slow search..
             */
            for(iFilter = 0; iFilter < ulNames; iFilter++)
            {
               if(strcmp(pfea2->szName, apszNames[iFilter]) == 0)
               {
                  /*
                   * EA name is in filter-list, set filter boolean to true, and
                   * break out of loop.
                   */
                  fFilter = TRUE;
                  break;
               }
            }

            if(!fFilter)
            {
               /*
                * EA name is not in the filter-list, add it to the Get EA list.
                */
               if(pGEA2Cursor->cbName)
               {
                  PBYTE pTmp = (PBYTE)pGEA2Cursor;

                  /*
                   * Note; the GEA2 entries *must* be 4-byte (double word) aligned
                   */
                  pGEA2Cursor->oNextEntryOffset = sizeof(GEA2)+pfea2->cbName;
                  pGEA2Cursor->oNextEntryOffset += (4-(pGEA2Cursor->oNextEntryOffset%4));
                  pTmp += pGEA2Cursor->oNextEntryOffset;
                  pGEA2Cursor = (GEA2*)pTmp;
               }
               pGEA2Cursor->oNextEntryOffset = 0;
               pGEA2Cursor->cbName = pfea2->cbName;
               memcpy(pGEA2Cursor->szName, pfea2->szName, pfea2->cbName+1);

               /*
                * Calculate EA buffer size (used for storing EA buffer).
                * The EA buffer file EAUTIL uses does not include the
                * oNextENtryOffset parameter, so don't include it here.
                */
               if(cbEABuf == 0)
               {
                  /* Intially add size long word */
                  cbEABuf += sizeof(eaop2.fpFEA2List->cbList);
               }
               cbEABuf += (sizeof(pfea2->fEA) +
                          sizeof(pfea2->cbName) +
                          sizeof(pfea2->cbValue) +
                          (pfea2->cbName+1) +
                          pfea2->cbValue);

               eaop2.fpGEA2List->cbList += (sizeof(GEA2)+pGEA2Cursor->cbName);
               if(eaop2.fpGEA2List->cbList % 4)
               {
                  eaop2.fpGEA2List->cbList += (4-(eaop2.fpGEA2List->cbList%4));
               }
            }

            p = pfea2->oNextEntryOffset + (PBYTE)pfea2;
            pfea2 = (FEA2*)p;
         }

         /*
          * Check if calculated EA buffer size. If it is non-zero, one or more GEA2 entries
          * should be read.
          */
         if(cbEABuf)
         {
            /*
             * Reset the eaop2.fpFEA2List->cbList value so the EA API knows it has enough
             * memory for the FEA2 list.
             */
            eaop2.fpFEA2List->cbList = cbList;

            /*
             * Get the FEA2 list from the GEA2 list.
             */
            rc = DosQueryPathInfo(pszFile, FIL_QUERYEASFROMLIST, &eaop2, sizeof(EAOP2));
            if(rc == NO_ERROR)
            {
               HFILE hFile = NULLHANDLE;
               ULONG ulAction = 0;
               const ULONG fsOpenFlags = OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS;
               const ULONG fsOpenMode = OPEN_FLAGS_SEQUENTIAL | OPEN_SHARE_DENYWRITE | OPEN_ACCESS_WRITEONLY;

               rc = DosOpen(pszEAFile, &hFile, &ulAction, 0UL, FILE_NORMAL, fsOpenFlags, fsOpenMode, NULL);
               if(rc == NO_ERROR)
               {
                  ULONG cbWritten = 0;
                  ULONG ibActual = 0;

                  /*
                   * The cbEABuf should by now contain the size of the EA buffer.
                   */
                  eaop2.fpFEA2List->cbList = cbEABuf;
                  rc = DosWrite(hFile, &eaop2.fpFEA2List->cbList, sizeof(eaop2.fpFEA2List->cbList), &cbWritten);

                  pfea2 = &eaop2.fpFEA2List->list[0];

                  while(1)
                  {
                     PBYTE pWriteBuf = (PBYTE)pfea2 + sizeof(pfea2->oNextEntryOffset);   /* Skip the oNextEntryOffset long word */

                     /*
                      * Write the FEA2 structure, but skip oNextEntryOffset
                      */
                     rc = DosWrite(hFile, pWriteBuf, sizeof(FEA2)-sizeof(pfea2->oNextEntryOffset)+pfea2->cbName+pfea2->cbValue, &cbWritten);

                     if(pfea2->oNextEntryOffset == 0)
                     {
                        /*
                         * Break out of loop if this entry is the last node
                         */
                        break;
                     }

                     /* Next FEA2 entry */
                     p = pfea2->oNextEntryOffset + (PBYTE)pfea2;
                     pfea2 = (FEA2*)p;
                  }
                  rc = DosClose(hFile);
               }
            }

         }
         free(eaop2.fpFEA2List);
      }
   }
   free(eaop2.fpGEA2List);
}



/*
 * Since the EA buffer can take more space in memory than on disk
 * the OS/2 API provides a method of calculating a buffer size which
 * will guarantee to fit the EA buffer from a specified file.
 */
ULONG _Inline queryEASize(PSZ pszFile)
{
   APIRET rc = NO_ERROR;
   FILESTATUS4 fstat4 = { 0 };

   rc = DosQueryPathInfo(pszFile, FIL_QUERYEASIZE, &fstat4, sizeof(fstat4));
   if(rc != NO_ERROR)
   {
      fstat4.cbList = 0UL;
   }
   return fstat4.cbList;
}
