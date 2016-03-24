#define WC_TERMINALWINDOW                "TermWndClass"

typedef struct _TERMCDATA
{
   USHORT cb;
   USHORT rows;
   USHORT columns;
   HFILE hDebugPipe;
   BOOL fLogFile;
   HFILE hfLogFile;
}TERMCDATA, *PTERMCDATA;

/*
 * Exported functions
 */
BOOL _Optlink registerTerminalWindow(HAB hAB);

