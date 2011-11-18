EXTRA_DIST = README echo-introspect.xml

if CROSS_COMPILING
dbusxx_xml2cpp = dbusxx-xml2cpp
else
dbusxx_xml2cpp = $(top_builddir)/tools/dbusxx-xml2cpp
DBUSXX_FLAGS = --templatedir=$(top_builddir)/tools
endif

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_builddir)/include

noinst_PROGRAMS = echo-server

echo_server_SOURCES = echo-server-glue.h echo-server.h echo-server.cpp
echo_server_LDADD = $(top_builddir)/src/libdbus-c++-1.la

echo-server-glue.h: echo-introspect.xml
	$(dbusxx_xml2cpp) $^ --adaptor=$@ $(DBUSXX_FLAGS)

noinst_PROGRAMS += echo-client-mt

echo_client_mt_SOURCES = echo-client-glue.h echo-client.h echo-client.cpp
echo_client_mt_LDADD = $(top_builddir)/src/libdbus-c++-1.la @PTHREAD_LIBS@
echo_client_mt_CXXFLAGS = @PTHREAD_CFLAGS@

echo-client-glue.h: echo-introspect.xml
	$(dbusxx_xml2cpp) $^ --proxy=$@ $(DBUSXX_FLAGS)

BUILT_SOURCES = echo-server-glue.h echo-client-glue.h
CLEANFILES = $(BUILT_SOURCES)

dist-hook:
	cd $(distdir); rm -f $(BUILT_SOURCES)

MAINTAINERCLEANFILES = \
	Makefile.in
