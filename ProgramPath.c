#pragma strings(readonly)

#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSERRORS

#include <os2.h>

#include <stdio.h>


BOOL _Inline get_process_modulename(ULONG cbName, PCH pch);


int main(int argc, char *argv[])
{
   char achTmp[CCHMAXPATHCOMP] = "";

   printf("argv[0] = %s\n", argv[0]);

   if(get_process_modulename(sizeof(achTmp), achTmp))
   {
      char *p = achTmp;

      printf("module pathname: %s\n", achTmp);

      /*
       * Quick and dirty removal of the filename
       */
      while(*p)
      {
         p++;
      }

      while(*p != '\\')
      {
         p--;
      }

      p+=1;
      *p = '\0';

      printf("program path: %s\n", achTmp);

   }
   else
   {
      puts("Error: Couldn't get module information");
   }


   return 0;
}

BOOL _Inline get_process_modulename(ULONG cbName, PCH pch)
{
   BOOL fSuccess = FALSE;
   APIRET rc = NO_ERROR;
   PTIB ptib = NULL;
   PPIB ppib = NULL;

   /*
    * Get the pointers to the process and thread info blocks.
    */
   rc = DosGetInfoBlocks(&ptib, &ppib);
   if(rc == NO_ERROR)
   {
      /*
       * The process info block contains the module handle,
       * use it to get the path of the module.
       */
      rc = DosQueryModuleName(ppib->pib_hmte, cbName, pch);
      if(rc == NO_ERROR)
      {
         fSuccess = TRUE;
      }
   }
   return fSuccess;
}
