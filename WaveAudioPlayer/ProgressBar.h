/*
** ProgressBar structures and definitions
*/
// classname
#define WC_PROGRESSBAR "ProgressBarClass"


// ProgressBar Creation data
typedef struct _PBCDATA
{
   ULONG    cbSize;
   ULONG    ulMin;
   ULONG    ulMax;
   ULONG    ulCurrent;
}PBCDATA, *PPBCDATA;


#define PBM_SETFRAMEOUTERBORDERCOLORS    WM_USER+1
#define PBM_SETFRAMEINNERBORDERCOLORS    WM_USER+2
#define PBM_SETBARBORDERCOLORS           WM_USER+3
#define PBM_SETBARCOLORS                 WM_USER+4
#define PBM_SETRANGE                     WM_USER+5
#define PBM_SETVALUE                     WM_USER+6
#define PBM_QUERYRANGE                   WM_USER+8
#define PBM_QUERYVALUE                   WM_USER+9


#define WinAddPBHilightField(hWnd,ulStart,ulLength) ((void)WinSendMsg((HWND)(hWnd),PBM_ADDHILIGHTFIELD,(MPARAM)(ulStart),(MPARAM)(ulLength)))
#define WinSetPBLimits(hWnd,ulLower,ulUpper)        ((BOOL)WinSendMsg((HWND)(hWnd),PBM_SETRANGE,(MPARAM)(ulLower),(MPARAM)(ulUpper)))
#define WinSetPBValue(hWnd,ulVal)                   ((void)WinSendMsg((HWND)(hWnd),PBM_SETVALUE,(MPARAM)(ulVal),(MPARAM)NULL))
#define WinSetPBFrame1Colors(hWnd,ulHi,ulLo)        ((void)WinSendMsg((HWND)(hWnd),PBM_SETFRAMEOUTERBORDERCOLORS,(MPARAM)(ulHi),(MPARAM)(ulLo)))
#define WinSetPBFrame2Colors(hWnd,ulHi,ulLo)        ((void)WinSendMsg((HWND)(hWnd),PBM_SETFRAMEINNERBORDERCOLORS,(MPARAM)(ulHi),(MPARAM)(ulLo)))
#define WinSetPBFrame3Colors(hWnd,ulHi,ulLo)        ((void)WinSendMsg((HWND)(hWnd),PBM_SETBARBORDERCOLORS,(MPARAM)(ulHi),(MPARAM)(ulLo)))

#define WinQueryPBRange(hWnd,ulLo,ulHi)             ((BOOL)WinSendMsg((HWND)hWnd,PBM_QUERYRANGE,(MPARAM)ulLo,(MPARAM)ulHi))
#define WinQueryPBValue(hWnd)                       ((ULONG)WinSendMsg((HWND)hWnd,PBM_QUERYVALUE,(MPARAM)NULL,(MPARAM)NULL))


extern BOOL pbcRegisterClass(void);

