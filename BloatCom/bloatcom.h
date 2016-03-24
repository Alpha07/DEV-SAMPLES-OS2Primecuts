typedef struct _COMPORT *PCOMPORT;

typedef struct _TIMER *PTIMER;

PCOMPORT _Optlink comOpen(PSZ pszComName, ULONG cbInBuffer, ULONG cbOutBuffer);
BOOL _Optlink comClose(PCOMPORT cp);
PCOMPORT _Optlink comAquireFromHandle(HFILE hCom);
void _Optlink comDeaquireAccess(PCOMPORT cp);

BOOL _Optlink comSetBaudrate(PCOMPORT cp, ULONG bps);
BOOL _Optlink comSetLineControl(PCOMPORT cp, BYTE bDataBits, BYTE bParity, BYTE bStopBits);

BOOL _Optlink comCarrier(PCOMPORT cp);
BOOL _Optlink comLowerDTR(PCOMPORT cp);
BOOL _Optlink comRaiseDTR(PCOMPORT cp);
BOOL _Optlink comHangup(PCOMPORT cp);

BOOL _Optlink comRead(PCOMPORT cp, PVOID pBuffer, ULONG cbRead, PULONG pcbActual);
BOOL _Optlink comWrite(PCOMPORT cp, PVOID pBuffer, ULONG cbWrite, PULONG pcbActual);


/* BOOL _Optlink comWaitFor(PCOMPORT cp, PBYTE buf, ULONG cb, PTIMER ptim); */
BOOL _Optlink comWaitFor(PCOMPORT cp, PBYTE buf, ULONG cb, PTIMER ptim, ULONG fl);

void _Optlink comMonitorCarrier(PCOMPORT cp, BOOL fState);

PTIMER _Optlink init_timer(void);
BOOL _Optlink term_timer(PTIMER ptim);
BOOL _Optlink start_timer(PTIMER ptim, ULONG ulMS);
BOOL _Optlink stop_timer(PTIMER ptim);
BOOL _Optlink timeout(PTIMER ptim);



#define FLG_TIMEOUT                      0x00000001
#define FLG_CARRIER_LOST                 0x00000002

