TARGET = PSPYouTube

OBJS = main.o youtube_raw.o

CFLAGS = -O2 -G0 -Wall
CXXFLAGS = $(CFLAGS)
ASFLAGS = $(CFLAGS)

LIBS = -lpspgu -lpspdisplay -lpspctrl -lpspsdk

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = PSP YouTube 0.1

PSPSDK = $(shell psp-config --pspsdk-path)

include $(PSPSDK)/lib/build.mak

youtube_raw.o: assets/youtube.raw
	psp-objcopy -I binary -O elf32-littlemips -B mips \
		--rename-section .data=.rodata,alloc,load,readonly,data,contents \
		assets/youtube.raw youtube_raw.o