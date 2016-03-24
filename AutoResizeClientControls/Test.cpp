#define INCL_WINWINDOWMGR

#include <os2.h>

#include <string.h>                      // memset()

#include "..\globals\common.hpp"

#include "AutoResizeClientControls.h"


int main(int argc, char *argv[])
{
   AnchorBlock    ab;
   MessageQueue   mq(ab);
   HWND           hwndFrame = NULLHANDLE;
   HWND           hwndClient = NULLHANDLE;
   ULONG          flFrame = FCF_TITLEBAR | FCF_SYSMENU | FCF_MINMAX | FCF_SIZEBORDER | FCF_SHELLPOSITION | FCF_NOBYTEALIGN | FCF_TASKLIST;
   QMSG           qmsg;

   ascRegisterClass(ab);

   hwndFrame = WinCreateStdWindow(HWND_DESKTOP, WS_ANIMATE, &flFrame, WC_AUTOSIZECLIENT, "AutoSizeClient test", 0, (HMODULE)NULLHANDLE, 100, &hwndClient);

   CREATEWND   cw;
   memset(&cw, 0, sizeof(cw));

   cw.hwndParent = hwndClient;
   cw.pszClass = WC_BUTTON;
   cw.pszTitle = "MyButton";
   cw.flStyle = WS_VISIBLE;

   cw.pos.x = 0.10;
   cw.pos.y = 0.10;
   cw.pos.xx = 0.30;
   cw.pos.yy = 0.20;

   cw.hwndOwner = hwndClient;
   cw.hwndInsertBehind = HWND_TOP;
   cw.id = 200;

   if(WinSendMsg(hwndClient, WMU_ARCC_ADD, (MPARAM)&cw, (MPARAM)NULL) == NULLHANDLE)
      return 1;

   cw.pszTitle = "MyButton2";
   cw.pos.x = 0.20;
   cw.pos.y = 0.20;
   cw.pos.xx = 0.40;
   cw.pos.yy = 0.30;
   cw.id = 201;

   if(WinSendMsg(hwndClient, WMU_ARCC_ADD, (MPARAM)&cw, (MPARAM)NULL) == NULLHANDLE)
      return 1;

   WinShowWindow(hwndFrame, TRUE);

   while(WinGetMsg(ab, &qmsg, NULL, NULL, NULL))
      WinDispatchMsg(ab, &qmsg);

   return 0;
}
