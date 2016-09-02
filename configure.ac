AC_INIT([libsmbusb], [1.0.1], [contact@karosium.e4ward.com])

AM_INIT_AUTOMAKE([foreign])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

LT_INIT
PKG_PROG_PKG_CONFIG([0.22])

SMB_LIB_VERSION_CURRENT=1
SMB_LIB_VERSION_REVISION=1
SMB_LIB_VERSION_AGE=0
SMB_LIB_VERSION="$SMB_LIB_VERSION_CURRENT:$SMB_LIB_VERSION_REVISION:$SMB_LIB_VERSION_AGE"
SMB_LIB_LDFLAGS="-version-info $SMB_LIB_VERSION"
AC_SUBST(SMB_LIB_VERSION_CURRENT)
AC_SUBST(SMB_LIB_VERSION_REVISION)
AC_SUBST(SMB_LIB_VERSION_AGE)
AC_SUBST(SMB_LIB_VERSION)
AC_SUBST(SMB_LIB_LDFLAGS)

test "$prefix" = NONE && prefix=/usr/local

# require libusb
PKG_CHECK_MODULES([libusb], [libusb-1.0 >= 1.0.5],
			[CFLAGS="$CFLAGS $libusb_CFLAGS";			
			LIBS="$LIBS $libusb_LIBS"])
AC_CHECK_LIB(usb-1.0, libusb_init)
#PKG_CHECK_MODULES([usb], [libusb-1.0 >= 1.0.9])

# require common stuff
AC_HEADER_STDC
AC_C_CONST
AC_TYPE_SIZE_T

# settings

SMB_CONF_DIRS=lib

AC_ARG_ENABLE([firmware],
  AS_HELP_STRING([--disable-firmware], [Don't build the firmware, use pre-built one]))

  AS_IF([test "x$enable_firmware" != "xno"], [
    AC_CHECK_PROG(xxd_found, xxd, yes)
    if test "$xxd_found" != yes ;then
            AC_MSG_ERROR("xxd is required for building the firmware")   
    fi
    AC_CHECK_PROG(sdcc_found, sdcc, yes)
    if test "$sdcc_found" != yes ;then
        AC_MSG_ERROR("SDCC is required for building the firmware")
    else
	has8051=`sdcc --version 2>&1 | grep "mcs51"`
	if test "$has8051" == "" ;then
		AC_MSG_ERROR("Installed SDCC doesn't support 8051 so can't built for EZUSB")
	fi
    fi
    
    SMB_CONF_DIRS="firmware $SMB_CONF_DIRS"
])

AC_ARG_ENABLE([tools],
    AS_HELP_STRING([--disable-tools], [Don't build the tools]))

AS_IF([test "x$enable_tools" != "xno"], [
  SMB_CONF_DIRS="$SMB_CONF_DIRS tools"
  AC_CONFIG_FILES([tools/Makefile])
])

AC_SUBST(SMB_CONF_DIRS)

AC_CONFIG_FILES([lib/Makefile		 
		 lib/libsmbusb.pc
		 Makefile])

AC_MSG_NOTICE(-------------------------------)
AC_MSG_NOTICE([Build: $build])
AC_MSG_NOTICE([Host: $host])
AC_MSG_NOTICE([Compiler: $CC])
AC_MSG_NOTICE([Prefix: $prefix])

AS_IF([test "x$enable_firmware" == "xno"], [
  AC_CONFIG_COMMANDS([firmwarecopy],[cp ${srcdir}/firmware/firmware.h ${srcdir}/lib/])
  AC_MSG_NOTICE(* Using pre-built firmware)
])
AS_IF([test "x$enable_tools" == "xno"], [
  AC_MSG_NOTICE(* Tools will not be built)
])
AC_MSG_NOTICE(-------------------------------)

AC_OUTPUT


