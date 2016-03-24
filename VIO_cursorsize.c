#define INCL_VIO

#include <os2.h>

#include <conio.h>

void main(void)
{
   VIOCURSORINFO  CursorData;

   SHORT   start;
   SHORT   end;
   short   chr;

   VioGetCurType(&CursorData, (HVIO)0);
   start = CursorData.yStart;
   end = CursorData.cEnd;

   while(chr != 27)
   {
      chr = getch();
      if(chr == 'q') start--;
      if(chr == 'a') start++;
      if(chr == 'w') end--;
      if(chr == 's') end++;
      CursorData.yStart = start;
      CursorData.cEnd = end;
      VioSetCurType(&CursorData, (HVIO)0);
      if(chr == 13)
          cprintf("start: %d\n\r  end: %d\n\r", start, end);
   }
}
