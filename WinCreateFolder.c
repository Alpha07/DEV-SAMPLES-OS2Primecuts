#define INCL_WINWORKPLACE

#include <os2.h>

void main(void)
{
    WinCreateObject("WPFolder", "A Folder on the desktop", "OBJECTID=<A_TEST_FOLDER>", "<WP_DESKTOP>",    CO_UPDATEIFEXISTS);
    WinCreateObject("WPFolder", "A Folder in a folder",    "OBJECTID=<TEST_FOLDER_2>", "<A_TEST_FOLDER>", CO_UPDATEIFEXISTS);
}
