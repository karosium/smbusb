ACLOCAL_AMFLAGS = -I autostuff

lib_LTLIBRARIES = libsmbusb.la

libsmbusb_la_SOURCES = \
	smbusb.c \
	libsmbusb.h \
	fxloader.c \
	firmware.h

libsmbusb_la_CFLAGS = $(SMB_LIB_CFLAGS)
libsmbusb_la_LDFLAGS = $(SMB_LIB_LDFLAGS)

libsmbusb_includedir=$(includedir)
libsmbusb_include_HEADERS = libsmbusb.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libsmbusb.pc

all: 
	cp ../firmware/firmware.h .