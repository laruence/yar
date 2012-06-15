dnl $Id$
dnl config.m4 for extension hermes

PHP_ARG_ENABLE(hermes, whether to enable hermes support,
[  --enable-hermes           Enable hermes support])

PHP_ARG_WITH(curl, for curl protocol support,
[  --with-curl[=DIR]       Include curl protocol support])

PHP_ARG_ENABLE(msgpack, for msgpack packager support,
[  --enable-msgpack Include msgpack packager support], no, no)

if test "$PHP_HERMES" != "no"; then
  if test -r $PHP_CURL/include/curl/easy.h; then
    CURL_DIR=$PHP_CURL
  else
    AC_MSG_CHECKING(for cURL in default path)
    for i in /usr/local /usr; do
      if test -r $i/include/curl/easy.h; then
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
  
  PHP_ADD_INCLUDE($CURL_DIR/include)
  PHP_EVAL_LIBLINE($CURL_LIBS, HERMES_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(curl, $CURL_DIR/$PHP_LIBDIR, HERMES_SHARED_LIBADD)

  if test "$PHP_MSGPACK" != "no"; then
    AC_DEFINE(ENABLE_MSGPACK,1,[enable msgpack packager])
  fi

  PHP_NEW_EXTENSION(hermes, hermes.c hermes_server.c hermes_client.c hermes_request.c hermes_response.c hermes_exception.c hermes_packager.c hermes_protocol.c packagers/php.c packagers/json.c packagers/msgpack.c hermes_transport.c transports/curl.c, $ext_shared)
  PHP_SUBST(HERMES_SHARED_LIBADD)
fi
