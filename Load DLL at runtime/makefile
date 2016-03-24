.SUFFIXES: .c .obj

CCFLAGS = /Gd+ /Gm- /Ti- /Wall+ppc-ppt-

CC = icc.exe /C $(CCFLAGS) /Q

.c.obj:
       $(CC) %s

all: Test.dll TestExe.exe

TestDLL.obj: TestDLL.c

TestExe.obj: TestExe.c


Test.dll: TestDLL.obj TestDLL.def
       ilink /nologo @<<
       /noinformation /out:Test
       TestDLL.obj
       TestDLL.def
<<

TestExe.exe: TestExe.obj TestExe.def
       ilink /nologo @<<
       /noinformation /out:TestExe
       TestExe.obj
       TestExe.def
<<

