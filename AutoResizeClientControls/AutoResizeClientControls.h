// window class name
#define WC_AUTOSIZECLIENT                "AutoSizeClientClass"

// relative coordinates structure
typedef struct _RELWNDPOS
{
   float x;                              // relative left coordinate from 0.0 to 1.0
   float y;                              // relative bottom coordinate from 0.0 to 1.0
   float xx;                             // relative right coordinate from 0.0 to 1.0
   float yy;                             // relative top coordinate from 0.0 to 1.0
   LONG  xAdd;                           // how many pels to add/subtract
   LONG  yAdd;                           // how many pels to add/subtract
   LONG  xxAdd;                          // how many pels to add/subtract
   LONG  yyAdd;                          // how many pels to add/subtract
}RELWNDPOS, *PRELWNDPOS;

// WinCreateWindow() parameters and relativecoordinates structure
typedef struct _CREATEWND
{
   HWND        hwndParent;
   PSZ         pszClass;
   PSZ         pszTitle;
   ULONG       flStyle;
   RELWNDPOS   pos;
   HWND        hwndOwner;
   HWND        hwndInsertBehind;
   ULONG       id;
   PVOID       pCtlData;
   PVOID       pPresParams;
}CREATEWND, *PCREATEWND;

// function prototypes
extern BOOL ascRegisterClass(HAB hAB);

// usermessages
#define WMU_ARCC_ADD                     WM_USER+1

