#pragma strings(readonly)

#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSDATETIME
#define INCL_DOSERRORS

#include <os2.h>

#include <malloc.h>
#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <process.h>

#include "bloatcom.h"



#define EHB_IGNORE                       0x00
#define EHB_DISABLE                      0x08
#define EHB_ENABLE                       0x10
#define EHB_AUTO                         0x18
#define EHB_MASK                         0x18

/* Receive Trigger Levels */
#define EHB_RTL_1                        0x00
#define EHB_RTL_4                        0x20
#define EHB_RTL_8                        0x40
#define EHB_RTL_14                       0x60

/* Transmit Buffer Load Counts */
#define EHB_TBLC_1                       0x00
#define EHB_TBLC_16                      0x80




#define EVENT_DATAAVAILABLE              1
#define EVENT_TIMEOUT                    2
#define EVENT_CARRIER_LOST               3


#pragma pack(1)
typedef struct _eqrd
{
  ULONG ulBitRate;
  BYTE  bFraction;
  ULONG ulMinBitRate;
  BYTE  bMinFraction;
  ULONG ulMaxBitRate;
  BYTE  bMaxFraction;
}EXTQUERYRATEDATA;
#pragma pack()




typedef struct _MODULOBUFFER
{
   HMTX hmtxLock;                        /* Lock buffer access */
   HEV hevDataAvailable;                 /* Posted when data is available in the data area */
   HEV hevBufferEmpty;                   /* Posted when there is no data available in the data area */
   ULONG iRead;                          /* Read position */
   ULONG iWrite;                         /* Write position */
   ULONG cbBuffer;                       /* Size of data buffer */
   ULONG cbUsed;                         /* Bytes used in buffer */
   ULONG cbFree;                         /* Bytes free in buffer */
   BYTE data[1];                         /* Data area */
}MODULOBUFFER, *PMODULOBUFFER;

typedef struct _THREADPAUSE
{
   HEV hevPause;
   HEV hevPaused;
   HEV hevResume;
}THREADPAUSE, *PTHREADPAUSE;


typedef struct _COMPORT
{
   HFILE hCom;
   ULONG cInstances;
   ULONG bps;
   APIRET rc;
   PMODULOBUFFER inbuf;                  /* Receive buffer */
   PMODULOBUFFER outbuf;                 /* Transmitbuffer */
   TID tidReceive;
   TID tidTransmit;
   #ifdef __TOOLKIT45__
   PVOID pThreadStacks;
   #endif
   HEV hevTerminate;
   HEV hevCarrierLost;                   /* Event sempahore indicating carrier lost */
   PTHREADPAUSE pauseReceive;
   PTHREADPAUSE pauseTransmit;
   BOOL fCheckCarrier;
}COMPORT;


typedef struct _TIMER
{
   HEV hEvent;
   HTIMER hTimer;
}TIMER;


/*
 * Internal functions
 */
#ifdef __TOOLKIT45__
static void _System receive_thread(ULONG param);
static void _System transmit_thread(ULONG param);
#else
static void _Optlink receive_thread(void* param);
static void _Optlink transmit_thread(void* param);
#endif


static BOOL _Optlink init_port(HFILE hCom);


/*
 * Modulo buffer functions
 */
static PMODULOBUFFER _Optlink init_modulo_buffer(ULONG cbBuffer);
static void _Optlink term_modulo_buffer(PMODULOBUFFER modbuf);
BOOL _Optlink open_modulo_buffer(PMODULOBUFFER modbuf);
BOOL _Optlink close_modulo_buffer(PMODULOBUFFER modbuf);
void _Inline reset_modulo_buffer(PMODULOBUFFER modbuf);

ULONG _Inline read_modulo_buffer(PMODULOBUFFER modbuf, PBYTE p, ULONG cb);
ULONG _Inline peek_modulo_buffer(PMODULOBUFFER modbuf, PBYTE p, ULONG cb);
ULONG _Inline write_modulo_buffer(PMODULOBUFFER modbuf, PBYTE p, ULONG cb);


PTHREADPAUSE _Optlink init_threadpause(void);
BOOL _Optlink term_threadpause(PTHREADPAUSE ptp);
BOOL _Inline pause_thread(PTHREADPAUSE ptp);
BOOL _Inline resume_thread(PTHREADPAUSE ptp);
void _Inline check_thread_pause(PTHREADPAUSE ptp);




PCOMPORT _Optlink comOpen(PSZ pszComName, ULONG cbInBuffer, ULONG cbOutBuffer)
{
   APIRET rc = NO_ERROR;
   PCOMPORT cp = NULL;
   HFILE hCom = NULLHANDLE;
   ULONG ulAction = 0;

   /* Open port */
   if((rc = DosOpen(pszComName, &hCom, &ulAction, 0L, FILE_NORMAL, OPEN_ACTION_OPEN_IF_EXISTS, OPEN_ACCESS_READWRITE | OPEN_SHARE_DENYNONE, 0L)) == NO_ERROR)
   {
      char achMemName[64] = "";
      PVOID pAlloc = NULL;

      #ifdef DEBUG_TERM
      printf("i hCom = %08x\n", hCom);
      #endif

      /* Allocate core structure */
      sprintf(achMemName, "\\SHAREMEM\\BloatCom.%08x", hCom);
      puts(achMemName);
      if((rc = DosAllocSharedMem(&pAlloc, achMemName, sizeof(COMPORT), PAG_READ | PAG_WRITE | PAG_COMMIT)) == NO_ERROR)
      {
         cp = (PCOMPORT)pAlloc;
         memset(cp, 0, sizeof(COMPORT));
         cp->hCom = hCom;
      }
      else
      {
         #ifdef DEBUG_TERM
         puts("e Couldn't allocate memory for COMPORT structure");
         printf("%d\n", rc);
         #endif
         DosClose(hCom);
      }
   }
   else
   {
      #ifdef DEBUG_TERM
      puts("e Couldn't open comport");
      #endif
   }

   if(cp)
   {
      THREADCREATE tc = { 0 };
      ULONG cbAlloc = 0;

      if(cbInBuffer == 0)
      {
         RXQUEUE rxq = { 0 };
         ULONG cbPinout = 0;
         ULONG cbDinout = sizeof(rxq);

         if((rc = DosDevIOCtl(cp->hCom, IOCTL_ASYNC, ASYNC_GETINQUECOUNT, NULL, 0, &cbPinout, &rxq, sizeof(RXQUEUE), &cbDinout)) == NO_ERROR)
         {
            cbInBuffer = rxq.cb;
            #ifdef DEBUG_TERM
            printf("> Receive buffer set to system default size (%u bytes).\n", cbInBuffer);
            #endif
         }
         else
         {
            #ifdef DEBUG_TERM
            puts("e Could not query system inqueue size");
            #endif
         }
      }

      if(cbOutBuffer == 0)
      {
         RXQUEUE rxq = { 0 };
         ULONG cbPinout = 0;
         ULONG cbDinout = sizeof(rxq);

         if((rc = DosDevIOCtl(cp->hCom, IOCTL_ASYNC, ASYNC_GETOUTQUECOUNT, NULL, 0, &cbPinout, &rxq, sizeof(RXQUEUE), &cbDinout)) == NO_ERROR)
         {
            cbOutBuffer = rxq.cb;
            #ifdef DEBUG_TERM
            printf("> Transmit buffer set to system default size (%u bytes).\n", cbOutBuffer);
            #endif
         }
         else
         {
            #ifdef DEBUG_TERM
            puts("e Could not query system outqueue size");
            #endif
         }
      }

      /* 115200 BPS */
      comSetBaudrate(cp, 115200UL);

      /* 8, n, 1 */
      comSetLineControl(cp, 8, 0, 0);

      init_port(cp->hCom);

      /* Initialize receive buffer */
      cp->inbuf = init_modulo_buffer(cbInBuffer);
      cp->pauseReceive = init_threadpause();

      /* Initialize transmit buffer */
      cp->outbuf = init_modulo_buffer(cbOutBuffer);
      cp->pauseTransmit = init_threadpause();

      rc = DosCreateEventSem(NULL, &cp->hevCarrierLost, 0UL, FALSE);

      rc = DosCreateEventSem(NULL, &cp->hevTerminate, 0UL, FALSE);

      #ifdef __TOOLKIT45__
      cp->pThreadStacks = malloc(8192);

      /* Start receive thread */
      memset(&tc, 0, sizeof(tc));
      tc.cbSize = sizeof(THREADCREATE);
      tc.pfnStart = receive_thread;
      tc.lParam = (ULONG)cp;
      tc.lFlag = CREATE_READY | STACK_SPARSE;
      tc.cbStack = 4096;
      tc.pStack = (PBYTE)cp->pThreadStacks+4096;
      if((rc = DosCreateThread2(&tc)) == NO_ERROR)
      {
         cp->tidReceive = (TID)tc.pTid;
      }

      /* Start transmit thread */
      memset(&tc, 0, sizeof(tc));
      tc.cbSize = sizeof(THREADCREATE);
      tc.pfnStart = transmit_thread;
      tc.lParam = (ULONG)cp;
      tc.lFlag = CREATE_READY | STACK_SPARSE;
      tc.cbStack = 4096;
      tc.pStack = (PBYTE)cp->pThreadStacks+8192;
      if((rc = DosCreateThread2(&tc)) == NO_ERROR)
      {
         cp->tidTransmit = (TID)tc.pTid;
      }
      #else
      cp->tidReceive = _beginthread(receive_thread, NULL, 16384, (void*)cp);
      cp->tidTransmit = _beginthread(transmit_thread, NULL, 16384, (void*)cp);
      #endif
   }
   return cp;
}

BOOL _Optlink comClose(PCOMPORT cp)
{
   APIRET rc = NO_ERROR;

   rc = DosPostEventSem(cp->hevTerminate);

   /* Wait for both com-threads to terminate */
   DosWaitThread(&cp->tidReceive, DCWW_WAIT);
   DosWaitThread(&cp->tidTransmit, DCWW_WAIT);

   puts("> Threads should be dead by now");

   DosCloseEventSem(cp->hevTerminate);

   DosCloseEventSem(cp->hevCarrierLost);

   /* Release receive thread's resources */
   term_modulo_buffer(cp->inbuf);
   term_threadpause(cp->pauseReceive);

   /* Release transmitter thread's resources */
   term_modulo_buffer(cp->outbuf);
   term_threadpause(cp->pauseTransmit);

   #ifdef __TOOLKIT45__
   /* Release thread stack memory */
   free(cp->pThreadStacks);
   #endif

   /* Close communications port */
   DosClose(cp->hCom);

   DosFreeMem(cp);

   _heapmin();

   return TRUE;
}

/*
 * Warning -- not finished.
 */
PCOMPORT _Optlink comAquireFromHandle(HFILE hCom)
{
   PCOMPORT cp = NULL;
   APIRET rc = NO_ERROR;
   PVOID pBuffer = NULL;
   char achMemName[64] = "";

   sprintf(achMemName, "\\SHAREMEM\\BloatCom.%08x", hCom);
   if((rc = DosGetNamedSharedMem(&pBuffer, achMemName, PAG_READ | PAG_WRITE)) == NO_ERROR)
   {
      cp = (PCOMPORT)pBuffer;
   }

   if(cp)
   {
      open_modulo_buffer(cp->inbuf);
      open_modulo_buffer(cp->outbuf);

      cp->cInstances++;                  /* Needs to be protected */
   }

   /*
    * To Do: Add comReleaseAccess to an exitlist so access will be released at process termination
    */

   return cp;
}

/*
 * Warning -- not finished.
 */
void _Optlink comDeaquireAccess(PCOMPORT cp)
{
   cp->cInstances--;                     /* Needs to be protected */

   close_modulo_buffer(cp->outbuf);
   close_modulo_buffer(cp->inbuf);

   DosFreeMem(cp);
}

BOOL _Optlink comSetBaudrate(PCOMPORT cp, ULONG bps)
{
   BOOL fSuccess = FALSE;
   ULONG cbPinout = 0;
   ULONG cbDinout = 0;

   if(bps <= 0x0000ffff)
   {
      USHORT usRate = (USHORT)bps;

      cbPinout = sizeof(usRate);
      cbDinout = 0;

      cp->rc = DosDevIOCtl(cp->hCom, IOCTL_ASYNC, ASYNC_SETBAUDRATE, &usRate, sizeof(usRate), &cbPinout, NULL, 0L, &cbDinout);
   }
   else
   {
      typedef struct _esrp
      {
         ULONG ulBitRate;
         BYTE  bFraction;
      }EXTSETRATEPARMS;
      EXTSETRATEPARMS esr = { 0, 0 };

      cbPinout = sizeof(esr);
      cbDinout = 0;

      esr.ulBitRate = bps;
      esr.bFraction = 0;

      cp->rc = DosDevIOCtl(cp->hCom, IOCTL_ASYNC, ASYNC_EXTSETBAUDRATE, &esr, sizeof(esr), &cbPinout, NULL, 0, &cbDinout);
   }

   if(cp->rc == NO_ERROR)
   {
      fSuccess = TRUE;
   }

   return fSuccess;
}


BOOL _Optlink comSetLineControl(PCOMPORT cp, BYTE bDataBits, BYTE bParity, BYTE bStopBits)
{
   LINECONTROL lc = { 0 };
   BOOL fSuccess = FALSE;

   lc.bDataBits = bDataBits; /* 8 */
   lc.bParity = bParity;     /* N */
   lc.bStopBits = bStopBits; /* 1 */

   if((cp->rc = DosDevIOCtl(cp->hCom, IOCTL_ASYNC, ASYNC_SETLINECTRL, &lc, sizeof(lc), NULL, NULL, 0L, NULL)) == NO_ERROR)
   {
      fSuccess = TRUE;
   }
   return fSuccess;
}


static BOOL _Optlink init_port(HFILE hCom)
{
   BOOL fSuccess = FALSE;
   APIRET rc = NO_ERROR;
   DCBINFO dcb = { 0 };
   ULONG cbPinout = 0;
   ULONG cbDinout = 0;
   BOOL fCtsRts = TRUE;
   BOOL fXonXoff = FALSE;
   BOOL fEHB = TRUE;

   cbPinout = 0;
   cbDinout = sizeof(DCBINFO);
   if((rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_GETDCBINFO, NULL, 0, &cbPinout, &dcb, sizeof(DCBINFO), &cbDinout)) == NO_ERROR)
   {
      dcb.fbTimeout &= ~MODE_NO_WRITE_TIMEOUT;
      dcb.fbTimeout &= ~(MODE_READ_TIMEOUT | MODE_NOWAIT_READ_TIMEOUT);
      dcb.fbTimeout |= MODE_WAIT_READ_TIMEOUT;

      /* dcb.usReadTimeout  = 100 * 5; */
      dcb.usReadTimeout  = 10;           /* .1 second timeout */
      dcb.usWriteTimeout = 100 * 10;     /* ten second write timeout */

      dcb.fbCtlHndShake |= MODE_DTR_CONTROL;

      if(fCtsRts)
      {
         /* Enable CTS/RTS */
         dcb.fbCtlHndShake |= MODE_CTS_HANDSHAKE;
         dcb.fbFlowReplace |= MODE_RTS_HANDSHAKE;
         dcb.fbFlowReplace &= ~MODE_RTS_CONTROL;
      }
      else
      {
         /* Disable CTS/RTS */
         dcb.fbCtlHndShake &= ~MODE_CTS_HANDSHAKE;
         dcb.fbFlowReplace &= ~MODE_RTS_HANDSHAKE;
      }

      if(fXonXoff)
      {
         dcb.fbFlowReplace |= (MODE_AUTO_TRANSMIT | MODE_AUTO_RECEIVE);
      }
      else
      {
         dcb.fbFlowReplace &= ~(MODE_AUTO_TRANSMIT | MODE_AUTO_RECEIVE);
      }

      if(fEHB)
      {
         dcb.fbTimeout &= ~(EHB_MASK | EHB_RTL_14 | EHB_TBLC_16);
         dcb.fbTimeout |= EHB_ENABLE | EHB_RTL_8 | EHB_TBLC_16;
      }

      if((rc = DosDevIOCtl(hCom, IOCTL_ASYNC, ASYNC_SETDCBINFO, &dcb, sizeof(DCBINFO), NULL, NULL, 0L, NULL)) == NO_ERROR)
      {
         fSuccess = TRUE;
      }
   }
   return fSuccess;
}



/*
 * Return CDC (CarrierDetect) flag as boolean
 */
BOOL _Optlink comCarrier(PCOMPORT cp)
{
   APIRET rc = NO_ERROR;
   ULONG cbPinout = 0;
   ULONG cbDinout = 0;
   UCHAR uchControl = 0;
   BOOL fCarrier = FALSE;

   cbDinout = sizeof(uchControl);
   if((cp->rc = DosDevIOCtl(cp->hCom, IOCTL_ASYNC, ASYNC_GETMODEMINPUT, NULL, 0, &cbPinout, &uchControl, sizeof(uchControl), &cbDinout)) == NO_ERROR)
   {
      if(uchControl & DCD_ON)
      {
         fCarrier = TRUE;
      }
   }
   return fCarrier;
}



BOOL _Optlink comLowerDTR(PCOMPORT cp)
{
   BOOL fSuccess = FALSE;
   APIRET rc = NO_ERROR;
   DCBINFO dcb = { 0 };
   ULONG cbPinout = 0;
   ULONG cbDinout = 0;

   cbPinout = 0;
   cbDinout = sizeof(DCBINFO);
   if((rc = DosDevIOCtl(cp->hCom, IOCTL_ASYNC, ASYNC_GETDCBINFO, NULL, 0, &cbPinout, &dcb, sizeof(DCBINFO), &cbDinout)) == NO_ERROR)
   {
      dcb.fbCtlHndShake &= ~MODE_DTR_CONTROL;
      if((rc = DosDevIOCtl(cp->hCom, IOCTL_ASYNC, ASYNC_SETDCBINFO, &dcb, sizeof(DCBINFO), NULL, NULL, 0L, NULL)) == NO_ERROR)
      {
         fSuccess = TRUE;
      }
   }
   return fSuccess;
}

BOOL _Optlink comRaiseDTR(PCOMPORT cp)
{
   BOOL fSuccess = FALSE;
   APIRET rc = NO_ERROR;
   DCBINFO dcb = { 0 };
   ULONG cbPinout = 0;
   ULONG cbDinout = 0;

   cbPinout = 0;
   cbDinout = sizeof(DCBINFO);
   if((rc = DosDevIOCtl(cp->hCom, IOCTL_ASYNC, ASYNC_GETDCBINFO, NULL, 0, &cbPinout, &dcb, sizeof(DCBINFO), &cbDinout)) == NO_ERROR)
   {
      dcb.fbCtlHndShake |= MODE_DTR_CONTROL;
      if((rc = DosDevIOCtl(cp->hCom, IOCTL_ASYNC, ASYNC_SETDCBINFO, &dcb, sizeof(DCBINFO), NULL, NULL, 0L, NULL)) == NO_ERROR)
      {
         fSuccess = TRUE;
      }
   }
   return fSuccess;
}

BOOL _Optlink comHangup(PCOMPORT cp)
{
   BOOL fSuccess = FALSE;

   fSuccess = comLowerDTR(cp);
   if(fSuccess)
   {
      DosSleep(1000);
      fSuccess = comRaiseDTR(cp);
   }
   return fSuccess;
}


/*
 * Read as much data as possible up to cbRead bytes.
 */
BOOL _Optlink comRead(PCOMPORT cp, PVOID pBuffer, ULONG cbRead, PULONG pcbActual)
{
   APIRET rc = NO_ERROR;
   BOOL fSuccess = FALSE;

   /* Lock the in-buffer */
   if((rc = DosRequestMutexSem(cp->inbuf->hmtxLock, (ULONG)SEM_INDEFINITE_WAIT)) == NO_ERROR)
   {
      *pcbActual = read_modulo_buffer(cp->inbuf, pBuffer, cbRead);

      #if defined(DEBUG_TERM) && DEBUG_LEVEL >= 5
      printf("> %s read %u bytes.\n", __FUNCTION__, (*pcbActual));
      #endif

      fSuccess = TRUE;

      /* Release the in-buffer */
      rc = DosReleaseMutexSem(cp->inbuf->hmtxLock);
   }
   return fSuccess;
}

/*
 * Read cbRead bytes. Don't return until all has been read, or carrier list in case
 * carrier monitoring has been enabled.
 */
BOOL _Optlink comReadBuffer(PCOMPORT cp, PVOID pBuffer, ULONG cbRead, PULONG pcbActual)
{
   APIRET rc = NO_ERROR;
   BOOL fSuccess = FALSE;
   ULONG cbTotalRead = 0;

   while(1)
   {
      if(cp->fCheckCarrier)
      {
         if((rc = DosWaitEventSem(cp->hevCarrierLost, SEM_IMMEDIATE_RETURN)) == NO_ERROR)
         {
            /* If carrier is lost, break out and return failiure */
            break;
         }
      }

      /* Lock the in-buffer */
      if((rc = DosRequestMutexSem(cp->inbuf->hmtxLock, (ULONG)SEM_INDEFINITE_WAIT)) == NO_ERROR)
      {
         ULONG cbActual = 0UL;

         cbActual = read_modulo_buffer(cp->inbuf, (PBYTE)pBuffer + cbTotalRead, cbRead-cbTotalRead);

         cbTotalRead += cbActual;
         if(pcbActual)
         {
            (*pcbActual) = cbTotalRead;
         }

         #if defined(DEBUG_TERM) && DEBUG_LEVEL >= 5
         printf("> %s: read %u of %u bytes.\n", __FUNCTION__, cbTotalRead, cbRead);
         #endif

         if(cbTotalRead == cbRead)
         {
            rc = DosReleaseMutexSem(cp->inbuf->hmtxLock);
            fSuccess = TRUE;
            break;
         }

         /* Release the in-buffer */
         rc = DosReleaseMutexSem(cp->inbuf->hmtxLock);
      }
      DosSleep(0);
   }
   return fSuccess;
}


/*
 * Writes as much data as possible to the transmitbuffer.
 */
BOOL _Optlink comWrite(PCOMPORT cp, PVOID pBuffer, ULONG cbWrite, PULONG pcbActual)
{
   APIRET rc = NO_ERROR;
   BOOL fSuccess = FALSE;

   if((rc = DosRequestMutexSem(cp->outbuf->hmtxLock, (ULONG)SEM_INDEFINITE_WAIT)) == NO_ERROR)
   {
      *pcbActual = write_modulo_buffer(cp->outbuf, pBuffer, cbWrite);

      #ifdef DEBUG_TERM
      printf("> %s: wrote %u bytes.\n", __FUNCTION__, (*pcbActual));
      #endif

      fSuccess = TRUE;

      rc = DosReleaseMutexSem(cp->outbuf->hmtxLock);
   }
   return fSuccess;
}

/*
 * Writes data to transmit buffer
 */
BOOL _Optlink comWriteBuffer(PCOMPORT cp, PVOID pBuffer, ULONG cbWrite, PULONG pcbActual)
{
   APIRET rc = NO_ERROR;
   BOOL fSuccess = FALSE;
   ULONG cbWritten = 0;

   while(cbWrite)
   {
      if(cp->fCheckCarrier)
      {
         if((rc = DosWaitEventSem(cp->hevCarrierLost, SEM_IMMEDIATE_RETURN)) == NO_ERROR)
         {
            break;
         }
      }

      if((rc = DosRequestMutexSem(cp->outbuf->hmtxLock, (ULONG)SEM_INDEFINITE_WAIT)) == NO_ERROR)
      {
         ULONG cbActual = write_modulo_buffer(cp->outbuf, (PBYTE)pBuffer+cbWritten, cbWrite);

         #ifdef DEBUG_TERM
         printf("> %s: wrote %u bytes.\n", __FUNCTION__, (*pcbActual));
         #endif

         cbWrite -= cbActual;
         cbWritten += cbActual;

         if(pcbActual)
         {
            (*pcbActual) = cbWritten;
         }

         if(cbWrite == 0)
         {
            fSuccess = TRUE;
         }

         rc = DosReleaseMutexSem(cp->outbuf->hmtxLock);
      }
   }

   return fSuccess;
}



SHORT _Optlink comReadChar(PCOMPORT cp)
{
   APIRET rc = NO_ERROR;
   SHORT sRet = -1;

   /* Request access to the buffer */
   if((rc = DosRequestMutexSem(cp->inbuf->hmtxLock, (ULONG)SEM_INDEFINITE_WAIT)) == NO_ERROR)
   {
      BYTE b = 0;

      if(read_modulo_buffer(cp->inbuf, &b, 1UL))
      {
         /* printf("> %s read '%c' character.\n", __FUNCTION__, b); */

         sRet = (SHORT)b;
      }

      rc = DosReleaseMutexSem(cp->inbuf->hmtxLock);
   }
   return sRet;
}

SHORT _Optlink comPeekChar(PCOMPORT cp)
{
   APIRET rc = NO_ERROR;
   SHORT sRet = -1;

   /* Request access to the buffer */
   if((rc = DosRequestMutexSem(cp->inbuf->hmtxLock, (ULONG)SEM_INDEFINITE_WAIT)) == NO_ERROR)
   {
      BYTE b = 0;

      if(peek_modulo_buffer(cp->inbuf, &b, 1UL))
      {
         /* printf("> %s read '%c' character.\n", __FUNCTION__, b); */

         sRet = (SHORT)b;
      }

      rc = DosReleaseMutexSem(cp->inbuf->hmtxLock);
   }
   return sRet;
}


/*
 * Read a single character; wait until it becomes available for a speciefied amout of time.
 */
SHORT comReadCharWait(PCOMPORT cp, ULONG ulMS)
{
   APIRET rc = NO_ERROR;
   SHORT sRet = -1;

   if((rc = DosWaitEventSem(cp->inbuf->hevDataAvailable, ulMS)) == NO_ERROR)
   {
      /* Request access to the buffer */
      if((rc = DosRequestMutexSem(cp->inbuf->hmtxLock, (ULONG)SEM_INDEFINITE_WAIT)) == NO_ERROR)
      {
         BYTE b = 0;

         if(read_modulo_buffer(cp->inbuf, &b, 1UL))
         {
            /* printf("> %s read '%c' character.\n", __FUNCTION__, b); */

            sRet = (SHORT)b;
         }
         rc = DosReleaseMutexSem(cp->inbuf->hmtxLock);
      }
   }
   return sRet;
}



BOOL _Optlink comFlushTransmitBuffer(PCOMPORT cp)
{
   BOOL fSuccess = FALSE;
   SEMRECORD aSemRec[2] = { 0 };
   int i = 0;
   HMUX hmuxEvent = NULLHANDLE;

   /* Wait for outbuffer to empty */
   aSemRec[i].hsemCur = (HSEM)cp->outbuf->hevBufferEmpty;
   aSemRec[i++].ulUser = 0;

   if(cp->fCheckCarrier)
   {
      /* Monitor carrier-lost event semaphore */
      aSemRec[i].hsemCur = (HSEM)cp->hevCarrierLost;
      aSemRec[i++].ulUser = 1;
   }

   /* Create a multiple wait semaphore */
   if((cp->rc = DosCreateMuxWaitSem(NULL, &hmuxEvent, i, aSemRec, DCMW_WAIT_ANY)) == NO_ERROR)
   {
      ULONG ulUser = 0;

      /* Wait for an event */
      if((cp->rc = DosWaitMuxWaitSem(hmuxEvent, (ULONG)SEM_INDEFINITE_WAIT, &ulUser)) == NO_ERROR)
      {
         switch(ulUser)
         {
            case 0:
               fSuccess = TRUE;
               break;

            case 1:
               fSuccess = FALSE;
               break;
         }
      }
      cp->rc = DosCloseMuxWaitSem(hmuxEvent);
   }
   return fSuccess;
}






BOOL _Optlink comWaitForEvent(PCOMPORT cp, PTIMER ptim, ULONG fl)
{
   HMUX hmuxTermOrChar = NULLHANDLE;
   SEMRECORD aSemRec[3] = { 0 };
   ULONG cSemRec = 0;
   BOOL fSuccess = FALSE;
   ULONG i = 0;
   APIRET rc = NO_ERROR;
   ULONG cPosts = 0;

   if(cp->fCheckCarrier)
   {
      aSemRec[i].hsemCur = (HSEM)cp->hevCarrierLost;
      aSemRec[i].ulUser = EVENT_CARRIER_LOST;
      i++;
   }

   if(fl & FLG_TIMEOUT)
   {
      aSemRec[i].hsemCur = (HSEM)ptim->hEvent;
      aSemRec[i].ulUser = EVENT_TIMEOUT;
      i++;
   }

   aSemRec[i].hsemCur = (HSEM)cp->inbuf->hevDataAvailable;
   aSemRec[i].ulUser = EVENT_DATAAVAILABLE;
   i++;

   cSemRec = i;

   /* Create a multiple wait semaphore which waits for in characters to get available or for timeout event sempahore to get posted */
   rc = DosCreateMuxWaitSem(NULL, &hmuxTermOrChar, cSemRec, aSemRec, DCMW_WAIT_ANY);
   if(rc == NO_ERROR)
   {
      ULONG ulUser = 0;

      if((rc = DosWaitMuxWaitSem(hmuxTermOrChar, (ULONG)SEM_INDEFINITE_WAIT, &ulUser)) == NO_ERROR)
      {
         switch(ulUser)
         {
            case EVENT_DATAAVAILABLE:
               fSuccess = TRUE;
               break;

            case EVENT_TIMEOUT:
            case EVENT_CARRIER_LOST:
               fSuccess = FALSE;
               break;
         }
      }

      rc = DosCloseMuxWaitSem(hmuxTermOrChar);
   }
   return fSuccess;
}


BOOL _Optlink comWaitFor(PCOMPORT cp, PBYTE buf, ULONG cb, PTIMER ptim, ULONG fl)
{
   HMUX hmuxTermOrChar = NULLHANDLE;
   SEMRECORD aSemRec[3] = { 0 };
   ULONG cSemRec = 0;
   BOOL fSuccess = FALSE;
   ULONG i = 0;
   APIRET rc = NO_ERROR;
   ULONG cPosts = 0;

   if(fl & FLG_CARRIER_LOST)
   {
      aSemRec[i].hsemCur = (HSEM)cp->hevCarrierLost;
      aSemRec[i].ulUser = EVENT_CARRIER_LOST;
      i++;
   }

   if(fl & FLG_TIMEOUT)
   {
      aSemRec[i].hsemCur = (HSEM)ptim->hEvent;
      aSemRec[i].ulUser = EVENT_TIMEOUT;
      i++;
   }

   aSemRec[i].hsemCur = (HSEM)cp->inbuf->hevDataAvailable;
   aSemRec[i].ulUser = EVENT_DATAAVAILABLE;
   i++;

   cSemRec = i;

   /* Create a multiple wait semaphore which waits for in characters to get available or for timeout event sempahore to get posted */
   if((rc = DosCreateMuxWaitSem(NULL, &hmuxTermOrChar, cSemRec, aSemRec, DCMW_WAIT_ANY)) == NO_ERROR)
   {
      ULONG ulUser = 0;
      BOOL fDone = FALSE;

      i = 0;

      while(!fDone)
      {
         if((rc = DosWaitMuxWaitSem(hmuxTermOrChar, (ULONG)SEM_INDEFINITE_WAIT, &ulUser)) == NO_ERROR)
         {
            char ch = 0;
            switch(ulUser)
            {
               case EVENT_DATAAVAILABLE:
                  ch = (char)comReadChar(cp);
                  if(buf[i++] == ch)
                  {
                     if(i == cb)
                     {
                        fSuccess = TRUE;
                        fDone = TRUE;
                     }
                  }
                  else
                  {
                     i = 0;
                  }
                  break;

               case EVENT_TIMEOUT:
                  fDone = TRUE;
                  break;

               case EVENT_CARRIER_LOST:
                  fDone = TRUE;
                  break;
            }
         }
         else
         {
            printf("*** ERROR: DosWaitMuxWaitSem() returned %u.\n", rc);
         }
      }

      rc = DosCloseMuxWaitSem(hmuxTermOrChar);
   }
   else
   {
      printf("***ERROR: DosCreateMuxWaitSem() returned %u.\n", rc);
   }

   return fSuccess;
}

BOOL _Optlink comReadWait(PCOMPORT cp, PBYTE pBuffer, ULONG cbRead, PULONG pcbActual, PTIMER ptim, ULONG fl)
{
   HMUX hmuxTermOrChar = NULLHANDLE;
   SEMRECORD aSemRec[3] = { 0 };
   ULONG cSemRec = 0;
   BOOL fSuccess = FALSE;
   ULONG i = 0;
   APIRET rc = NO_ERROR;
   ULONG cPosts = 0;

   *pcbActual = 0;

   if(fl & FLG_CARRIER_LOST)
   {
      aSemRec[i].hsemCur = (HSEM)cp->hevCarrierLost;
      aSemRec[i].ulUser = EVENT_CARRIER_LOST;
      i++;
   }

   if(fl & FLG_TIMEOUT)
   {
      aSemRec[i].hsemCur = (HSEM)ptim->hEvent;
      aSemRec[i].ulUser = EVENT_TIMEOUT;
      i++;
   }

   aSemRec[i].hsemCur = (HSEM)cp->inbuf->hevDataAvailable;
   aSemRec[i].ulUser = EVENT_DATAAVAILABLE;
   i++;

   cSemRec = i;

   /* Create a multiple wait semaphore which waits for in characters to get available or for timeout event sempahore to get posted */
   if((rc = DosCreateMuxWaitSem(NULL, &hmuxTermOrChar, cSemRec, aSemRec, DCMW_WAIT_ANY)) == NO_ERROR)
   {
      ULONG ulUser = 0;
      BOOL fDone = FALSE;

      i = 0;

      while(cbRead)
      {
         if((rc = DosWaitMuxWaitSem(hmuxTermOrChar, (ULONG)SEM_INDEFINITE_WAIT, &ulUser)) == NO_ERROR)
         {
            char ch = 0;
            switch(ulUser)
            {
               case EVENT_DATAAVAILABLE:
                  ch = (char)comReadChar(cp);
                  *pBuffer++ = ch;
                  cbRead--;
                  *(pcbActual) += 1;
                  break;

               case EVENT_TIMEOUT:
                  fDone = TRUE;
                  break;

               case EVENT_CARRIER_LOST:
                  fDone = TRUE;
                  break;
            }
         }
         else
         {
            printf("*** ERROR: DosWaitMuxWaitSem() returned %u.\n", rc);
         }
      }

      rc = DosCloseMuxWaitSem(hmuxTermOrChar);
   }
   else
   {
      printf("***ERROR: DosCreateMuxWaitSem() returned %u.\n", rc);
   }

   return fSuccess;
}




void _Optlink comMonitorCarrier(PCOMPORT cp, BOOL fState)
{
   printf("> %s: Attempt to pause receive thread\n", __FUNCTION__);

   if(pause_thread(cp->pauseReceive))
   {
      ULONG cPosts = 0;
      APIRET rc = NO_ERROR;

      #ifdef DEBUG_TERM
      printf("> %s: Receive thread paused\n", __FUNCTION__);
      #endif

      rc = DosQueryEventSem(cp->hevCarrierLost, &cPosts);
      if(!rc && cPosts)
      {
         rc = DosResetEventSem(cp->hevCarrierLost, &cPosts);
      }

      cp->fCheckCarrier = fState;

      if(resume_thread(cp->pauseReceive))
      {
         #ifdef DEBUG_TERM
         printf("> %s: Receive thread resumed\n", __FUNCTION__);
         #endif
      }
   }
}


HEV _Optlink queryInDataAvailableSemHandle(PCOMPORT cp)
{
   return cp->inbuf->hevDataAvailable;
}

HEV _Optlink queryCarrierLostSemHandle(PCOMPORT cp)
{
   return cp->hevCarrierLost;
}




#ifdef __TOOLKIT45__
static void _System receive_thread(ULONG param)
#else
static void _Optlink receive_thread(void* param)
#endif
{
   PCOMPORT cp = (PCOMPORT)param;
   APIRET rc = NO_ERROR;
   BOOL fTerminate = FALSE;
   const ULONG cbTempMax = 65536;
   ULONG cbTemp = 0;
   PBYTE pTemp = malloc(cbTempMax);
   ULONG cTermPosts = 0;

   printf("> Receive thread started. cp = %08x, hCom = %08x\n", cp, cp->hCom);

   while(!cTermPosts)
   {
      ULONG cbRead = 0;
      ULONG cbActual = 0;

      /* Check if a pause thread has been requested */
      check_thread_pause(cp->pauseReceive);

      /* Get data from COM port into temporary buffer */
      if((rc = DosRead(cp->hCom, pTemp+cbTemp, cbTempMax-cbTemp, &cbActual)) == NO_ERROR)
      {
         #ifdef DEBUG_TERM
         printf("> DosRead() cbActual = %u.\n", cbActual);
         #endif
         cbTemp += cbActual;
      }
      else
      {
         printf("*** ERROR: DosRead(%08x, %08x, %u) returned %u.\n", cp->hCom, pTemp+cbTemp, cbTempMax-cbTemp, rc);
         printf("           cbActual = %u\n", cbActual);
      }

      /*
       * Check if carrier detect monitoring is enabled
       * Note: This should be done after DosRead() but _before_ transfering data to the modulo-buffer, since
       *       it makes sure that the "NO CARRIER" string won't be missed by the application.
       */
      if(cp->fCheckCarrier)
      {
         if(!comCarrier(cp))
         {
            DosPostEventSem(cp->hevCarrierLost);
         }
         cp->fCheckCarrier = FALSE;
      }

      if(cbTemp)
      {
         ULONG cbWritten = 0;

         /* Request receive buffer access */
         if((rc = DosRequestMutexSem(cp->inbuf->hmtxLock, SEM_INDEFINITE_WAIT)) == NO_ERROR)
         {
            ULONG cbWritten = 0;

            /* Store data in modulo buffer */
            cbWritten = write_modulo_buffer(cp->inbuf, pTemp, cbTemp);
            cbTemp -= cbWritten;

            /* If entire buffer wasn't transferred, move the rest of the data to the beginning of the temp buffer */
            if(cbTemp)
            {
               memmove(pTemp, pTemp+cbWritten, cbTemp);
            }

            /* Release receive buffer access */
            rc = DosReleaseMutexSem(cp->inbuf->hmtxLock);
         }
      }
      DosSleep(0);

      /* Check for termination event sempahore */
      rc = DosQueryEventSem(cp->hevTerminate, &cTermPosts);
   }
   free(pTemp);

   printf("> %s ending.\n", __FUNCTION__);

   #ifdef __TOOLKIT45__
   DosExit(EXIT_THREAD, 0UL);
   #else
   _endthread();
   #endif
}


#ifdef __TOOLKIT45__
static void _System transmit_thread(ULONG param)
#else
static void _Optlink transmit_thread(void* param)
#endif
{
   PCOMPORT cp = (PCOMPORT)param;
   APIRET rc = NO_ERROR;
   ULONG cTermPosts = 0;
   ULONG cPosts = 0;
   const ULONG cbTempMax = 65536;
   PBYTE pTemp = malloc(cbTempMax);
   HMUX hmuxEvent = NULLHANDLE;
   BOOL fTerminate = FALSE;
   SEMRECORD aSemRec[2] = { 0 };

   printf("> Transmit thread started. cp = %08x, hCom = %08x\n", cp, cp->hCom);

   /*
    * Monitor termination event and "data available" event
    * Note: It's important that termination is first since MuxWait checks events
    *       in the same order they are added. Termination should have a higher
    *       priority than in data event.
    */
   aSemRec[0].hsemCur = (HSEM)cp->hevTerminate;
   aSemRec[0].ulUser = 0;
   aSemRec[1].hsemCur = (HSEM)cp->outbuf->hevDataAvailable;
   aSemRec[1].ulUser = 1;

   if((rc = DosCreateMuxWaitSem(NULL, &hmuxEvent, 2, aSemRec, DCMW_WAIT_ANY)) == NO_ERROR)
   {
      while(!fTerminate)
      {
         ULONG ulUser = 0;

         /* Wait for termination or new data event */
         if((rc = DosWaitMuxWaitSem(hmuxEvent, (ULONG)SEM_INDEFINITE_WAIT, &ulUser)) == NO_ERROR)
         {
            switch(ulUser)
            {
               case 0:
                  fTerminate = TRUE;
                  break;

               case 1:
                  if((rc = DosRequestMutexSem(cp->outbuf->hmtxLock, SEM_INDEFINITE_WAIT)) == NO_ERROR)
                  {
                     ULONG cbTemp = 0;

                     cbTemp = read_modulo_buffer(cp->outbuf, pTemp, cbTempMax);

                     /* Loop until all data has been written to the port */
                     while(cbTemp)
                     {
                        ULONG cbActual = 0;

                        #ifdef DEBUG
                        printf("> DosWrite(%08x, %08x, %u)\n", cp->hCom, pTemp, cbTemp);
                        #endif

                        if((rc = DosWrite(cp->hCom, pTemp, cbTemp, &cbActual)) == NO_ERROR)
                        {
                           #ifdef DEBUG
                           printf("> DosWrite() wrote %u bytes.\n", cbActual);
                           #endif
                        }
                        else
                        {
                           printf("*** ERROR: DosWrite(%08x, %08x, %u) returned %u.\n", cp->hCom, pTemp, cbTemp, rc);
                           printf("           cbActual = %u\n", cbActual);
                        }
                        cbTemp -= cbActual;
                        if(cbTemp)
                        {
                           memmove(pTemp, pTemp+cbActual, cbTemp);
                           DosSleep(0);
                        }
                     }
                     rc = DosReleaseMutexSem(cp->outbuf->hmtxLock);
                  }
                  break;
            }
         }
      }
   }

   DosCloseMuxWaitSem(hmuxEvent);

   free(pTemp);

   printf("> %s ending.\n", __FUNCTION__);

   #ifdef __TOOLKIT45__
   DosExit(EXIT_THREAD, 0UL);
   #else
   _endthread();
   #endif
}




static PMODULOBUFFER _Optlink init_modulo_buffer(ULONG cbBuffer)
{
   PMODULOBUFFER modbuf = NULL;
   APIRET rc = NO_ERROR;
   PVOID pAlloc = NULL;
   ULONG cbAlloc = cbBuffer+(sizeof(MODULOBUFFER)-1);

   /*
    * To do: Allocate core structure and in/out modulo buffers in one memory object.
    */

   /* Allocate modulo buffer header and modulo buffer data area */
   if((rc = DosAllocSharedMem(&pAlloc, NULL, cbAlloc, PAG_READ | PAG_WRITE | PAG_COMMIT | OBJ_GETTABLE)) == NO_ERROR)
   {
      memset(pAlloc, 0, cbAlloc);

      modbuf = (PMODULOBUFFER)pAlloc;
   }

   if(modbuf)
   {
      rc = DosCreateEventSem(NULL, &modbuf->hevDataAvailable, DC_SEM_SHARED, FALSE);
      if(rc == NO_ERROR)
      {
         rc = DosCreateEventSem(NULL, &modbuf->hevBufferEmpty, DC_SEM_SHARED, TRUE);
      }
      if(rc == NO_ERROR)
      {
         rc = DosCreateMutexSem(NULL, &modbuf->hmtxLock, DC_SEM_SHARED, FALSE);
      }

      modbuf->cbBuffer = cbBuffer;

      reset_modulo_buffer(modbuf);
   }
   return modbuf;
}

static void _Optlink term_modulo_buffer(PMODULOBUFFER modbuf)
{
   DosCloseMutexSem(modbuf->hmtxLock);
   DosCloseEventSem(modbuf->hevBufferEmpty);
   DosCloseEventSem(modbuf->hevDataAvailable);

   DosFreeMem(modbuf);
}

BOOL _Optlink open_modulo_buffer(PMODULOBUFFER modbuf)
{
   APIRET rc = NO_ERROR;
   if((rc = DosGetSharedMem(modbuf, PAG_READ | PAG_WRITE)) == NO_ERROR)
   {
      DosOpenEventSem(NULL, &modbuf->hevDataAvailable);
      DosOpenEventSem(NULL, &modbuf->hevBufferEmpty);
      DosOpenMutexSem(NULL, &modbuf->hmtxLock);
   }
   return TRUE;
}

BOOL _Optlink close_modulo_buffer(PMODULOBUFFER modbuf)
{
   DosCloseMutexSem(modbuf->hmtxLock);
   DosCloseEventSem(modbuf->hevBufferEmpty);
   DosCloseEventSem(modbuf->hevDataAvailable);
   DosFreeMem(modbuf);
   return TRUE;
}

void _Inline reset_modulo_buffer(PMODULOBUFFER modbuf)
{
   APIRET rc = NO_ERROR;
   ULONG cPosts = 0;

   modbuf->iRead = modbuf->iWrite = 0UL;
   modbuf->cbFree = modbuf->cbBuffer;
   modbuf->cbUsed = 0;

   rc = DosResetEventSem(modbuf->hevDataAvailable, &cPosts);
   rc = DosPostEventSem(modbuf->hevBufferEmpty);
}

ULONG _Inline read_modulo_buffer(PMODULOBUFFER modbuf, PBYTE p, ULONG cb)
{
   ULONG c = 0;
   APIRET rc = NO_ERROR;
   ULONG cPosts = 0;

   while(cb-- && modbuf->cbUsed)
   {
      /* printf("Read %02x\n", p[c]); */
      p[c++] = modbuf->data[modbuf->iRead];                   /* Copy character from receive buffer aux buffer */
      modbuf->iRead = (modbuf->iRead+1) % modbuf->cbBuffer;   /* Move read pointer forward */
      modbuf->cbFree++;
      modbuf->cbUsed--;
   }
   rc = DosQueryEventSem(modbuf->hevDataAvailable, &cPosts);
   if(modbuf->cbUsed == 0UL && cPosts)
   {
      rc = DosResetEventSem(modbuf->hevDataAvailable, &cPosts);
      #ifdef DEBUG
      if(rc == NO_ERROR)
      {
         puts("> Reset data available semaphore successful");
      }
      #endif
   }

   rc = DosQueryEventSem(modbuf->hevBufferEmpty, &cPosts);
   if(modbuf->cbUsed == 0UL && cPosts)
   {
      rc = DosResetEventSem(modbuf->hevBufferEmpty, &cPosts);
   }

   return c;
}

/*
 * Same as read_modulo_buffer, but it doesn't change buffer pointers or other information
 */
ULONG _Inline peek_modulo_buffer(PMODULOBUFFER modbuf, PBYTE p, ULONG cb)
{
   ULONG c = 0;
   APIRET rc = NO_ERROR;
   ULONG cPosts = 0;
   ULONG iTemp = modbuf->iRead;

   cb = min(cb, modbuf->cbUsed);
   while(cb--)
   {
      p[c++] = modbuf->data[iTemp];            /* Copy character from receive buffer aux buffer */
      iTemp = (iTemp+1) % modbuf->cbBuffer;    /* Move temporary read pointer forward */
   }

   return c;
}


ULONG _Inline write_modulo_buffer(PMODULOBUFFER modbuf, PBYTE p, ULONG cb)
{
   ULONG c = 0;

   while(cb-- && modbuf->cbFree)
   {
      /* printf("Write %02x\n", p[c]); */
      modbuf->data[modbuf->iWrite] = p[c++];                        /* Copy character to modulo buffer */
      modbuf->iWrite = (modbuf->iWrite + 1) % modbuf->cbBuffer;     /* Move write pointer forward */
      modbuf->cbFree--;
      modbuf->cbUsed++;
   }
   if(modbuf->cbUsed)
   {
      APIRET rc = NO_ERROR;
      ULONG cPosts = 0;
      rc = DosQueryEventSem(modbuf->hevDataAvailable, &cPosts);
      if(!cPosts)
      {
         rc = DosPostEventSem(modbuf->hevDataAvailable);
         #ifdef DEBUG
         if(rc == NO_ERROR)
         {
            puts("> Posted data available semaphore");
         }
         #endif
      }
   }
   else
   {
      APIRET rc = NO_ERROR;
      ULONG cPosts = 0;
      rc = DosQueryEventSem(modbuf->hevBufferEmpty, &cPosts);
      if(!cPosts)
      {
         rc = DosPostEventSem(modbuf->hevBufferEmpty);
      }
   }

   return c;
}





PTIMER _Optlink init_timer(void)
{
   APIRET rc = NO_ERROR;
   PTIMER ptim = (PTIMER)malloc(sizeof(TIMER));

   memset(ptim, 0, sizeof(TIMER));
   if((rc = DosCreateEventSem(NULL, &ptim->hEvent, DC_SEM_SHARED, FALSE)) != NO_ERROR)
   {
      free(ptim);
      ptim = NULL;
   }
   return ptim;
}

BOOL _Optlink term_timer(PTIMER ptim)
{
   APIRET rc = NO_ERROR;

   if((rc = DosCloseEventSem(ptim->hEvent)) == NO_ERROR)
   {
      return TRUE;
   }
   return FALSE;
}

BOOL _Optlink start_timer(PTIMER ptim, ULONG ulMS)
{
   APIRET rc = NO_ERROR;
   ULONG cPosts = 0;
   BOOL fSuccess = FALSE;

   rc = DosQueryEventSem(ptim->hEvent, &cPosts);
   if(rc == NO_ERROR && cPosts)
   {
      DosResetEventSem(ptim->hEvent, &cPosts);
   }
   if((rc = DosAsyncTimer(ulMS, (HSEM)ptim->hEvent, &ptim->hTimer)) == NO_ERROR)
   {
      fSuccess = TRUE;
   }
   return fSuccess;
}

BOOL _Optlink stop_timer(PTIMER ptim)
{
   APIRET rc = NO_ERROR;
   rc = DosStopTimer(ptim->hTimer);
   if(rc == NO_ERROR)
   {
      return TRUE;
   }
   return FALSE;
}

BOOL _Optlink timeout(PTIMER ptim)
{
   APIRET rc = NO_ERROR;

   if((rc = DosWaitEventSem(ptim->hEvent, SEM_IMMEDIATE_RETURN)) == NO_ERROR)
   {
      return TRUE;
   }
   return FALSE;
}



static PTHREADPAUSE _Optlink init_threadpause(void)
{
   PTHREADPAUSE ptp = malloc(sizeof(THREADPAUSE));
   if(ptp)
   {
      APIRET rc = NO_ERROR;

      memset(ptp, 0, sizeof(THREADPAUSE));

      rc = DosCreateEventSem(NULL, &ptp->hevPause, 0UL, FALSE);
      if(rc == NO_ERROR)
      {
         rc = DosCreateEventSem(NULL, &ptp->hevPaused, DCE_AUTORESET, FALSE);
      }
      if(rc == NO_ERROR)
      {
         rc = DosCreateEventSem(NULL, &ptp->hevResume, DCE_AUTORESET, FALSE);
      }
   }
   return ptp;
}

static BOOL _Optlink term_threadpause(PTHREADPAUSE ptp)
{
   APIRET rc = NO_ERROR;

   rc = DosCloseEventSem(ptp->hevPause);
   rc = DosCloseEventSem(ptp->hevPaused);
   rc = DosCloseEventSem(ptp->hevResume);
   free(ptp);

   return TRUE;
}

static BOOL _Inline pause_thread(PTHREADPAUSE ptp)
{
   APIRET rc = NO_ERROR;
   BOOL fPaused = FALSE;

   /* Tell thread to pause */
   rc = DosPostEventSem(ptp->hevPause);
   if(rc == NO_ERROR)
   {
      /* For for thread to acknowledge pause event */
      rc = DosWaitEventSem(ptp->hevPaused, (ULONG)SEM_INDEFINITE_WAIT);
      if(rc == NO_ERROR)
      {
         ULONG cPosts = 0;

         /* Reset pause event semaphore */
         rc = DosResetEventSem(ptp->hevPause, &cPosts);

         fPaused = TRUE;
      }
   }
   return fPaused;
}

static BOOL _Inline resume_thread(PTHREADPAUSE ptp)
{
   APIRET rc = NO_ERROR;
   BOOL fResumed = FALSE;

   rc = DosPostEventSem(ptp->hevResume);
   if(rc == NO_ERROR)
   {
      fResumed = TRUE;
   }
   return fResumed;
}

void _Inline check_thread_pause(PTHREADPAUSE ptp)
{
   APIRET rc = NO_ERROR;

   rc = DosWaitEventSem(ptp->hevPause, SEM_IMMEDIATE_RETURN);
   if(rc == NO_ERROR)
   {
      ULONG cPosts = 0;

      printf("> %s: Pause requested\n", __FUNCTION__);

      /* Post paused state notification event */
      rc = DosPostEventSem(ptp->hevPaused);
      if(rc == NO_ERROR)
      {
         printf("> %s: Paused event posted\n", __FUNCTION__);

         printf("> %s: Wait for resume event\n", __FUNCTION__);
         rc = DosWaitEventSem(ptp->hevResume, SEM_INDEFINITE_WAIT);
         if(rc == NO_ERROR)
         {
            printf("> %s: Got resume event\n", __FUNCTION__);
         }
      }
      DosResetEventSem(ptp->hevResume, &cPosts);
   }
}
