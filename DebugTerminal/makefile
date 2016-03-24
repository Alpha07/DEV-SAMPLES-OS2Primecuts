.SUFFIXES: .c .obj .h .rc .res

.SILENT:

BASENAME=DebugTerminal

BACKUP_FILES = *.c *.h makefile *.def *.ico *.rc

.c.obj:
   icc /C /Gm+ /Gd+ /G5 /Ti- /O+ /I. /Wall+ppc-ppt- /Q+ $<

.rc.res:
   rc -n -r $<

all: $(BASENAME).exe

OBJS = DebugTerminal.obj TerminalWindow.obj

DebugTerminal.obj: $(@B).c res.h TerminalWindow.h

TerminalWindow.obj: $(@B).c $(@B).h res.h

$(BASENAME).res: $(@B).rc res.h $(@B).ico


$(BASENAME).exe: $(OBJS) $(@B).res $(@B).def
   ilink /nologo @<<
      /optfunc /nodebug /out:$(@B) /map:$(@B) /exepack:2
      $(OBJS)
      $(@B).def
   <<
   rc -n -x2 $(@B).res $@
   dllrname /Q /R $@ cppom30=os2om30
#   @detach pkzip /add /silent last_successful_build $(BACKUP_FILES)
#   @detach pkzip /add /silent d:\Backup\Development\$(BASENAME)_successful_build_$(DATE) $(BACKUP_FILES)
#   copy $@ c:\Utils\



clean:
   del *.obj
   del *.res
   del *.exe
   del *.map
#   del last_successful_build.zip

