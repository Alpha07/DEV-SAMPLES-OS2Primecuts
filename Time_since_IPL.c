/*---------------------------------------------------------------------------*\
 *    Title: Time since initial program load                                 *
 * Filename: Time since IPL.c                                                *
 *     Date: 1999-04-16                                                      *
 *   Author: Jan M. Danielsson                                               *
 *                                                                           *
 *  Purpose:                                                                 *
 *                                                                           *
 * (C) Treadstone, 1999. All rights reserved.                                *
 *                                                                           *
 * ToDo:                                                                     *
 *   - Nothing, yet                                                          *
 *                                                                           *
 * Known bugs:                                                               *
 *   - Since the counter is a 32-bit millisecond counter, it will wrap after *
 *     4.2 billion milliseconds. It will take a couple of weeks for the      *
 *     counter to wrap.                                                      *
\*---------------------------------------------------------------------------*/
#define INCL_DOSMISC

#include <os2.h>

#include <stdio.h>


#define  SECOND   1000
#define  MINUTE   (SECOND * 60)
#define  HOUR     (MINUTE * 60)
#define  DAY      (HOUR * 24)


void main(void)
{
   APIRET   rc = NO_ERROR;
   ULONG    ulRunTime = 0;

   ULONG    ulMilliSec = 0;
   ULONG    ulSeconds = 0;
   ULONG    ulMinutes = 0;
   ULONG    ulHours = 0;
   ULONG    ulDays = 0;

   ULONG    ulTmp = 0;

   rc = DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &ulRunTime, sizeof(ULONG));

   ulDays    = ulRunTime / DAY;
   ulTmp     = ulRunTime % DAY;

   ulHours   = ulTmp     / HOUR;
   ulTmp     = ulTmp     % HOUR;

   ulMinutes = ulTmp     / MINUTE;
   ulTmp     = ulTmp     % MINUTE;

   ulSeconds = ulTmp     / SECOND;
   ulTmp     = ulTmp     % SECOND;

   ulMilliSec = ulTmp;

   printf("%d days, %d hours, %d minutes, %d seconds and %d milliseconds\n", ulDays, ulHours, ulMinutes, ulSeconds, ulMilliSec);
}
