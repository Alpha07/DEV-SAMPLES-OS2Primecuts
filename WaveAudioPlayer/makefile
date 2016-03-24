.SUFFIXES:
.SUFFIXES: .rc .res .obj .lst .cpp

CCOPTS     = /c /Gd+ /G5 /ss /Gm+ /O+ /Ti- /Q+ /Wallppc-ppt-vft- /I$(CLPATH)

CC         = icc $(CCOPTS)

.cpp.lst:
    $(CC) -Fc$*.lst -Fo$*.obj $*.cpp

.cpp.obj:
    $(CC) -Fo$*.obj $*.cpp

.rc.res:
    rc -r -n $*.rc


OBJS = main.obj progressbar.obj
HEADERS = res.h progressbar.h

all: WavePlayer.Exe

WavePlayer.res: WavePlayer.rc res.h WavePlayer.Ico
        rc -r -n WavePlayer.rc

main.obj: main.cpp $(HEADERS)
progressbar.obj: progressbar.cpp $(HEADERS)

WavePlayer.exe: $(OBJS) WavePlayer.def WavePlayer.res
    icc.exe @<<
     /B" /nologo /exepack:2 /pmtype:pm /packd"
     /FeWavePlayer.Exe 
     /FmWavePlayer.Map 
     $(OBJS) 
     WavePlayer.def 
     mmpm2.lib
<<
   rc -p -x -n WavePlayer.res WavePlayer.exe
   dllrname /Q /R WavePlayer.Exe CPPOM30=OS2OM30
   copy WavePlayer.Exe d:\Applications\WavePlayer

