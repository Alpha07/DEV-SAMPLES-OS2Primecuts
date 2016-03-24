.SUFFIXES:
.SUFFIXES: .c .rc .obj .h .exe .res

.SILENT:

BASENAME=FrameWindowTricks

BACKUP_FILES = *.c *.h *.rc *.ico makefile *.def


all: $(BASENAME).exe

OBJS = $(BASENAME).obj \
ClientWindow.obj \
StatusBarWindow.obj


# terminal - launch debug terminal, include debug code
# yes - include debug code
# no - no debug code, no debug terminal
!IFNDEF DEBUG
DEBUG = no
!ENDIF

release:
   nmake -nologo DEBUG=no

debug:
   nmake -nologo DEBUG=yes

debugterm:
   nmake -nologo DEBUG=terminal


clean:
   del *.obj
   del *.exe
   del *.res
   del *.map
   del *.sym



.c.obj:
!IFDEF DEBUG
!  IF "$(DEBUG)" == "yes"
       @icc /C /Gd+ /Gm+ /Ti+ /O- /Q+ /I. /DDEBUG /Wall+ppc-ppt- %s
!  ELIF "$(DEBUG)" == "terminal"
       @icc /C /Gd+ /Gm+ /Ti+ /O- /Q+ /I. /DDEBUG /DDEBUG_TERM /Wall+ppc-ppt- %s
!  ELSE
       @icc /C /Gd+ /Gm+ /G5 /Gi+ /Gf+ /Gu+ /Ti- /O+ /Q+ /I. /Wall+ppc-ppt- %s
!  ENDIF
!ELSE
!    ERROR Macro named DEBUG is not defined.
!ENDIF

.rc.res:
   rc -n -r %s $(@B).res


$(BASENAME).obj: $(@B).c resources.h StatusbarWindow.h

ClientWindow.obj: $(@B).c resources.h StatusbarWindow.h

StatusbarWindow.obj: $(@B).c $(@B).h


$(BASENAME).res: $(@B).rc resources.h $(BASENAME).ico


$(BASENAME).exe: $(OBJS) $(@B).def $(@B).res
   ilink /nologo @<<
!if "$(DEBUG)" == "yes" || "$(DEBUG)" == "terminal"
       /noinformation /exec /out:$(@B) /map:$(@B) /debug
!else
       /noinformation /exec /out:$(@B) /map:$(@B) /exepack:2 /nodebug /optfunc /packcode /packdata
!endif
      $(OBJS)
      $(@B).def
   <<
!if "$(DEBUG)" == "yes" || "$(DEBUG)" == "terminal"
   rc -n $(@B).res $@
!else
   rc -n -x2 $(@B).res $@
!endif
   dllrname /Q /R $@ cppom30=os2om30
   mapsym -n $(@B).map
   @detach pkzip /add /silent last_successful_build $(BACKUP_FILES)

