/*---------------------------------------------------------------------------*\
 *    Title: Kill                                                            *
 * Filename: kill.c                                                          *
 *     Date: 1999-04-16                                                      *
 *   Author: Jan M. Danielsson                                               *
 *                                                                           *
 *  Purpose:                                                                 *
 *                                                                           *
 * ToDo:                                                                     *
 *   - Read PID from stdin.                                                  *
 *                                                                           *
 * Known bugs:                                                               *
 *   - None, yet                                                             *
\*---------------------------------------------------------------------------*/
#pragma strings(readonly)

#define INCL_DOSPROCESS

#include <os2.h>

#include <stdio.h>
#include <ctype.h>


unsigned long hex2long(char *str);


int main(int argc, char *argv[])
{
   APIRET   rc;
   short    i = 1;
   PID      pid;

   if(argc < 2)
   {
      puts("Usage: Kill <pid> [pid] ...");
      puts("Note: The 'pid' must be specified in hex (same as pstat uses).");
      return 1;
   }

   while(i < argc)
   {
      pid = hex2long(argv[i]);
      if((rc = DosKillProcess(1, pid)) != 0)
      {
         printf("Error: DosKillProcess() returned %u\n", rc);
      }
      else
      {
         printf("Process %04x killed successfully.\n", pid);
      }
      i++;
   }

   return 0;
}

unsigned long hex2long(char *str)
{
   unsigned long i = 0;
   unsigned long j = 0;

   while(*str && isxdigit(*str))
   {
      i = *str++ - '0';

      if(9 < i)
      {
         i -= 7;
      }
      j <<= 4;
      j |= (i & 0x0f);
    }
    return j;
}
