if UNITTESTS
include ../src/common.mk
include $(top_srcdir)/aminclude_static.am

AM_CPPFLAGS = -I$(top_srcdir)/src -I$(top_srcdir)/include
AM_CFLAGS = $(XORG_CFLAGS) $(CWARNFLAGS) $(UDEV_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS= $(CODE_COVERAGE_LDFLAGS)

check_PROGRAMS = wacom-tests
check_LTLIBRARIES = libwacom-test.la
libwacom_test_la_SOURCES =$(DRIVER_SOURCES)
libwacom_test_la_CFLAGS = -DUNIT_TESTS -I$(top_srcdir)/src $(AM_CFLAGS) -fvisibility=default

TESTS=$(check_PROGRAMS)

TEST_LDADD= libwacom-test.la
COMMON_SOURCES=fake-symbols.c fake-symbols.h

wacom_tests_LDADD=$(TEST_LDADD) $(UDEV_LIBS)
wacom_tests_CFLAGS=  -DUNIT_TESTS $(AM_CFLAGS)
wacom_tests_SOURCES=wacom-tests.c $(COMMON_SOURCES)

endif
