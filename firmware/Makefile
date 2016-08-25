FX2LIBDIR= ../fx2lib
BASENAME = smbusb_firmware
SOURCES= smbusb_firmware.c
A51_SOURCES= dscr.a51

include $(FX2LIBDIR)/lib/fx2.mk

all: $(BUILDDIR)/$(BASENAME).ihx 
	xxd -i $(BUILDDIR)/$(BASENAME).ihx firmware.h
	cp firmware.h ../lib/firmware.h

install:

distclean:
	-rm -rf $(BUILDDIR)
clean:
	-rm -rf $(BUILDDIR)