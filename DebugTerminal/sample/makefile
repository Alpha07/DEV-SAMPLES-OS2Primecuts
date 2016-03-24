.SUFFIXES:
.SUFFIXES: .c .obj .h .rc .res .def .exe

# Don't echo commands
.SILENT:

BASENAME = TestApplication


# terminal - include debug code and debug terminal
# yes - include debug code
# no - no debug code, fully optimized
DEBUG = terminal

!if "$(DEBUG)" == "yes" || "$(DEBUG)" == "terminal"
CC_DBG = /Ti+ /Tx+ /O-
!if "$(DEBUG)" == "terminal"
CC_TERM = /DDEBUG
!endif
!else
CC_DBG = /Ti- /O+ /G5 /Gi+ /Gf+
!endif


.c.obj:
   icc /C /Gd+ /Gm+ $(CC_DBG) $(CC_TERM) /Q+ /Wall+ppc-ppt- %s


#.c.obj:
#!IFDEF DEBUG
#!  IF "$(DEBUG)" == "yes"
#       @icc /C /Gd+ /Gm+ /Ti+ /Tx+ /O- /Q+ /Wall+ppc-ppt- %s
#!  ELIF "$(DEBUG)" == "terminal"
#       @icc /C /Gd+ /Gm+ /Ti+ /Tx+ /O- /Q+ /DDEBUG /Wall+ppc-ppt- %s
#!  ELSE
#       @icc /C /Gd+ /Gm+ /G5 /Gi+ /Gf+ /Ti- /O+ /Q+ /Wall+ppc-ppt- %s
#!  ENDIF
#!ELSE
#!  ERROR DEBUG macro is not defined.
#!ENDIF


.rc.res:
       @rc -n -r -i . %s

OBJS = $(BASENAME).obj ClientWindow.obj

all: $(BASENAME).exe

$(BASENAME).obj: $(@B).c resource.h

ClientWindow.obj: $(@B).c resource.h

$(BASENAME).res: $(@B).rc resource.h $(@B).ico

$(BASENAME).exe: $(OBJS) $(@B).res $(@B).def
   ilink /nologo @<<
   /noinformation /exec /out:$(@B) /map:$(@B)
!if "$(DEBUG)" == "yes" || "$(DEBUG)" == "terminal"
   /debug
!elif "$(DEBUG)" == "no"
   /exepack:2 /nodebug /optfunc
!else
!error DEBUG macro error.
!endif
   $(OBJS)
   $(@B).def
<<
   rc -x2 -n $(@B).res $@
   dllrname /Q /R $@ cppom30=os2om30
   pkzip /add /silent last_successful_build *.c *.h *.rc *.def makefile



clean:
   del *.obj
   del *.res
   del *.exe
   del *.map
   del last_successful_build.zip

