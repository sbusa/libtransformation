AC_INIT([corenova], [1.4.0],
	[http://bugs.corenova.com/])
AC_CONFIG_SRCDIR(configure.ac)

# This is the only custom macro directory we have.
AC_CONFIG_MACRO_DIR([m4])

# We can store things like config.guess and config.sub here.
AC_CONFIG_AUX_DIR([config])

AC_CANONICAL_TARGET

AM_INIT_AUTOMAKE(AC_PACKAGE_NAME, AC_PACKAGE_VERSION)
AM_CONFIG_HEADER(config/config.h)


AC_ENABLE_SHARED(yes)
AC_ENABLE_STATIC(yes)

AC_PROG_SED
#AC_ISC_POSIX
#AC_PROG_CC
AC_HEADER_STDC

AC_PROG_CPP
AC_PROG_CXX


AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal)

dnl type checks
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# check for headers
AC_CHECK_HEADERS([stdint.h sys/loadavg.h])

# check for functions
AC_CHECK_FUNCS([strcasestr asprintf getline getloadavg])

### default CFLAGS!
CFLAGS="-Wall -O2"
ARCH=`uname -p`
CPPFLAGS="$CPPFLAGS -D_REENTRANT -D$ARCH"

# system check
changequote(, )
PARTIALTARGETOS=`echo $target_os | sed 's/[-._].*//'`

case "$PARTIALTARGETOS" in
	 xmingw32msvc)
	   CFLAGS="$CFLAGS -Dmingw32"
	 ;;
	 freebsd*)
	   CFLAGS="$CFLAGS -I/usr/local/include"
	   LDFLAGS="$LDFLAGS -L/usr/local/lib"
	   CPPFLAGS="${CPPFLAGS} -I/usr/local/include"
	 ;;
esac

CFLAGS="$CFLAGS -D$PARTIALTARGETOS"
CPPFLAGS="$CPPFLAGS -D$PARTIALTARGETOS"

changequote([, ])

AC_MSG_NOTICE([target platform is $PARTIALTARGETOS]) 
AC_MSG_NOTICE([CFLAGS=$CFLAGS]) 
AC_MSG_NOTICE([LDFLAGS=$LDFLAGS]) 

dnl look into CFLAGS again... something sets up cflags with
dnl -march=i686, etc. but on some other platform, it insists on adding
dnl -g, which blows up the size of the generated objects!

###
AC_MSG_NOTICE(checking for existence of libtool)
LT_INIT([dlopen])
LT_CONFIG_LTDL_DIR([libltdl])
LTDL_INIT
AC_SUBST(LIBLTDL)
AC_SUBST(LD)

###
AC_MSG_NOTICE([checking for version of libtool])

ac_save_CPPFLAGS=$CPPFLAGS
ac_save_LDFLAGS=$LDFLAGS

CPPFLAGS="$CPPFLAGS $LTDLINCL"
LDFLAGS="$LDFLAGS $LIBLTDL"
# lt_dladvise_init was introduced with ltdl-2.2
AC_CHECK_LIB([ltdl], [lt_dladvise_init], [], [
					 AC_DEFINE(LTDL_OLD,1,[Define this if LIBTOOL is OLD])])
CPPFLAGS=$ac_save_CPPFLAGS
LDFLAGS=$ac_save_LDFLAGS


###
AC_MSG_NOTICE([checking for existence of OpenSSL])

dnl we don't save CPPFLAGS here since dependency on ssl.h by other
dnl headers (other than our ssl module header) would be unable to find
dnl it.  I need to create a better abstraction for network connection
dnl like I did for database abstraction module.

# ac_save_CPPFLAGS=$CPPFLAGS
ac_save_LDFLAGS=$LDFLAGS

case "$PARTIALTARGETOS" in
	 freebsd*)
		LDFLAGS="${LDFLAGS} `krb5-config --libs` -lcom_err -lssl -lcrypt"
esac


AC_ARG_WITH(kerberos,
  [ --with-kerberos=[[=DIR]] use kerberos installation in DIR], [ 
  KERBEROS_DIR=$withval
  CPPFLAGS="${CPPFLAGS} -I${withval}/include"
  LDFLAGS="${LDFLAGS} -L${withval}/lib -lssl"
   ])

AC_ARG_WITH(crypto,
  [ --with-crypto=[[=DIR]] use crypto installation in DIR], [ 
  CRYPTO_DIR=$withval
  CPPFLAGS="${CPPFLAGS} -I${withval}/include"
  LDFLAGS="${LDFLAGS} -L${withval}/lib" ])

AC_ARG_WITH(ssl, 
  [ --with-ssl=[[=DIR]] use openssl installation in DIR], [ 
  SSL_DIR=$withval
  CPPFLAGS="${CPPFLAGS} -I${withval}/include"
  LDFLAGS="${LDFLAGS} -L${withval}/lib" ])

case "$PARTIALTARGETOS" in
	 freebsd*)
		LDFLAGS="${LDFLAGS} `krb5-config --libs` -lcom_err"
	 ;;
esac

AC_CHECK_HEADERS(krb5.h,
  AC_CHECK_LIB(krb5, krb5_init_context, [
    AC_DEFINE(HAVE_LIBKERBEROS, 1, [Define this if kerberos is available])
	if test x$KERBEROS_DIR != x; then
	  SSL_CFLAGS="-I${KERBEROS_DIR}/include"
	  LIBSSL="-L${KERBEROS_DIR}/lib -lkrb5"
	else
	  LIBSSL="-lkrb5"
	if test "$PARTIALTARGETOS" = "freebsd8"; then
		LIBSSL="${LIBSSL} `krb5-config --libs` -lcom_err"
	fi
fi],
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  kerberos library not found!  Consider using --with-kerberos to 
  specify the installation location for kerberos.

  SSL support (if found) will be provided without kerberos support!
  --------------------------------------------------------------------- 
  ])), [
  CPPFLAGS="${CPPFLAGS} -DOPENSSL_NO_KRB5"
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  kerberos headers not found!  Consider using --with-kerberos to 
  specify the installation location for kerberos.

  SSL support (if found) will be provided without kerberos support!
  --------------------------------------------------------------------- 
  ]) ])

AC_CHECK_HEADERS(openssl/ssl.h,
  AC_CHECK_LIB(crypto, RSA_new,
  LDFLAGS="${LDFLAGS} -lcrypto"
    AC_CHECK_LIB(ssl, SSL_new, [
	build_ssl=true
	if test x$SSL_DIR != x; then
      SSL_CFLAGS="$SSL_CFLAGS -I${SSL_DIR}/include"
      LIBSSL="$LIBSSL -L${SSL_DIR}/lib -lssl -lcrypto"
    else
      LIBSSL="$LIBSSL -lssl -lcrypto"
	if test "$PARTIALTARGETOS" = "freebsd8"; then
		LIBSSL="${LIBSSL} -lcom_err -lasn1"
	fi
    fi],
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  ssl library not found!  Consider using --with-ssl to specify the 
  installation location for openssl.

  No SSL support will be provided with this build.
  --------------------------------------------------------------------- 
  ])),
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  crypto library not found!  Consider using --with-crypto to specify 
  the installation location for crypto.

  No SSL & crypto support will be provided with this build.
  --------------------------------------------------------------------- 
  ])),
  AC_MSG_RESULT( [
  Consider using --with-ssl to specify the location
  ]))

AC_SUBST(SSL_CFLAGS)
AC_SUBST(LIBSSL)

AM_CONDITIONAL(BUILD_SSL, test x$build_ssl = xtrue)

# CPPFLAGS=$ac_save_CPPFLAGS
LDFLAGS=$ac_save_LDFLAGS

###
AC_MSG_NOTICE([checking for existence of Pthreads])

AC_CHECK_HEADERS(pthread.h,
  AC_CHECK_LIB(pthread, pthread_create, [
    AC_DEFINE(HAVE_LIBPTHREAD, 1, [Define this if pthreads is available])
	if test "$PARTIALTARGETOS" = "solaris2"; then
	  PTHREAD_CFLAGS="-pthreads"
	  LIBPTHREAD="-lpthread"
	else
	  PTHREAD_CFLAGS="-pthread"
	  LIBPTHREAD="-lpthread"
	fi ],
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  Pthread library not found.  Since Corenova Framework relies heavily
  on locking features of libpthread building without this is not 
  recommended.  
  --------------------------------------------------------------------- 
  ])),
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  Pthread headers not found.  Since Corenova Framework relies heavily
  on locking features of libpthread building without this is not 
  recommended.  
  --------------------------------------------------------------------- 
  ]))
AC_SUBST(LIBPTHREAD)
AC_SUBST(PTHREAD_CFLAGS)

###
AC_MSG_NOTICE([checking for existence of LIBNETFILTER and NFQUEUE])

ac_save_LDFLAGS=$LDFLAGS


##### Keen changed 'linux/netfilter.h' to 'libnfnetlink/libnfnetlink.h'
##AC_CHECK_HEADERS([linux/netfilter.h],
AC_CHECK_HEADERS([libnfnetlink/libnfnetlink.h],
  AC_CHECK_LIB(nfnetlink, nfnl_open, [
    LDFLAGS="${LDFLAGS} -lnfnetlink"
    AC_DEFINE(HAVE_LIBNFNETLINK, 1, [Define this if nfnetlink library is available])
    LIBNETFILTER="-lnfnetlink" ],
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  nfnetlink library not found.  For proxying operations transparent proxy
  based on syn capture from the firewall will not work.
  --------------------------------------------------------------------- 
  ])),
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  linux netfilter headers not found.
  --------------------------------------------------------------------- 
  ]))

AC_CHECK_HEADERS(libnetfilter_queue/libnetfilter_queue.h,
  AC_CHECK_LIB(netfilter_queue, nfq_handle_packet, [
    AC_DEFINE(HAVE_LIBNFQUEUE, 1, [Define this if netfilter queue library is available])
  	  LIBNFQUEUE="-lnetfilter_queue"
	  build_nfqueue=true ],
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  netfilter queue library not found.  For proxying operations
  transparent proxy based on syn capture from the firewall will 
  not work.
  ---------------------------------------------------------------------
  ])),
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  linux netfilter queue headers not found.
  --------------------------------------------------------------------- 
  ]))

LDFLAGS=$ac_save_LDFLAGS

AC_SUBST(LIBNETFILTER)
AC_SUBST(LIBNFQUEUE)

AM_CONDITIONAL(BUILD_NFQUEUE, test x$build_nfqueue = xtrue)


###
AC_MSG_NOTICE([checking for existence of tiny c compiler library])

AC_CHECK_HEADERS(libtcc.h,
  AC_CHECK_LIB(tcc, tcc_preprocess_file, [
    AC_DEFINE(HAVE_LIBTCC, 1, [Define this if tiny c compiler library is available])
  	  LIBTCC="-ltcc"
	  build_tcc=true ],
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  Tiny C Compiler library not found.  For configuration of transformations
  dynamic preprocessing of configuration rules will not be supported.
  ---------------------------------------------------------------------
  ])),
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  Tiny C Compiler headers not found.
  --------------------------------------------------------------------- 
  ]))

CPPFLAGS=$ac_save_CPPFLAGS
LDFLAGS=$ac_save_LDFLAGS

AC_SUBST(LIBTCC)

AM_CONDITIONAL(BUILD_TCC, test x$build_tcc = xtrue)


###
AC_MSG_NOTICE([checking for existence of watchdog])

AC_CHECK_HEADERS(linux/watchdog.h, [
  build_watchdog=true ],
  AC_MSG_RESULT( [
  ---------------------------------------------------------------------
  linux/watchdog.h not found.
  
  No watchdog module support will be provided with this build.
  --------------------------------------------------------------------- 
  ]))

AM_CONDITIONAL(BUILD_WATCHDOG, test x$build_watchdog = xtrue)


###
dnl Allow a '--with-mysql' or '--without-mysql' option, default to yes.
AC_ARG_WITH([mysql],
	[ --with-mysql=[[=DIR]] use mysql installation in DIR],
	[],
	[with_mysql=check])

if test "x$with_mysql" != xno; then
	AC_MSG_NOTICE([checking for existence of MySQL])

	ac_save_CPPFLAGS=$CPPFLAGS
	ac_save_LDFLAGS=$LDFLAGS

	dnl If configured with an argument, use it instead of the default path.
	if test "x$with_mysql" != xcheck; then
		MYSQL_CFLAGS="-I${with_mysql}/include"
		LIBMYSQL="-L${with_mysql}/lib/mysql -lmysqlclient_r"
	else
		dnl common mysql library installation paths
		MYSQL_LIBDIRS=" -L/usr/lib64/mysql -L/usr/lib/mysql -L/usr/local/lib/mysql"
		LIBMYSQL="${MYSQL_LIBDIRS} -lmysqlclient_r"
	fi

	CPPFLAGS="${CPPFLAGS} $MYSQL_CFLAGS"
	LDFLAGS="${LDFLAGS} ${LIBMYSQL}"

	AC_CHECK_HEADERS(mysql/mysql.h,
		AC_CHECK_LIB(mysqlclient_r, mysql_init,
			[AC_DEFINE(HAVE_LIBMYSQL, 1, [Define this if MySQL is available])
			build_mysql=true],
			AC_MSG_RESULT( [
			---------------------------------------------------------------------
			Mysql library not found!  Consider using --with-mysql to specify the 
			installation location for Mysql.

			No Mysql support will be provided with this build.
			---------------------------------------------------------------------
			])),
		AC_MSG_RESULT( [
		---------------------------------------------------------------------
		Mysql header files not found!  Consider using --with-mysql to specify 
		the installation location for Mysql.

		No Mysql support will be provided with this build.
		--------------------------------------------------------------------- 
		]))
fi

AC_SUBST(MYSQL_CFLAGS)
AC_SUBST(LIBMYSQL)


AM_CONDITIONAL(BUILD_MYSQL, test x$build_mysql = xtrue)

CPPFLAGS=$ac_save_CPPFLAGS
LDFLAGS=$ac_save_LDFLAGS

#####

dnl Allow a '--with-ntlm' or '--without-ntlm' option, default to yes.
AC_ARG_WITH([ntlm],
	[ --with-ntlm=[[=DIR]] use ntlm installation in DIR],
	[],
	[with_ntlm=check])

if test "x$with_ntlm" != xno; then
	AC_MSG_NOTICE([checking for existence of LIBNTLM])

	ac_save_CPPFLAGS=$CPPFLAGS
	ac_save_LDFLAGS=$LDFLAGS

	dnl If configured with an argument, use it instead of the default path.
	if test "x$with_ntlm" != xcheck; then
		NTLM_CFLAGS="-I${with_ntlm}/include"
		LIBNTLM="-L${with_ntlm}/lib/ntlm -lntlm"
	else
		dnl common ntlm library installation paths
		NTLM_LIBDIRS=" -L/usr/lib64/ntlm -L/usr/lib/ntlm -L/usr/local/lib/ntlm"
		LIBNTLM="${NTLM_LIBDIRS} -lntlm"
	fi

	CPPFLAGS="${CPPFLAGS} $NTLM_CFLAGS"
	LDFLAGS="${LDFLAGS} ${LIBNTLM}"

	AC_CHECK_HEADERS(ntlm.h,
		AC_CHECK_LIB(ntlm, buildSmbNtlmAuthRequest,
			[AC_DEFINE(HAVE_LIBNTLM, 1, [Define this if LIBNTLM is available])
			build_ntlm=true],
			AC_MSG_RESULT( [
			---------------------------------------------------------------------
			NTLM library not found!  Consider using --with-ntlm to specify the
			installation location for NTLM.

			No NTLM support will be provided with this build.
			---------------------------------------------------------------------
			])),
		AC_MSG_RESULT( [
		---------------------------------------------------------------------
		NTLM header files not found!  Consider using --with-ntlm to specify
		the installation location for NTLM.

		No NTLM support will be provided with this build.
		---------------------------------------------------------------------
		]))
fi

AC_SUBST(NTLM_CFLAGS)
AC_SUBST(LIBNTLM)


AM_CONDITIONAL(BUILD_NTLM, test x$build_ntlm = xtrue)

CPPFLAGS=$ac_save_CPPFLAGS
LDFLAGS=$ac_save_LDFLAGS

#####

dnl Allow a '--with-pgsql' or '--without-pgsql' option, default to yes.
AC_ARG_WITH([pgsql],
	[ --with-pgsql=[[=DIR]] use pgsql installation in DIR],
	[],
	[with_pgsql=check])

if test "x$with_pgsql" != xno; then
	AC_MSG_NOTICE([checking for existence of PostgreSQL])

	ac_save_CPPFLAGS=$CPPFLAGS
	ac_save_LDFLAGS=$LDFLAGS

	if test "x$with_pgsql" != xcheck; then
		PGSQL_CFLAGS="-I${with_pgsql}/include"
		LIBPGSQL="-L${with_pgsql}/lib -lpq"
	else
		AC_CHECK_PROG(PGCONFIG, pg_config, :)
		PGSQL_LIBDIRS="-L`pg_config --libdir`"
		PGSQL_INCDIRS="-I`pg_config --includedir`"
		PGSQL_CFLAGS="${PGSQL_INCDIRS}"
		LIBPGSQL="${PGSQL_LIBDIRS} -lpq"
	fi

	CPPFLAGS="${CPPFLAGS} $PGSQL_CFLAGS"
	LDFLAGS="${LDFLAGS} $LIBPGSQL"

	AC_CHECK_HEADERS(libpq-fe.h,
	  AC_CHECK_LIB(pq, PQconnectdb, [ 
	    AC_DEFINE(HAVE_LIBPGSQL, 1, [Define this if PostgreSQL is available])
	    build_pgsql=true],
		  AC_MSG_RESULT( [
		  ---------------------------------------------------------------------
		  PostgreSQL library not found!  Consider using --with-pgsql to specify the 
		  installation location for PostgreSQL.
	
		  No PostgreSQL support will be provided with this build.
		  ---------------------------------------------------------------------
	  	])),
	  AC_MSG_RESULT( [
	  ---------------------------------------------------------------------
	  PostgreSQL header files not found!  Consider using --with-pgsql to specify 
	  the installation location for PostgreSQL.
	
	  No PostgreSQL support will be provided with this build.
	  --------------------------------------------------------------------- 
	  ]))
fi

AC_SUBST(PGSQL_CFLAGS)
AC_SUBST(LIBPGSQL)

AM_CONDITIONAL(BUILD_PGSQL, test x$build_pgsql = xtrue)

CPPFLAGS=$ac_save_CPPFLAGS
LDFLAGS=$ac_save_LDFLAGS

#################################################################################
#### move activemq here, and add checking for mq functions and lib existance ####
#################################################################################

####################### Added by Keen for ActiveMQ-CPP lib. [Start] #######################
AC_MSG_NOTICE(checking for ActiveMQ-CPP lib start.)

ac_save_CPPFLAGS=$CPPFLAGS
ac_save_LDFLAGS=$LDFLAGS

AC_ARG_WITH(activemq, 
  [ --with-activemq=[[=DIR]] use ActiveMQ header files folder in DIR(/usr/local/include/activemq-cpp-3.1.0)], [ 
  ACTIVEMQ_HEADERS_DIR=$withval])

if test x$ACTIVEMQ_HEADERS_DIR != x; then


	AC_MSG_NOTICE(--------------- find and configure the Apache Decaf Library --------------)
	
	## find and configure the Apache Decaf Library
	## DECAF_CONFIGURE_DECAF - Future use for decaf as a standalone lib
	## Flags for building the decaf cpp library
	## find and configure the APR, and APR Utils
	DECAF_CONFIGURE_APR
	DECAF_CXXFLAGS="$APR_CPPFLAGS $APR_INCLUDES $APU_INCLUDES"
	DECAF_LIBS="$APR_LIBS $APR_LDFLAGS $APU_LIBS $APU_LDFLAGS"

	AC_SUBST([DECAF_CXXFLAGS])
	AC_SUBST([DECAF_LIBS])

	AC_MSG_NOTICE(---------------------------------------------------------------------------)
	
#	CPPFLAGS="${CPPFLAGS} ${DECAF_CXXFLAGS}"
#	LDFLAGS="${LDFLAGS} ${DECAF_LIBS}"


	ACTIVEMQ_CFLAGS="-I${ACTIVEMQ_HEADERS_DIR}"
	AC_SUBST(ACTIVEMQ_CFLAGS)

	LIBACTIVEMQ="-lactivemq-cpp"
	AC_SUBST(LIBACTIVEMQ)
	
	CPPFLAGS="${CPPFLAGS} -I${ACTIVEMQ_HEADERS_DIR}"
	LDFLAGS="${LDFLAGS} ${LIBACTIVEMQ}"
	
	build_activemq=true

	AC_LANG_PUSH([C++])
	
    # Perform some tests on C++.
    
	if test x$build_activemq = xtrue; then
	  AC_CHECK_HEADERS([activemq/library/ActiveMQCPP.h],[
	    AC_MSG_RESULT([== Head File for ActiveMQCPP Found ==])
	  ],[
	    AC_MSG_RESULT( [
	    -----------------------------------
	    Head File for ActiveMQCPP NOT Found
	    -----------------------------------
	    ])
	    build_activemq=false
	  ])
	fi
	
	if test x$build_activemq = xtrue; then
  	  AC_TRY_LINK(
		[#include <activemq/library/ActiveMQCPP.h>],
		[activemq::library::ActiveMQCPP::initializeLibrary();],
		[
  		  AC_MSG_RESULT([== ActiveMQ LIB Link Test OK ==])
		],[
		  AC_MSG_RESULT([
		  -----------------------------
		  ActiveMQ LIB Link Test Failed
		  -----------------------------
		  ])
		  build_activemq=false		  
		]
	  )
	fi
	
	AC_LANG_POP([C++])
fi

AM_CONDITIONAL(BUILD_ACTIVEMQ, test x$build_activemq = xtrue)

CPPFLAGS=$ac_save_CPPFLAGS
LDFLAGS=$ac_save_LDFLAGS

AC_MSG_NOTICE(checking for ActiveMQ-CPP finished.)
####################### Added by Keen for ActiveMQ-CPP lib. [End] ##########################

AC_MSG_NOTICE([checking for existence of PCAP])

ac_save_CPPFLAGS=$CPPFLAGS
ac_save_LDFLAGS=$LDFLAGS

AC_ARG_WITH(pcap,
  [ --with-pcap=[[=DIR]] use pcap installation in DIR], [
  PCAP_DIR=$withval
  CPPFLAGS="${CPPFLAGS} -I${withval}/include"
  LDFLAGS="${LDFLAGS} -L${withval}/lib" ])

# for existence of PCAP library
AC_CHECK_HEADERS(pcap.h,
  AC_CHECK_LIB(pcap, pcap_version, [
    AC_DEFINE(HAVE_PCAP, 1, [Define this if PCAP is available])
	build_pcap=true
    if test x$PCAP_DIR != x; then
	  PCAP_CFLAGS="-I${PCAP_DIR}/include"
	  PCAP_LDFLAGS="-I${PCAP_DIR}/lib -lpcap"
    else
	  PCAP_LDFLAGS="-lpcap"
	fi ],
  AC_MSG_RESULT([
  -----------------------------------------------------------------
  PCAP library not found! You cannot --enable-trafstat to
  provide traffic statistic capture capability.  You can use
  --with-pcap to specify the installation location of PCAP.
  -----------------------------------------------------------------
  ]),-lpthread),
  AC_MSG_RESULT([
  -----------------------------------------------------------------
  PCAP headers not found! You cannot --enable-trafstat to
  provide traffic statistic capture capability.  You can use
  --with-pcap to specify the installation location of PCAP.
  -----------------------------------------------------------------
  ]))
AC_SUBST(PCAP_CFLAGS)
AC_SUBST(PCAP_LDFLAGS)

AM_CONDITIONAL(BUILD_PCAP, test x$build_pcap = xtrue)

CPPFLAGS=$ac_save_CPPFLAGS
LDFLAGS=$ac_save_LDFLAGS

#####

AC_MSG_NOTICE([performing build option checks])

AC_ARG_WITH(debug,
  [ --with-debug=LVL       enable debugging mode set to specified level],
  [ CFLAGS="$CFLAGS -D__DEBUG__=$withval -g" 
    build_debug=true ])

AM_CONDITIONAL(BUILD_TESTPROGS, test x$build_debug = xtrue)

###
AC_MSG_CHECKING(whether to build static program executables)
AC_ARG_ENABLE(static-build,
  [ --enable-static-build  enable static build of program executable], [
  AC_MSG_RESULT(yes)
  STATIC_BUILD=-static 
  AC_SUBST(STATIC_BUILD) ],
  AC_MSG_RESULT(no))

###
AC_MSG_CHECKING(whether to preopen dynamically loaded modules)
AC_ARG_ENABLE(preopen,
  [ --enable-preopen       enable preopen for programs], [
  AC_MSG_RESULT(yes)
  AC_DEFINE(USE_PREOPEN, 1,[Defined if using preopen])
  PREOPEN="-dlpreopen force"
  AC_SUBST(PREOPEN) ],
  AC_MSG_RESULT(no))

###
AC_MSG_CHECKING(whether to use Electric Fence for memory protection)
AC_ARG_ENABLE(efence,
  [ --enable-efence        enable Electric Fence memory guard], [
  AC_MSG_RESULT(yes)
  AC_CHECK_LIB(efence, calloc, [
    AC_DEFINE(HAVE_LIBEFENCE, 1, [Define this if libefence is found])
    LIBS="$LIBS -lefence" ],
  AC_MSG_RESULT( [  
  -----------------------------------------------------------------
  Electric Fence library not found; You cannot '--enable-efence' to
  protect your code during debugging. 
  ----------------------------------------------------------------- 
  ]),$LIBPTHREAD)], AC_MSG_RESULT(no))

###
AC_MSG_CHECKING(whether to use Netlog for network usage statistics)
AC_ARG_ENABLE(netlog,
  [ --enable-netlog       enable NetLog library for network usage stats], [
  AC_MSG_RESULT(yes)
  AC_CHECK_LIB(netlog, netlog_init, [
    AC_DEFINE(HAVE_LIBNETLOG, 1, [Define this if libnetlog is found])
    CFLAGS="$CFLAGS -DUSE_NETLOG1"
    LIBS="$LIBS -lnetlog" ],
  AC_MSG_RESULT( [
  -----------------------------------------------------------------
  NetLog library not found; No network usage stats will be provided
  * refer to Documentation for details on NetLog.
  ----------------------------------------------------------------- 
  ]))],
  AC_MSG_RESULT(no))

###
# always allow contrib to be built -- for now...
enable_contrib=true
#AC_ARG_DISABLE(contrib, [ --disable-contrib       disable building sources in contrib], enable_contrib=false,
#  AC_DEFINE(HAVE_LIBUBIQX, 1, [Define this if libubiqx is found]))
AM_CONDITIONAL(BUILD_CONTRIB, test x$enable_contrib = xtrue)

##
# Makefiles to generate

AC_OUTPUT([
Makefile
corenova.pc
src/Makefile
src/contrib/Makefile
src/include/Makefile
src/library/Makefile
src/modules/Makefile
src/modules/corenova/Makefile
src/modules/corenova/data/Makefile
src/modules/corenova/data/db/Makefile
src/modules/corenova/data/processor/Makefile
src/modules/corenova/data/configuration/Makefile
src/modules/corenova/net/Makefile
src/modules/corenova/xform/Makefile
src/modules/corenova/net/proto/Makefile
src/modules/corenova/sys/Makefile
src/programs/Makefile
src/programs/valgrind.sh
src/contrib/ActiveMQ/Makefile
])
