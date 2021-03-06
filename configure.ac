# Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59) ## specificy version of autoconf
AC_INIT(BaseVarC, 1.0.0, zimusen94@gmail.com)
AM_INIT_AUTOMAKE(foreign)
AC_CONFIG_SRCDIR([src/BaseVarC.cpp])
AC_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE([disable])

# Checks for programs.
AC_PROG_CXX ## test for cpp compiler
AC_PROG_CC ## test for C compiler
AC_PROG_RANLIB ## required if libraries are built in package

# Check for headers
AC_LANG([C++])
AC_CHECK_HEADER([zlib.h])

# Check for libraries
##AC_SEARCH_LIBS([library],[function], [action-if-found], [action if not found])
AC_SEARCH_LIBS([lzma_code],[lzma],,[AC_MSG_ERROR([lzma (part of xz) not found, please install xz (https://tukaani.org/xz/)])])
AC_SEARCH_LIBS([BZ2_bzBuffToBuffDecompress],[bz2],,[AC_MSG_ERROR([lib bz2 not found, please install])])
AC_SEARCH_LIBS([gzopen],[z],,[AC_MSG_ERROR([libz not found, please install zlib (http://www.zlib.net/)])])
AC_SEARCH_LIBS([clock_gettime], [rt], [AC_DEFINE([HAVE_CLOCK_GETTIME], [1], [clock_getttime found])], )

# Only fail on warnings when the --enable-development flag is passed into configure
AC_ARG_ENABLE(development, AS_HELP_STRING([--enable-development],
	[Turn on development options, like failing compilation on warnings]))
if test "$enable_development"; then
    fail_on_warning="-Werror"
fi

# Set compiler flags.
AC_SUBST(AM_CXXFLAGS, "-std=c++11 -Wall -Wextra -Wno-unknown-pragmas")
##AC_SUBST(CXXFLAGS, "$CXXFLAGS")
##AC_SUBST(CFLAGS, "$CFLAGS")
##pthread required for htslib to link
AC_SUBST(LDFLAGS, "$LDFLAGS -pthread")
##AC_SUBST(LIBS, "$LIBS")

AC_CONFIG_FILES([Makefile
                 SeqLib/src/Makefile
                 src/Makefile])

AC_OUTPUT
