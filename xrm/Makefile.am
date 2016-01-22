
MAINTAINERCLEANFILES = Makefile.in

lib_LTLIBRARIES = libxcb-xrm.la

xcbinclude_HEADERS = xcb_xrm.h

AM_CFLAGS = $(CWARNFLAGS)

libxcb_xrm_la_SOURCES = xrm.c entry.c
libxcb_xrm_la_CPPFLAGS = $(XCB_CFLAGS) $(XPROTO_CFLAGS)
libxcb_xrm_la_LIBADD = $(XCB_LIBS) $(XPROTO_LIBS)
libxcb_xrm_la_LDFLAGS = -version-info 0:0:0 -no-undefined -export-symbols-regex '^xcb_xrm_'

pkgconfig_DATA = xcb-xrm.pc

EXTRA_DIST = xcb-xrm.pc.in xrm.h
