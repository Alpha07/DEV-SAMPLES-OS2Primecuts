/*
 * Illustrate the usage of shared memory.
 * The allocating process does not have to be the one to free the memeory
 * object.
 */
#pragma strings(readonly)

#define INCL_DOSMEMMGR
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#define INCL_DOSERRORS
#define INCL_KBD

#include <os2.h>

#include <stdio.h>


int main(void)
{
   APIRET rc = NO_ERROR;
   PBYTE pbBuf = NULL;
   ULONG ulPid = 0;
   PTIB ptib = NULL;
   PPIB ppib = NULL;
   UCHAR pchMemName[] = "\\SHAREMEM\\Primecuts\\SharedMemTest";
   HMTX hmtxLock = NULLHANDLE;
   UCHAR pchMtxName[] = "\\SEM32\\Primecuts\\SharedMemTest";
   KBDKEYINFO keyinfo = { 0 };

   /*
    * Attempt to create mutual exclusion sempahore to serialize access to the
    * access counter.
    */
   rc = DosCreateMutexSem(pchMtxName, &hmtxLock, 0UL, FALSE);
   if(rc == ERROR_DUPLICATE_NAME)
   {
      /*
       * The semaphore already exists in system, attempt to open it instead.
       */
      rc = DosOpenMutexSem(pchMtxName, &hmtxLock);
   }

   if(rc != NO_ERROR)
   {
      puts("Couldn't create or open mutex semaphore.");
      return 1;
   }

   rc = DosGetInfoBlocks(&ptib, &ppib);
   printf("PID: %04x\n", ppib->pib_ulpid);

   /*
    * Allocate named memory block
    */
   rc = DosAllocSharedMem((PPVOID)&pbBuf, pchMemName, 1024, PAG_READ | PAG_WRITE | PAG_COMMIT);
   if(rc == ERROR_ALREADY_EXISTS)
   {
      rc = DosGetNamedSharedMem((PPVOID)&pbBuf, pchMemName, PAG_READ | PAG_WRITE);
      if(rc != NO_ERROR)
      {
         printf("Error: DosGetNamedSharedMem() returned: %d\n", rc);
      }
      else
      {
         printf("Got Address: %08x\n", pbBuf);
      }
   }
   else
   {
      printf("Address of new memoryblock: %08x\n", pbBuf);
      *pbBuf = 0;
   }

   /*
    * Increase access count
    */
   rc = DosRequestMutexSem(hmtxLock, SEM_INDEFINITE_WAIT);
   if(rc == NO_ERROR)
   {
      (*pbBuf)++;

      printf("%d\n", *pbBuf);

      rc = DosReleaseMutexSem(hmtxLock);
   }

   /*
    * Wait for a key press
    */
   KbdCharIn(&keyinfo, IO_WAIT, (HKBD)0);

   /*
    * Decrease access count
    */
   rc = DosRequestMutexSem(hmtxLock, SEM_INDEFINITE_WAIT);
   if(rc == NO_ERROR)
   {
      (*pbBuf)--;

      rc = DosReleaseMutexSem(hmtxLock);
   }

   /*
    * When the access count is zero, free the memory object.
    */
   if(*pbBuf == 0)
   {
      rc = DosFreeMem(pbBuf);
      if(rc == NO_ERROR)
      {
         printf("Memory block Freed.\n");
      }
   }

   /*
    * Close mutex semaphore
    */
   rc = DosCloseMutexSem(hmtxLock);

   return 0;
}
