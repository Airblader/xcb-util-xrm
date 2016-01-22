ACLOCAL_AMFLAGS = -I m4

MAINTAINERCLEANFILES = ChangeLog INSTALL Makefile.in

.PHONY: ChangeLog INSTALL

INSTALL:
	$(INSTALL_CMD)  

ChangeLog:
	$(CHANGELOG_CMD)

dist-hook: ChangeLog INSTALL

EXTRA_DIST = autogen.sh xcb-xrm.pc.in xrm/xrm.h

lib_LTLIBRARIES = libxcb-xrm.la

xcbinclude_HEADERS = xrm/xcb_xrm.h

AM_CFLAGS = $(CWARNFLAGS)

libxcb_xrm_la_SOURCES = xrm/xrm.c xrm/entry.c xrm/util.c
libxcb_xrm_la_CPPFLAGS = $(XCB_CFLAGS) $(XPROTO_CFLAGS)
libxcb_xrm_la_LIBADD = $(XCB_LIBS) $(XPROTO_LIBS)
libxcb_xrm_la_LDFLAGS = -version-info 0:0:0 -no-undefined -export-symbols-regex '^xcb_xrm_'

pkgconfig_DATA = xrm/xcb-xrm.pc

TESTS = tests/test
check_PROGRAMS = tests/test
tests_test_SOURCE = tests/test.c
tests_test_CPPFLAGS = -I$(srcdir)/xrm/ $(XCB_CFLAGS)
tests_test_LDADD = libxcb-xrm.la $(XCB_LIBS)
