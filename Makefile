PASS =		tcfggen

OBJS =		tcfggen.o lcugen.o suif_pass.o
MAIN_OBJ =	suif_main.o
CPPS =		$(OBJS:.o=.cpp) $(MAIN_OBJ:.o=.cpp)
HDRS =		tcfggen.h lcugen.h suif_pass.h

NWHDRS =
NWCPPS =

LIBS =		-lmachine -lcfg -lcfa -lsuifrm

include $(MACHSUIFHOME)/Makefile.common
