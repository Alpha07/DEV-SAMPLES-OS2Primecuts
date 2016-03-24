#define INCL_DOSMODULEMGR
#define INCL_DOSMEMMGR
#define INCL_DOSERRORS

#include <os2.h>

#include <stdio.h>

typedef long (APIENTRY _PFNFUNC)(char);
typedef _PFNFUNC *PFNFUNC;

int main(void)
{
   APIRET   rc = 0;
   char     ch = 5;
   long     l  = 0;
   HMODULE  hMod = NULLHANDLE;
   char     aszLoadError[256] = "";
   PFNFUNC  pfnFunc = NULL;

   /*
   ** Test.DLL
   */
   rc = DosLoadModule(aszLoadError, sizeof(aszLoadError), "Test", &hMod);
   if(rc != NO_ERROR)
   {
      printf("Error: DosLoadModule() returned %d.\n", rc);
   }

   if((rc = DosQueryProcAddr(hMod, 0L, "func", (PFN*)&pfnFunc)) != NO_ERROR)
   {
      printf("Error: DosQueryProcAddr() returned %d.\n", rc);
   }

   l = (pfnFunc)(ch);

   DosFreeModule(hMod);

   printf("%d\n", l);

   return 0;
}
