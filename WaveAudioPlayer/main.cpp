#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINSHELLDATA
#define INCL_WINSTATICS
#define INCL_WINSTDSLIDER
#define INCL_DOSPROCESS
#define INCL_OS2MM
#define INCL_MMIOOS2

#include <os2.h>
#include <os2me.h>


#include <string.h>
#include <stdlib.h>
#include <process.h>

#include "res.h"
#include "progressbar.h"


class AnchorBlock
{
   private:
   HAB   handle;

   public:
   AnchorBlock(ULONG flOptions = 0) {
      handle = WinInitialize(flOptions);
   }
   ~AnchorBlock() {
      if(handle)
         WinTerminate(handle);
   }

   operator HAB() { return handle; }
};

class MessageQueue
{
   private:
   HMQ   handle;

   public:
   MessageQueue(AnchorBlock& ab, LONG lQueueSize = 0) {
      handle = WinCreateMsgQueue(ab, lQueueSize);
   }
   ~MessageQueue() {
      if(handle)
         WinDestroyMsgQueue(handle);
   }

   operator HMQ() { return handle; }
};


typedef struct _INITDATA
{
   ULONG cbSize;
   int   argc;
   char  **argv;
   HWND  hWnd;
}INITDATA, *PINITDATA;


typedef struct _STRINGLIST
{
   struct _STRINGLIST *next;
   struct _STRINGLIST *prev;
   PSZ   pszString;
}STRINGLIST, *PSTRINGLIST;

// windowdata
#define QWP_PLAYLIST                     0
#define QWP_CURRENTENTRY                 QWP_PLAYLIST+sizeof(PSTRINGLIST)
#define QWL_PLAYLISTENTRIES              QWP_CURRENTENTRY+sizeof(PSTRINGLIST)
#define QWS_DEVICEID                     QWL_PLAYLISTENTRIES+sizeof(ULONG)
#define QWL_LOOP                         QWS_DEVICEID+sizeof(USHORT)
#define QW_EXTRA                         QWL_LOOP+sizeof(ULONG)

// window usermessages
#define WMU_LOAD                         WM_USER+1
#define WMU_PLAY                         WM_USER+2

// mm user parameters
#define MMUP_PLAY                        1

// function prototypes
MRESULT EXPENTRY MainWndProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2);
BOOL mwpCreate(HWND hWnd, MPARAM mp1);
void mwpPaint(HWND hWnd);
void mwpSize(HWND hWnd);
BOOL mwpLoad(HWND hWnd);
BOOL mwpPlay(HWND hWnd);

void _Optlink StartupThread(void *pArg);
void _Optlink ShutdownThread(void *pArg);

void AddToPlaylist(HWND hWnd, PSZ pszFilespec);
void MakePathFromFilepath(PSZ pszPath, PSZ pszFilepath);
void plAddEntry(HWND hWnd, PSZ pszFilename);
void plNext(HWND hWnd);
void plNuke(HWND hWnd);

void mmMCINotify(HWND hWnd, MPARAM mp1, MPARAM mp2);
void PlayCompleted(HWND hWnd);

BOOL IsValidFile(PSZ pszFilename);
void MakeWndTitleFromPathName(PSZ pszTitle, PCSZ pszPathName);
BOOL WinSetControlRelPos(HWND hWnd, HWND hwndInsertBehind, PRECTL rect, float X, float Y, float XX, float YY, ULONG fl, LONG x1a, LONG y1a, LONG x2a, LONG y2a);


// usage: WavePlayer [options] [file1 [file2 ... [fileN]]]
//    v# - volume (0-100)
//    i# - device index (0 - default)
//    l - loop (stop playing when closed)
int main(int argc, char **argv)
{
   AnchorBlock    ab;
   MessageQueue   mq(ab);
   HWND           hwndFrame = NULLHANDLE;
   HWND           hwndClient = NULLHANDLE;
   FRAMECDATA     fcd;
   PSZ            pszClientClass = NULL;
   PSZ            pszTitle = NULL;
   QMSG           qmsg;

   pbcRegisterClass();

   PINITDATA pinitdata = new INITDATA;

   memset(pinitdata, 0, sizeof(INITDATA));

   pinitdata->cbSize = sizeof(INITDATA);
   pinitdata->argc = argc;
   pinitdata->argv = argv;

   fcd.cb = sizeof(fcd);
   fcd.flCreateFlags = FCF_TITLEBAR | FCF_SYSMENU | FCF_MINMAX | FCF_SIZEBORDER | FCF_ICON | FCF_SHELLPOSITION | FCF_TASKLIST;
   fcd.hmodResources = NULLHANDLE;
   fcd.idResources = WIN_MAIN;

   pszClientClass = new char[128];
   WinLoadString(ab, (HMODULE)NULLHANDLE, IDS_CLIENTCLASSNAME, 128, pszTitle);
   WinRegisterClass(ab, pszClientClass, MainWndProc, CS_SIZEREDRAW, QW_EXTRA);

   pszTitle = new char[128];
   WinLoadString(ab, (HMODULE)NULLHANDLE, IDS_APPNAME, 128, pszTitle);
   hwndFrame = WinCreateWindow(HWND_DESKTOP, WC_FRAME, pszTitle, WS_ANIMATE, 0, 0, 0, 0, (HWND)NULLHANDLE, HWND_TOP, WIN_MAIN, (PVOID)&fcd, (PVOID)NULL);
   hwndClient = WinCreateWindow(hwndFrame, pszClientClass, (PSZ)NULL, 0, 0, 0, 0, 0, hwndFrame, HWND_TOP, FID_CLIENT, (PVOID)pinitdata, (PVOID)NULL);

   delete pszClientClass;
   delete pszTitle;

   if(hwndFrame == NULLHANDLE || hwndClient == NULLHANDLE)
      return 1;

   while(WinGetMsg(ab, &qmsg, NULL, NULL, NULL))
      WinDispatchMsg(ab, &qmsg);

   WinDestroyWindow(hwndFrame);

   return 0;
}


MRESULT EXPENTRY MainWndProc(HWND hWnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   BOOL    fHandled = TRUE;
   MRESULT mReturn = 0;

   switch(msg)
   {
      case WM_CREATE:
         return (MRESULT)mwpCreate(hWnd, mp1);

      case WM_PAINT:
         mwpPaint(hWnd);
         break;

      case WM_SIZE:
         mwpSize(hWnd);
         break;

      case WM_CONTROL:
         switch(SHORT1FROMMP(mp1))
         {
            case SLRD_VOLUME:
               if((SHORT2FROMMP(mp1) == SLN_CHANGE) || (SHORT2FROMMP(mp1) == SLN_SLIDERTRACK))
               {
                  USHORT   usDeviceID = WinQueryWindowUShort(hWnd, QWS_DEVICEID);
                  ULONG    ulLevel = (ULONG)WinSendMsg(WinWindowFromID(hWnd, SLRD_VOLUME), SLM_QUERYSLIDERINFO, MPFROMLONG(MAKELONG(SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE)), NULL);

                  ULONG          ulError = 0;
                  MCI_SET_PARMS  msp;
                  const ULONG    flParam1 = MCI_WAIT | MCI_SET_AUDIO | MCI_SET_VOLUME;

                  msp.ulLevel = ulLevel;
                  msp.ulAudio = MCI_SET_AUDIO_ALL;

                  mciSendCommand(usDeviceID, MCI_SET, flParam1, (PVOID)&msp, 0);
               }
               break;

            default:
               fHandled = FALSE;
               break;
         }
         break;

      case WM_CLOSE:
         _beginthread(ShutdownThread, NULL, 16384, (void*)hWnd);
         break;

      case MM_MCIPOSITIONCHANGE:
         WinSetPBValue(WinWindowFromID(hWnd, PB_PROGRESS), LONGFROMMP(mp2));
         break;

      case MM_MCINOTIFY:
         mmMCINotify(hWnd, mp1, mp2);
         break;

      case WMU_LOAD:
         mReturn = (MRESULT)mwpLoad(hWnd);
         break;

      case WMU_PLAY:
         mwpPlay(hWnd);
         break;

      default:
         fHandled = FALSE;
   }
   if(!fHandled)
      mReturn = WinDefWindowProc(hWnd, msg, mp1, mp2);

   return mReturn;
}

BOOL mwpCreate(HWND hWnd, MPARAM mp1)
{
   BOOL        fAbort = FALSE;
   PINITDATA   pinitdata = (PINITDATA)mp1;
   ULONG    flSlider = WS_VISIBLE | SLS_HORIZONTAL | SLS_CENTER | SLS_PRIMARYSCALE1 | SLS_HOMELEFT | SLS_BUTTONSLEFT | SLS_SNAPTOINCREMENT | SLS_RIBBONSTRIP | WS_DISABLED;
   ULONG    flSText = WS_VISIBLE | SS_TEXT | DT_VCENTER | DT_RIGHT;
   SLDCDATA sldData;
   PSZ      pszTitle = new char[128];
   HAB      hAB = WinQueryAnchorBlock(hWnd);


   WinCreateWindow(hWnd, WC_PROGRESSBAR, (PSZ)NULL, WS_VISIBLE, 10, 10, 300, 60, hWnd, HWND_TOP, PB_PROGRESS, (PVOID)NULL, (PVOID)NULL);

   WinLoadString(hAB, (HMODULE)NULLHANDLE, IDS_VOLUME, 128, pszTitle);
   WinCreateWindow(hWnd, WC_STATIC, pszTitle,  flSText, 20, 40, 100, 40, hWnd, HWND_TOP, ST_VOLUME, NULL, NULL);

   sldData.cbSize = sizeof(sldData);
   sldData.usScale1Increments = 100;
   sldData.usScale1Spacing = 0;
   WinCreateWindow(hWnd, WC_SLIDER, (PSZ)NULL, flSlider, 20, 40, 100, 40, hWnd, HWND_TOP, SLRD_VOLUME, &sldData, NULL);

   pinitdata->hWnd = hWnd;

   if(_beginthread(StartupThread, NULL, 16386, (void*)pinitdata) == 0)
      fAbort = TRUE;

   delete pszTitle;

   return fAbort;
}

//************************************************************************************************
// Do some paintjob
//************************************************************************************************
void mwpPaint(HWND hWnd)
{
   RECTL rect;
   HPS   hPS = NULLHANDLE;

   hPS = WinBeginPaint(hWnd, NULLHANDLE, &rect);

   WinFillRect(hPS, &rect, CLR_PALEGRAY);

   WinEndPaint(hPS);
}

void mwpSize(HWND hWnd)
{
   RECTL    rect;
   ULONG    fl = SWP_SIZE | SWP_MOVE;

   WinQueryWindowRect(hWnd, &rect);

   WinSetControlRelPos(WinWindowFromID(hWnd, PB_PROGRESS), HWND_TOP, &rect, 0.00, 0.40, 1.00, 1.00, fl, 0, 5, -5, -5);
   WinSetControlRelPos(WinWindowFromID(hWnd, ST_VOLUME),   HWND_TOP, &rect, 0.00, 0.00, 0.10, 0.40, fl, 5, 5,  0, -5);
   WinSetControlRelPos(WinWindowFromID(hWnd, SLRD_VOLUME), HWND_TOP, &rect, 0.10, 0.00, 1.00, 0.40, fl, 0, 5, -5, -5);
}

BOOL mwpLoad(HWND hWnd)
{
   ULONG          ulError = 0;
   MCI_LOAD_PARMS mciLoadParms;
   PSTRINGLIST    pEntry = (PSTRINGLIST)WinQueryWindowPtr(hWnd, QWP_CURRENTENTRY);
   USHORT         usDeviceID = WinQueryWindowUShort(hWnd, QWS_DEVICEID);
   BOOL           fSuccess = TRUE;

   WinSetPBValue(WinWindowFromID(hWnd, PB_PROGRESS), 0);

   memset((void*)&mciLoadParms, 0, sizeof(MCI_LOAD_PARMS));

   mciLoadParms.pszElementName = pEntry->pszString;
   ulError = mciSendCommand(usDeviceID, MCI_LOAD, MCI_WAIT | MCI_OPEN_ELEMENT | MCI_READONLY, (PVOID)&mciLoadParms, (USHORT)0);
   if(LOUSHORT(ulError) != MCIERR_SUCCESS)
      fSuccess = FALSE;

   if(fSuccess)
   {
      MCI_STATUS_PARMS   msp;

      memset((void*)&msp, 0, sizeof(msp));

      msp.ulItem = MCI_STATUS_LENGTH;

      ulError = mciSendCommand(usDeviceID, MCI_STATUS, MCI_WAIT | MCI_STATUS_ITEM, (PVOID)&msp, (USHORT)0);
      if(LOUSHORT(ulError) == MCIERR_SUCCESS)
      {
         WinSetPBLimits(WinWindowFromID(hWnd, PB_PROGRESS), 0, msp.ulReturn);
      }
   }

   if(fSuccess)
   {
      MCI_POSITION_PARMS mppPos;

      memset((void*)&mppPos, 0, sizeof(mppPos));

      mppPos.hwndCallback = hWnd;
      mppPos.ulUnits = MSECTOMM(500);
      ulError = mciSendCommand(usDeviceID, MCI_SET_POSITION_ADVISE, MCI_NOTIFY | MCI_SET_POSITION_ADVISE_ON, (PVOID)&mppPos, (USHORT)0);
      if(LOUSHORT(ulError) != MCIERR_SUCCESS)
         fSuccess = FALSE;
   }

   if(fSuccess)
   {
      PSZ   pszTitle = new char[128];
      MakeWndTitleFromPathName(pszTitle, pEntry->pszString);
      WinSetWindowText(WinQueryWindow(hWnd, QW_PARENT), pszTitle);
      delete pszTitle;
   }

   return fSuccess;
}


BOOL mwpPlay(HWND hWnd)
{
   ULONG          ulError = 0;
   MCI_PLAY_PARMS mciPlayParms;
   USHORT         usDeviceID = WinQueryWindowUShort(hWnd, QWS_DEVICEID);
   BOOL           fSuccess = TRUE;

   memset((void*)&mciPlayParms, 0, sizeof(mciPlayParms));

   mciPlayParms.hwndCallback = hWnd;
   ulError = mciSendCommand(usDeviceID, MCI_PLAY, MCI_NOTIFY, (PVOID)&mciPlayParms, MMUP_PLAY);
   if(LOUSHORT(ulError) != MCIERR_SUCCESS)
      fSuccess = FALSE;

   return fSuccess;
}



//************************************************************************************************
// The allmighty StartupThread
//************************************************************************************************
void _Optlink StartupThread(void *pArg)
{
   AnchorBlock    ab;
   MessageQueue   mq(ab);
   PINITDATA      pinit = (PINITDATA)pArg;
   int            i;
   USHORT         usDeviceIndex = 0;     // default use default deviceindex
   USHORT         usVolume = 75;         // default 75% volume
   ULONG          ulError = 0;           // mci error
   MCI_OPEN_PARMS mciOpenParms;
   BOOL           fDeviceOpen = FALSE;

   //*********************************************************************************************
   // Set the lowest posible priority to make us systemfriendly
   //*********************************************************************************************
   DosSetPriority(PRTYS_THREAD, PRTYC_IDLETIME, PRTYD_MINIMUM, 0);


   //*********************************************************************************************
   // Clear some evetual garbage, just in case....
   //*********************************************************************************************
   WinSetWindowPtr(pinit->hWnd, QWP_PLAYLIST, NULL);
   WinSetWindowPtr(pinit->hWnd, QWP_CURRENTENTRY, NULL);
   WinSetWindowULong(pinit->hWnd, QWL_PLAYLISTENTRIES, 0);
   WinSetWindowUShort(pinit->hWnd, QWS_DEVICEID, 0);
   WinSetWindowULong(pinit->hWnd, QWL_LOOP, 0);   // do not loop by default


   //*********************************************************************************************
   // Parse the commandline
   //*********************************************************************************************
   for(i = 1; i < pinit->argc; i++)
   {
      switch(pinit->argv[i][0])
      {
         case '/':
         case '-':
            switch(pinit->argv[i][1])
            {
               case 'i':
               case 'I':
                  usDeviceIndex = (USHORT)atoi(&pinit->argv[i][2]);
                  break;

               case 'l':
               case 'L':
                  WinSetWindowULong(pinit->hWnd, QWL_LOOP, 1);
                  break;

               case 'v':
               case 'V':
                  usVolume = (USHORT)atoi(&pinit->argv[i][2]);
                  break;
            }
            break;

         default:
            AddToPlaylist(pinit->hWnd, pinit->argv[i]);
            break;
      }
   }

   // make sure we're at the first entry of the playlist
   WinSetWindowPtr(pinit->hWnd, QWP_CURRENTENTRY, WinQueryWindowPtr(pinit->hWnd, QWP_PLAYLIST));


   //*********************************************************************************************
   // open the waveaudio device
   //*********************************************************************************************
   memset((void*)&mciOpenParms, 0, sizeof(mciOpenParms));

   mciOpenParms.pszDeviceType = (PSZ)MAKEULONG(MCI_DEVTYPE_WAVEFORM_AUDIO, usDeviceIndex);
   ulError = mciSendCommand((USHORT)0, MCI_OPEN, MCI_WAIT | MCI_OPEN_TYPE_ID, (PVOID)&mciOpenParms, 0);

   if(LOUSHORT(ulError) == MCIERR_SUCCESS)
   {
      fDeviceOpen = TRUE;
      WinSetWindowUShort(pinit->hWnd, QWS_DEVICEID, mciOpenParms.usDeviceID);
   }

   if((fDeviceOpen == TRUE) && (WinQueryWindowULong(pinit->hWnd, QWL_PLAYLISTENTRIES) != 0))
   {
      MCI_SET_PARMS        msp;

      msp.ulLevel = usVolume;
      msp.ulAudio = MCI_SET_AUDIO_ALL;
      ulError = mciSendCommand(mciOpenParms.usDeviceID, MCI_SET, MCI_WAIT | MCI_SET_AUDIO | MCI_SET_VOLUME, (PVOID)&msp, 0);

      if(LOUSHORT(ulError) == MCIERR_SUCCESS)
      {
         WinSendMsg(WinWindowFromID(pinit->hWnd, SLRD_VOLUME), SLM_SETSLIDERINFO, MPFROM2SHORT(SMA_SLIDERARMPOSITION, SMA_INCREMENTVALUE), MPFROMSHORT(usVolume));
         WinEnableWindow(WinWindowFromID(pinit->hWnd, SLRD_VOLUME), TRUE);
      }

      if((BOOL)WinSendMsg(pinit->hWnd, WMU_LOAD, NULL, NULL))
      {
         WinSendMsg(pinit->hWnd, WMU_PLAY, NULL, NULL);
      }
   }


   //*********************************************************************************************
   // free the initdata buffer
   //*********************************************************************************************
   delete pinit;

   // show the window
   SWP   swp;
   ULONG pulBufferMax = sizeof(swp);
   if(PrfQueryProfileData(HINI_USERPROFILE, "WavePlayer", "WindowPosition", (PVOID)&swp, &pulBufferMax))
      WinSetWindowPos(WinQueryWindow(pinit->hWnd, QW_PARENT), HWND_TOP, swp.x, swp.y, swp.cx, swp.cy, SWP_SHOW | SWP_MOVE | SWP_SIZE);
   else
      WinShowWindow(WinQueryWindow(pinit->hWnd, QW_PARENT), TRUE);

   // bye, bye!
   _endthread();
}



//************************************************************************************************
// The not so mighty ShutdownThread
//************************************************************************************************
void _Optlink ShutdownThread(void *pArg)
{
   HWND           hWnd = (HWND)pArg;
   AnchorBlock    ab;
   MessageQueue   mq(ab);

   ULONG            ulError = 0;
   MCI_STATUS_PARMS msp;
   MCI_GENERIC_PARMS mciGenericParms;
   USHORT           usDeviceID = WinQueryWindowUShort(hWnd, QWS_DEVICEID);

   //*********************************************************************************************
   // Set the lowest posible priority to make us systemfriendly
   //*********************************************************************************************
   DosSetPriority(PRTYS_THREAD, PRTYC_IDLETIME, PRTYD_MINIMUM, 0);


   //*********************************************************************************************
   // Store windowpositoin
   //*********************************************************************************************
   SWP   swp;
   WinQueryWindowPos(WinQueryWindow(hWnd, QW_PARENT), &swp);
   PrfWriteProfileData(HINI_USERPROFILE, "WavePlayer", "WindowPosition", (PVOID)&swp, sizeof(swp));

   memset((void*)&msp, 0, sizeof(msp));

   msp.ulItem = MCI_STATUS_MODE;
   ulError = mciSendCommand(usDeviceID, MCI_STATUS, MCI_WAIT | MCI_STATUS_ITEM, (PVOID)&msp, (USHORT)0);

   mciGenericParms.hwndCallback = NULL;

   if(msp.ulReturn == MCI_MODE_PLAY)
      mciSendCommand(usDeviceID, MCI_STOP, MCI_WAIT, (PVOID)&mciGenericParms, NULL);

   mciSendCommand(usDeviceID, MCI_CLOSE, MCI_WAIT, (PVOID)&mciGenericParms, (USHORT)0);

   plNuke(hWnd);

   WinPostMsg(hWnd, WM_QUIT, NULL, NULL);

   _endthread();
}


void AddToPlaylist(HWND hWnd, PSZ pszFilespec)
{
   HDIR           hFind          = HDIR_SYSTEM;
   FILEFINDBUF3   FindBuffer     = {0};
   ULONG          ulResultBufLen = sizeof(FILEFINDBUF3);
   ULONG          ulFindCount    = 1;
   APIRET         rc             = NO_ERROR;
   ULONG          flAtttribute   = FILE_ARCHIVED | FILE_SYSTEM | FILE_HIDDEN | FILE_READONLY;
   PSZ            pszFilepath = new char[CCHMAXPATH];
   PSZ            pTemp = NULL;

   MakePathFromFilepath(pszFilepath, pszFilespec);
   pTemp = pszFilepath + strlen(pszFilepath);

   rc = DosFindFirst(pszFilespec, &hFind, flAtttribute, &FindBuffer, ulResultBufLen, &ulFindCount, FIL_STANDARD);
   while(rc == NO_ERROR)
   {
      strcpy(pTemp, FindBuffer.achName);
      if(IsValidFile(pszFilepath))
      {
         plAddEntry(hWnd, pszFilepath);
         WinSetWindowText(WinQueryWindow(hWnd, QW_PARENT), pTemp);
      }
      ulFindCount = 1;
      rc = DosFindNext(hFind, &FindBuffer, ulResultBufLen, &ulFindCount);
   }
   DosFindClose(hFind);

   delete pszFilepath;
}

void MakePathFromFilepath(PSZ pszPath, PSZ pszFilepath)
{
   char *p = pszPath;

   strcpy(pszPath, pszFilepath);

   while(*p)
      p++;

   while(*p != '\\')
   {
      // file is in current drectory
      if(p == pszPath)
      {
         strcpy(pszPath, ".\\");
         return;
      }
      p--;
   }
   p++;
   *p = '\0';
}

//************************************************************************************************
// "Playlist API"
//************************************************************************************************
void plAddEntry(HWND hWnd, PSZ pszFilename)
{
   PSTRINGLIST pFirstNode = (PSTRINGLIST)WinQueryWindowPtr(hWnd, QWP_PLAYLIST);
   PSTRINGLIST pList = (PSTRINGLIST)WinQueryWindowPtr(hWnd, QWP_CURRENTENTRY);
   ULONG       cCount = WinQueryWindowULong(hWnd, QWL_PLAYLISTENTRIES);

   if(pFirstNode == NULL)
   {
      // allocate node and increase nodecounter
      pFirstNode = new STRINGLIST;
      cCount++;

      // link it to itself (ever tried biting your own tail?)
      pFirstNode->next = pFirstNode;
      pFirstNode->prev = pFirstNode;

      // allocate stringbuffer in node and copy string to node
      pFirstNode->pszString = new char[strlen(pszFilename)+1];
      strcpy(pFirstNode->pszString, pszFilename);

      // set some nice pointers
      WinSetWindowPtr(hWnd, QWP_PLAYLIST, (PVOID)pFirstNode);
      WinSetWindowPtr(hWnd, QWP_CURRENTENTRY, (PVOID)pFirstNode);
      WinSetWindowULong(hWnd, QWL_PLAYLISTENTRIES, cCount);

      // go no further
      return;
   }

   // allocate the new node & increase the nodecounter
   pList->next = new STRINGLIST;
   cCount++;

   // set the new nodes previous-pointer to the current node (are we confused yet?)
   pList->next->prev = pList;

   // move to the next node
   pList = pList->next;

   // allocate stringbuffer in node and copy string to node
   pList->pszString = new char[strlen(pszFilename)+1];
   strcpy(pList->pszString, pszFilename);

   // link it to the first node in the list
   pList->next = pFirstNode;

   // link the first node's previous-pointer to the current node, since it is the last one (hello?)
   pFirstNode->prev = pList;

   // finish it all off with a big party!
   WinSetWindowPtr(hWnd, QWP_CURRENTENTRY, (PVOID)pList);
   WinSetWindowULong(hWnd, QWL_PLAYLISTENTRIES, cCount);
}

void plNext(HWND hWnd)
{
   PSTRINGLIST pEntry = (PSTRINGLIST)WinQueryWindowPtr(hWnd, QWP_CURRENTENTRY);
   pEntry = pEntry->next;
   WinSetWindowPtr(hWnd, QWP_CURRENTENTRY, pEntry);
}

void plNuke(HWND hWnd)
{
   PSTRINGLIST pFirst = (PSTRINGLIST)WinQueryWindowPtr(hWnd, QWP_PLAYLIST);
   PSTRINGLIST pEntry = pFirst;
   PSTRINGLIST pTmp = pEntry;

   if(pFirst == NULL)
      return;

   do
   {
      pEntry = pEntry->next;

      delete pTmp->pszString;
      delete pTmp;

      pTmp = pEntry;

      DosSleep(0);
   }while(pTmp != pFirst);

   // reset playlist info
   WinSetWindowPtr(hWnd, QWP_PLAYLIST, (PVOID)NULL);
   WinSetWindowPtr(hWnd, QWP_CURRENTENTRY, (PVOID)NULL);
   WinSetWindowULong(hWnd, QWL_PLAYLISTENTRIES, 0);
}




void mmMCINotify(HWND hWnd, MPARAM mp1, MPARAM mp2)
{
   USHORT usNotifyCode = SHORT1FROMMP(mp1);
   USHORT usUserParam  = SHORT2FROMMP(mp1);

   switch(SHORT2FROMMP(mp2))
   {
      case MCI_PLAY:
         switch(usNotifyCode)
         {
            case MCI_NOTIFY_SUCCESSFUL:
               switch(usUserParam)
               {
                  /* Normal play completed */
                  case MMUP_PLAY:
                     PlayCompleted(hWnd);
                     break;
               }
               break;
         }
         break;
   }
}

void PlayCompleted(HWND hWnd)
{
   PSTRINGLIST pFirst = (PSTRINGLIST)WinQueryWindowPtr(hWnd, QWP_PLAYLIST);
   PSTRINGLIST pList = (PSTRINGLIST)WinQueryWindowPtr(hWnd, QWP_CURRENTENTRY);

   if((pList->next == pFirst) && (WinQueryWindowULong(hWnd, QWL_LOOP) == 0))
   {
      WinSendMsg(hWnd, WM_CLOSE, NULL, NULL);
      return;
   }
   pList = pList->next;
   WinSetWindowPtr(hWnd, QWP_CURRENTENTRY, pList);

   if((BOOL)WinSendMsg(hWnd, WMU_LOAD, NULL, NULL))
   {
      WinSendMsg(hWnd, WMU_PLAY, NULL, NULL);
   }
}




BOOL IsValidFile(PSZ pszFilename)
{
   HMMIO          hMMIO = NULL;
   MMIOINFO       mmioInfo;

   memset((PVOID)&mmioInfo, 0, sizeof(MMIOINFO));

   if((hMMIO = mmioOpen(pszFilename, &mmioInfo, MMIO_READ)) == NULL)
      return FALSE;

   mmioClose(hMMIO, 0);

   return TRUE;
}


void MakeWndTitleFromPathName(PSZ pszTitle, PCSZ pszPathName)
{
   PCSZ  p = pszPathName;
   PSZ   p2 = pszTitle;

   // goto end of string
   while(*p)
      p++;

   // step back to first backslash
   while(*p != '\\')
   {
      // abort if we're at the beginning of the string
      if(p == pszPathName)
         break;
      p--;
   }
   if(*p == '\\')
      p++;

   strcpy(pszTitle, p);

   // remove extension
   while(*p2)
      p2++;
   while(*p2 != '.')
   {
      if(p2 == pszPathName)
      {
         while(*p2)
            p2++;
         break;
      }
      p2--;
   }
   *p2 = '\0';
}



BOOL WinSetControlRelPos(HWND hWnd, HWND hwndInsertBehind, PRECTL rect, float X, float Y, float XX, float YY, ULONG fl, LONG x1a, LONG y1a, LONG x2a, LONG y2a)
{
   ULONG x = ULONG(float(rect->xRight)*X);
   ULONG y = ULONG(float(rect->yTop)*Y);
   ULONG x2 = ULONG(float(rect->xRight)*XX);
   ULONG y2 = ULONG(float(rect->yTop)*YY);

   return WinSetWindowPos(hWnd, hwndInsertBehind, x+x1a, y+y1a, ((x2-x)+x2a)-x1a, ((y2-y)+y2a)-y1a, fl);
}
