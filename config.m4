dnl $Id$
dnl config.m4 for extension yar

PHP_ARG_ENABLE(yar, whether to enable yar support,
[  --enable-yar           Enable yar support])

PHP_ARG_WITH(curl, for curl protocol support,
[  --with-curl[=DIR]       Include curl protocol support])

PHP_ARG_ENABLE(msgpack, for msgpack packager support,
[  --enable-msgpack Include msgpack packager support], no, no)

if test "$PHP_YAR" != "no"; then
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
  PHP_EVAL_LIBLINE($CURL_LIBS, YAR_SHARED_LIBADD)
  PHP_ADD_LIBRARY_WITH_PATH(curl, $CURL_DIR/$PHP_LIBDIR, YAR_SHARED_LIBADD)

  PHP_NEW_EXTENSION(yar, yar.c yar_server.c yar_client.c yar_request.c yar_response.c yar_exception.c yar_packager.c yar_protocol.c packagers/php.c packagers/json.c packagers/msgpack.c yar_transport.c transports/curl.c, $ext_shared)
  if test "$PHP_MSGPACK" != "no"; then
    AC_DEFINE(ENABLE_MSGPACK,1,[enable msgpack packager])
    ifdef([PHP_ADD_EXTENSION_DEP],
    [
    PHP_ADD_EXTENSION_DEP(yar, msgpack, true)
    ])
  fi
  PHP_SUBST(YAR_SHARED_LIBADD)
fi
