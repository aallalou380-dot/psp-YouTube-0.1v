TARGET = PSPYouTube

OBJS = main.o

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

LIBS = -lpspdebug -lpspdisplay -lpspctrl -lpspsdk

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP YouTube 0.1

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak
