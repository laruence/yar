<?php

/** @generate-legacy-arginfo */

class Yar_Server {

	/* properties */
	/* protected object $_executor = NULL; */

	/* methods */
	public function __construct(object $executor) {}

	public function handle():bool {}
}

class Yar_Client {
	/* properties */

	/* methods */
	public function __construct(string $uri, array $options = NULL) {}

	public function call(string $method, array $arguments):mixed {}

	public function getOpt(int $type):mixed {}

	public function setOpt(int $type, mixed $value):Yar_Client|bool {}

    /**
     * @implementation-alias Yar_Client::call
     */
	public function __call(string $method, array $arguments):mixed {}
}

class Yar_Concurrent_Client {
	/* properties */

	/* methods */
	public static function call(string $uri, string $method, ?array $arguments = NULL, ?callable $callback = NULL, ?callable $error_callback = NULL, ?array $options = NULL):null|int|bool {}

	public static function loop(?callable $callback = NULL, ?callable $error_callback = NULL):?bool {}

	public static function reset():bool {}
}

class Yar_Server_Exception extends Exception {

	/* methods */
	public function getType():string|int { }
}

class Yar_Client_Exception extends Exception {

	/* methods */
	public function getType():string { }
}

class Yar_Server_Request_Exception extends Yar_Server_Exception {
}

class Yar_Server_Protocol_Exception extends Yar_Server_Exception {
}

class Yar_Server_Packager_Exception extends Yar_Server_Exception {
}

class Yar_Server_Output_Exception extends Yar_Server_Exception {
}

class Yar_Client_Transport_Exception extends Yar_Client_Exception {
}

class Yar_Client_Protocol_Exception extends Yar_Client_Exception {
}

class Yar_Client_Packager_Exception extends Yar_Client_Exception {
}
