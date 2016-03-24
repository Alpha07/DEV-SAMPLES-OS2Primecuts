.SUFFIXES:
.SUFFIXES: .c .rc .obj .h .exe .res

.SILENT:

BASENAME=generate_filelist
BACKUP_FILES=*.c makefile *.def

all: $(BASENAME).exe

.c.obj:
   icc /C /Gd+ /Gm+ /G5 /Gi+ /Gf+ /Ti- /O+ /Q+ /I. /Wall+ppc-ppt- %s



OBJS=generate_filelist.obj

$(BASENAME).obj: $(@B).c

generate_filelist.exe: $(OBJS) $(@B).def
   ilink /nologo @<<
      /noinformation /exec /out:$(@B) /map:$(@B) /exepack:2 /nodebug /optfunc
      $(OBJS)
      $(@B).def
   <<
   dllrname $@ cppom30=os2om30
   copy $@ d:\TS_BBS
   @detach pkzip /add /silent last_successful_build $(BACKUP_FILES)
   @detach pkzip /add /silent e:\Backup\Development\$(BASENAME)_successful_build_$(DATE) $(BACKUP_FILES)

clean:
   del *.obj
   del *.exe
   del *.map
