.SUFFIXES:
.SUFFIXES: .c .rc .obj .h .exe .res

.SILENT:

BASENAME=BloatCom

all: $(BASENAME).dll $(BASENAME).lib

SRCDIR=..

{..}.c.obj:
   @icc /C /Gd+ /Gm+ /Ge- /Gi+ /G5 /Ti- /O+ /Q+ /I$(SRCDIR) /Wall+ppc-ppt- %s

!include ..\makeglobals

$(BASENAME).dll: $(OBJS) ..\$(@B).def
   ilink /nologo @<<
      /noinformation /dll /out:$(@B) /map:$(@B) /exepack:2 /nodebug /optfunc /packcode /packdata
      $(OBJS)
      ..\$(@B).def
   <<
   dllrname /Q /R $@ cppom30=os2om30
   %do successful_build
   %cd release

$(BASENAME).lib: $(@B).dll
   implib /nologo $@ $(@B).dll

clean:
   del *.obj
   del *.dll
   del *.map
   del *.lib

