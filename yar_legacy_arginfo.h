/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: f627ffbe0bc3ace1d517c32646e4a242a7e9d6dc */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Yar_Server___construct, 0, 0, 1)
	ZEND_ARG_INFO(0, executor)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Yar_Server_handle, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Yar_Client___construct, 0, 0, 1)
	ZEND_ARG_INFO(0, uri)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Yar_Client_call, 0, 0, 2)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, arguments)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Yar_Client_getOpt, 0, 0, 1)
	ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Yar_Client_setOpt, 0, 0, 2)
	ZEND_ARG_INFO(0, type)
	ZEND_ARG_INFO(0, value)
ZEND_END_ARG_INFO()

#define arginfo_class_Yar_Client___call arginfo_class_Yar_Client_call

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Yar_Concurrent_Client_call, 0, 0, 2)
	ZEND_ARG_INFO(0, uri)
	ZEND_ARG_INFO(0, method)
	ZEND_ARG_INFO(0, arguments)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, error_callback)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Yar_Concurrent_Client_loop, 0, 0, 0)
	ZEND_ARG_INFO(0, callback)
	ZEND_ARG_INFO(0, error_callback)
	ZEND_ARG_INFO(0, options)
ZEND_END_ARG_INFO()

#define arginfo_class_Yar_Concurrent_Client_reset arginfo_class_Yar_Server_handle

#define arginfo_class_Yar_Server_Exception_getType arginfo_class_Yar_Server_handle

#define arginfo_class_Yar_Client_Exception_getType arginfo_class_Yar_Server_handle
