.SUFFIXES: .c .rc .obj .res .h

.SILENT:

BACKUP_FILES = *.c makefile *.def *.h *.rc

BASENAME = BitmapViewer

# yes - include debug code
# no - no debug code
!IFNDEF DEBUG
DEBUG = no
!ENDIF

all: $(BASENAME).exe

debug:
   nmake -nologo DEBUG=yes

.c.obj:
!IFDEF DEBUG
!  IF "$(DEBUG)" == "yes"
      @icc /C /Gd+ /Gm+ /Ti+ /O- /Q+ /I. /DDEBUG_TERM /Wall+ppc-ppt- %s
!  ELSE
      @icc /C /Gd+ /Gm+ /G5 /Gi+ /Ti- /O+ /Q+ /I. /Wall+ppc-ppt- %s
!  ENDIF
!ENDIF

.rc.res:
   @rc -n -r %s

OBJS = $(BASENAME).obj \
CanvasWindow.obj \
FrameSubProc.obj \
WorkerThread.obj \
PixelBuffer.obj

$(BASENAME).obj: $(@B).c CanvasWindow.h resources.h

CanvasWindow.obj: $(@B).c $(@B).h resources.h

FrameSubProc.obj: $(@B).c $(@B).h resources.h

WorkerThread.obj: $(@B).c $(@B).h CanvasWindow.h FrameSubProc.h resources.h

PixelBuffer.obj:  $(@B).c $(@B).h

$(BASENAME).res:  $(@B).rc $(@B).ico resources.h


$(BASENAME).exe: $(OBJS) $(@B).def $(@B).res makefile
   @ilink /nologo @<<
!if "$(DEBUG)" == "yes"
   /noinformation /exec /out:$(@B) /map:$(@B) /debug
!else
   /noinformation /exec /out:$(@B) /map:$(@B) /exepack:2 /nodebug /optfunc
!endif
   $(OBJS)
   $(@B).def
<<
!if "$(DEBUG)" == "yes"
   rc -n $(@B).res $@
!else
   rc -n -x2 $(@B).res $@
!endif
   @dllrname /Q /R $(@F) CPPOM30=OS2OM30
   @mapsym -n $(@B).map
   @detach pkzip /add /silent last_successful_build $(BACKUP_FILES)

clean:
   del *.obj
   del *.res
   del $(BASENAME).exe
   del $(BASENAME).map
   del $(BASENAME).sym

