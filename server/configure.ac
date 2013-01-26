AC_PREREQ([2.68])
AC_INIT([fastfile-server], [1.x], [v9@fakehalo.us])
AC_PREFIX_DEFAULT([/usr])
AC_CONFIG_HEADERS([config.h])

AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

AC_HEADER_STDC

AC_MSG_CHECKING(for unsigned long long)
AC_TRY_COMPILE([],[unsigned long long x; x = 0;], [], [AC_MSG_ERROR([No unsigned long long])])

AC_ARG_WITH(apxs, AC_HELP_STRING([--with-apxs=NAME],
            [name of the apxs executable [apxs]]),
            [APXS="$with_apxs"])
if test -z "${APXS}"; then
    AC_PATH_PROGS(APXS, apxs2 apxs, no, [$PATH:/usr/sbin:/usr/local/apache2/bin])
fi
if test "$APXS" = no; then
    AC_MSG_ERROR([apxs not found. set apxs with --with-apxs.])
fi
AC_SUBST(APXS)

AP_INSTALLBUILDDIR=`$APXS -q installbuilddir`
AP_LIBEXECDIR=`$APXS -q LIBEXECDIR`
AP_SYSCONFDIR=`$APXS -q SYSCONFDIR`
AP_TOP_BUILDDIR=`dirname $AP_INSTALLBUILDDIR`
AP_TOP_SRCDIR=$AP_TOP_BUILDDIR
AC_SUBST(AP_INSTALLBUILDDIR)
AC_SUBST(AP_LIBEXECDIR)
AC_SUBST(AP_SYSCONFDIR)
AC_SUBST(AP_TOP_BUILDDIR)
AC_SUBST(AP_TOP_SRCDIR)

AC_ARG_WITH(apachectl, AC_HELP_STRING([--with-apachectl=NAME],
            [name of the apachectl executable [apachectl]]),
            [APACHECTL="$with_apachectl"])
if test -z "${APACHECTL}"; then
    AC_PATH_PROGS(APACHECTL, apache2ctl apachectl, no, [$PATH:/usr/sbin:/usr/local/apache2/bin])
fi
if test "$APACHECTL" = no; then
    AC_MSG_ERROR([apachectl not found. set apachectl with --with-apachectl.])
fi
AC_SUBST(APACHECTL)

AC_ARG_WITH(pool-dir, AC_HELP_STRING([--with-pool-dir=DIR],
            [specifies alternate default pool directory [/home/fastfile]]),
            [POOLDIR="$withval"], [POOLDIR="/home/fastfile"])
AC_SUBST(POOLDIR)
AC_ARG_WITH(apache-group, AC_HELP_STRING([--with-apache-group=GROUP],
            [specifies the group apache runs under [www-data]]),
            [APACHEGROUP="$withval"], [APACHEGROUP="www-data"])
AC_SUBST(APACHEGROUP)

AC_CHECK_HEADERS([stdio.h stdarg.h stdlib.h unistd.h string.h strings.h time.h fcntl.h ctype.h getopt.h netdb.h signal.h errno.h dirent.h libgen.h termios.h pwd.h grp.h glob.h syslog.h sys/wait.h sys/mman.h sys/types.h sys/stat.h sys/time.h sys/ioctl.h sys/socket.h netinet/in.h arpa/inet.h])

AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_PID_T

AC_FUNC_CHOWN
AC_FUNC_FORK
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([ftruncate memmove memset mkdir realpath strcasecmp strchr strdup strtol])

AC_OUTPUT([fastfile.conf Makefile mod_fastfile/Makefile ffpool/Makefile])
