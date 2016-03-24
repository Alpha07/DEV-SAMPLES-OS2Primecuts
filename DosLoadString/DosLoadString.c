/*---------------------------------------------------------------------------*\
 *    Title: DosLoadString                                                   *
 * Filename: DosLoadString.c                                                 *
 *     Date: 1999-01-20                                                      *
 *   Author: Jan M. Danielsson (jan.m.danielsson@telia.com)                  *
 *                                                                           *
 *  Purpose: Load a resource string using the CP API.                        *
 *                                                                           *
 * ToDo:                                                                     *
 *   - In an application, the stringbundles should be cached.                *
 *     DosFreeResource() should not be called for every time a single string *
 *     is loaded.                                                            *
 *   - Errorchecking in DosLoadString(). DosFreeResource() returnvalue is    *
 *     is not handled.                                                       *
 *                                                                           *
 * Known bugs:                                                               *
 *   - None, yet                                                             *
\*---------------------------------------------------------------------------*/
#define INCL_DOSRESOURCES                /* DosGetResource()        */
#define INCL_DOSMEMMGR                   /* DosAllocMem()           */
#define INCL_DOSERRORS                   /* Error-value definitions */

#include <os2.h>

#include <stdio.h>
#include <string.h>

#include "res.h"


/*
 * Function prototypes
 */
APIRET APIENTRY DosLoadString(HMODULE hMod, ULONG ulID, PSZ pszBuf, ULONG cbBuffer);


int main(void)
{
   APIRET   rc = 0;
   char     pszString[64] = "";

   if((rc = DosLoadString((HMODULE)0, IDS_APPNAME, pszString, sizeof(pszString))) != NO_ERROR)
   {
      printf("Error: DosLoadString()/DosGetResource() returned: %d\n", rc);
      return 1;
   }

   printf("String ID %d is '%s'.\n", IDS_APPNAME, pszString);

   return 0;
}


APIRET APIENTRY DosLoadString(HMODULE hMod, ULONG ulID, PSZ pszBuf, ULONG cbBuffer)
{
   APIRET   rc = 0;
   PSZ      pszTmp = NULL;
   PSZ      p = NULL;                    /* Temporary pointer used when parsing string */
   long     lCount = 0;
   long     i = 0;
   long     lTmp = 0;

   /*
    * When retriving strings with DosGetResource(), it actually loads a
    * bundle of strings (up to 16 strings in each). Strings are
    * stored in bundles of 16 in the resource file.
    */
   if((rc = DosGetResource(hMod, RT_STRING, (ulID/16)+1, (PPVOID)&pszTmp)) != 0)
   {
      return rc;
   }

   p = pszTmp;
   p += 2;        /* Ignore the stringtable codepage in this demonstration */

   /*
    * Set the pointer to the location of the string we want to retrive
    * in the bunble we got with DosGetResource().
    */
   lCount = ulID % 16;
   while(i < lCount)
   {
      lTmp = (BYTE)p[0];
      p += (lTmp+1);
      i++;
   }
   p++;

   /* Copy the resourcestring outputbuffer */
   strncpy(pszBuf, (const char *)p, cbBuffer);

   DosFreeResource((PVOID)pszTmp);

   return NO_ERROR;
}
