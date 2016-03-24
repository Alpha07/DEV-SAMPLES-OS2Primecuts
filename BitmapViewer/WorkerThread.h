typedef struct _WRKRPARAMS
{
   HWND hwndAppFrame;
   HWND hwndCanvas;
   int argc;
   char **argv;
}WRKRPARAMS, *PWRKRPARAMS;




/*
 * Worker thread messages
 * These are posted *to* the worker thread
 */
#define WTMSG_LOAD_BITMAP                WM_USER+1
#define WTMSG_SET_HORZ_ALIGNMENT         WM_USER+2
#define WTMSG_SET_VERT_ALIGNMENT         WM_USER+3


/*
 * Outgoing worker thread messages
 * These are sent/posted FROM the worker thread
 */
/*
#define WMU_WORKERTHREAD_NOTIFICATION    WM_USER+0x1000
#define INIT_HMQ                         1
*/
