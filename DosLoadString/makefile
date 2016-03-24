.SUFFIXES: .c .obj .h

CFLAGS = /Gd+ /Gm- /Wall+ppc-ppt-

CC = icc.exe /C $(CFLAGS) /Q+


.c.obj:
       $(CC) %s

OBJS = DosLoadString.obj

all: Test.exe

DosLoadString.obj: DosLoadString.c res.h

Test.exe: $(OBJS) Test.def
       ilink /nologo @<<
       /noinformation /out:Test /map:Test
       $(OBJS)
       Test.def
<<
       rc Test.rc test.exe
