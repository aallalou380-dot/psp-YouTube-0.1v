TARGET = PSPYouTube

OBJS = main.o

CXX = psp-g++

CXXFLAGS = -O2 -G0 -Wall -fno-exceptions -fno-rtti

LIBS = \
	-lintrafont \
	-lpspgu \
	-lpspdisplay \
	-lpspctrl \
	-lpspsdk

EXTRA_TARGETS = EBOOT.PBP

PSP_EBOOT_TITLE = PSP YouTube 
PSP_EBOOT_ICON = ICON0.PNG

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak