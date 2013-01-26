#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT([fastfile-client], [1.x], [v9@fakehalo.us])
AC_PREFIX_DEFAULT([/usr])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_HEADER_STDC

AC_MSG_CHECKING(for unsigned long long)
AC_TRY_COMPILE([],[unsigned long long x; x = 0;], [], [AC_MSG_ERROR([No unsigned long long])])

AC_CHECK_LIB([nsl], [gethostbyname])
AC_CHECK_LIB([xnet], [gethostbyname])
AC_CHECK_LIB([socket], [connect])
AC_MSG_CHECKING([for --with-openssl])
AC_ARG_WITH(
	[openssl],
	[AS_HELP_STRING([--with-openssl=PATH], [Path to OpenSSL])],
	[ LDFLAGS="$LDFLAGS -L$with_openssl/lib";
	  CFLAGS="$CFLAGS -I$with_openssl/include/";
	  LIBS="-lssl";
	  AC_MSG_RESULT([$with_openssl])
	],
	[AC_MSG_RESULT([no])]
)
AC_CHECK_LIB([z], [compress])
AC_CHECK_LIB([crypto], [CRYPTO_new_ex_data])
AC_CHECK_LIB([ssl], [SSL_library_init])

AC_CHECK_HEADERS([stdio.h stdarg.h stdlib.h unistd.h string.h strings.h time.h fcntl.h ctype.h getopt.h netdb.h signal.h errno.h dirent.h libgen.h termios.h pwd.h grp.h glob.h syslog.h sys/wait.h sys/mman.h sys/types.h sys/stat.h sys/time.h sys/ioctl.h sys/socket.h netinet/in.h arpa/inet.h openssl/ssl.h openssl/rand.h openssl/err.h openssl/bio.h])

AC_CHECK_TYPE([struct sockaddr_in6], [AC_DEFINE(HAVE_SOCKADDR_IN6, 1, [Have IPv6 struct.]) ], [], [#include <netinet/in.h>])
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T

AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([alarm gethostbyname gethostbyname2 inet_pton gettimeofday memset munmap select socket strcasecmp strdup strerror])

AC_OUTPUT([Makefile])
