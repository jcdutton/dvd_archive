include $(top_srcdir)/misc/Makefile.common

AM_CFLAGS = -I$(top_srcdir)/src

bin_PROGRAMS = dvd_archive

dvd_archive_SOURCES = \
	dvd_archive.c dvd_udf.c

dvd_archive_LDADD = -L$(libdir) -ldl

dvd_archive_LDFLAGS = -g -O0

