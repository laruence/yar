dnl $Id$
dnl config.m4 for extension yar

PHP_ARG_ENABLE(yar, whether to enable yar support,
[  --enable-yar           Enable yar support])

PHP_ARG_WITH(curl, for curl protocol support,
[  --with-curl[=DIR]       Include curl protocol support])

PHP_ARG_ENABLE(msgpack, for msgpack packager support,
[  --enable-msgpack Include msgpack packager support], no, no)

PHP_ARG_ENABLE(epoll, for epoll support,
[  --enable-epoll Use epoll instead select support], no, no)

AC_DEFUN([AC_YAR_EPOLL],
  [
    AC_MSG_CHECKING([for epoll])

    AC_TRY_COMPILE(
    [
      #include <sys/epoll.h>
    ], [
      int epollfd;
      struct epoll_event e;

      epollfd = epoll_create(1);
      if (epollfd < 0) {
        return 1;
      }

      e.events = EPOLLIN | EPOLLET;
      e.data.fd = 0;

      if (epoll_ctl(epollfd, EPOLL_CTL_ADD, 0, &e) == -1) {
        return 1;
      }

      e.events = 0;
      if (epoll_wait(epollfd, &e, 1, 1) < 0) {
        return 1;
      }
    ], [
      AC_DEFINE([HAVE_EPOLL], 1, [do we have epoll?])
      AC_MSG_RESULT([yes])
    ], [
      AC_MSG_RESULT([no])
    ])
  ])

if test "$PHP_EPOLL" != "no"; then
	AC_YAR_EPOLL()
fi;

if test "$PHP_YAR" != "no"; then
  if test -r $PHP_CURL/include/curl/easy.h; then
    CURL_DIR=$PHP_CURL/include
  else
    AC_MSG_CHECKING(for cURL in default path)
    for i in /usr/include /usr/local/include /usr/include/x86_64-linux-gnu; do
      if test -r $i/curl/easy.h; then
        CURL_DIR=$i
        AC_MSG_RESULT(found in $i)
        break
      fi
    done
  fi
  
  if test -z "$CURL_DIR"; then
    AC_MSG_RESULT(not found)
    AC_MSG_ERROR(Please reinstall the libcurl distribution - easy.h should be in <curl-dir>/include/curl/)
  fi
  
  PHP_ADD_INCLUDE($CURL_DIR)
  PHP_EVAL_LIBLINE($CURL_LIBS, YAR_SHARED_LIBADD)

  if test -r $PHP_CURL/$PHP_LIBDIR/${CURL_LIB_NAME}.${SHLIB_SUFFIX_NAME} -o -r $PHP_CURL/$PHP_LIBDIR/${CURL_LIB_NAME}.a; then
    CURL_LIB_DIR=$PHP_CURL/$PHP_LIBDIR
  else
    AC_MSG_CHECKING(for lua library in default path)
    for i in /usr/$PHP_LIBDIR /usr/lib /usr/lib64 /usr/lib/x86_64-linux-gnu; do
      if test -r $i/${CURL_LIB_NAME}.${SHLIB_SUFFIX_NAME} -o -r $i/${CURL_LIB_NAME}.a; then
        CURL_LIB_DIR=$i
        AC_MSG_RESULT(found in $i)
        break
      fi
    done
  fi
  PHP_ADD_LIBRARY_WITH_PATH(curl, $CURL_LIB_DIR, YAR_SHARED_LIBADD)

  PHP_NEW_EXTENSION(yar, yar.c yar_server.c yar_client.c yar_request.c yar_response.c yar_exception.c yar_packager.c yar_protocol.c packagers/php.c packagers/json.c packagers/msgpack.c yar_transport.c transports/curl.c transports/socket.c, $ext_shared)

  PHP_ADD_BUILD_DIR([$ext_builddir/packagers])
  PHP_ADD_BUILD_DIR([$ext_builddir/transports])

  ifdef([PHP_ADD_EXTENSION_DEP],
  [
    PHP_ADD_EXTENSION_DEP(yar, json, true)
  ])

  if test "$PHP_MSGPACK" != "no"; then
    AC_DEFINE(ENABLE_MSGPACK,1,[enable msgpack packager])
    ifdef([PHP_ADD_EXTENSION_DEP],
    [
    PHP_ADD_EXTENSION_DEP(yar, msgpack, true)
    ])
  fi
  PHP_SUBST(YAR_SHARED_LIBADD)
fi
