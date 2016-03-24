#include <os2.h>

#pragma export(func,,1)
long APIENTRY func(char chVal)
{
   return 100*chVal;
}
