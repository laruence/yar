/* This is a generated file, edit the .stub.php file instead.
 * Stub hash: c76c64f60b5bed6bfd71a7eebc17cac0eadf066a */

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Yar_Server___construct, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, executor, IS_OBJECT, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Yar_Server_handle, 0, 0, _IS_BOOL, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_class_Yar_Client___construct, 0, 0, 1)
	ZEND_ARG_TYPE_INFO(0, uri, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 0, "NULL")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Yar_Client_call, 0, 2, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, method, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, arguments, IS_ARRAY, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Yar_Client_getOpt, 0, 1, IS_MIXED, 0)
	ZEND_ARG_TYPE_INFO(0, type, IS_LONG, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_OBJ_TYPE_MASK_EX(arginfo_class_Yar_Client_setOpt, 0, 2, Yar_Client, MAY_BE_BOOL)
	ZEND_ARG_TYPE_INFO(0, type, IS_LONG, 0)
	ZEND_ARG_TYPE_INFO(0, value, IS_MIXED, 0)
ZEND_END_ARG_INFO()

#define arginfo_class_Yar_Client___call arginfo_class_Yar_Client_call

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_class_Yar_Concurrent_Client_call, 0, 2, MAY_BE_NULL|MAY_BE_LONG|MAY_BE_BOOL)
	ZEND_ARG_TYPE_INFO(0, uri, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO(0, method, IS_STRING, 0)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, arguments, IS_ARRAY, 1, "NULL")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, callback, IS_CALLABLE, 1, "NULL")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, error_callback, IS_CALLABLE, 1, "NULL")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, options, IS_ARRAY, 1, "NULL")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Yar_Concurrent_Client_loop, 0, 0, _IS_BOOL, 1)
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, callback, IS_CALLABLE, 1, "NULL")
	ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, error_callback, IS_CALLABLE, 1, "NULL")
ZEND_END_ARG_INFO()

#define arginfo_class_Yar_Concurrent_Client_reset arginfo_class_Yar_Server_handle

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_MASK_EX(arginfo_class_Yar_Server_Exception_getType, 0, 0, MAY_BE_STRING|MAY_BE_LONG)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_class_Yar_Client_Exception_getType, 0, 0, IS_STRING, 0)
ZEND_END_ARG_INFO()
