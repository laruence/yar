<?php

define("YAR_API_USER", "dummy");
define("YAR_API_PASS", "foo");

define("YAR_API_HOSTNAME", (getenv('YAR_API_HOSTNAME') ?: "localhost"));
define("YAR_API_PORT",     (getenv('YAR_API_PORT') ?: "8964"));
define("YAR_API_HOST", YAR_API_HOSTNAME . ":" . YAR_API_PORT);
define("YAR_API_URI",  "/");
define("YAR_API_ADDRESS", "http://" . YAR_API_HOST . YAR_API_URI);

function yar_server_start($doc_root = __DIR__ . "/htdocs", $cmd_args = "-dextension=" . __DIR__ . "/../modules/yar.so ") {
	$php_executable = (getenv('TEST_PHP_EXECUTABLE')?:PHP_BINARY);
	$tmp = getenv('TEST_PHP_ARGS');
	if ($tmp) {
		$cmd_args .= $tmp;
	}

	$descriptorspec = array(
		0 => STDIN,
		1 => STDOUT,
		2 => STDERR,
	);

	if (substr(PHP_OS, 0, 3) == 'WIN') {
		$cmd = "{$php_executable} -t {$doc_root} -n {$cmd_args} -S " . YAR_API_HOST;

		$handle = proc_open(addslashes($cmd), $descriptorspec, $pipes, $doc_root, NULL, array("bypass_shell" => true,  "suppress_errors" => true));
	} else {
		$cmd = "exec {$php_executable} -t {$doc_root} -n {$cmd_args} -S " . YAR_API_HOST . " 2>/dev/null";

		$handle = proc_open($cmd, $descriptorspec, $pipes, $doc_root);
	}
	
	// note: even when server prints 'Listening on localhost:8964...Press Ctrl-C to quit.'
	//       it might not be listening yet...need to wait until fsockopen() call returns
    $i = 0;
    while (($i++ < 30) && !($fp = @fsockopen(YAR_API_HOSTNAME, YAR_API_PORT))) {
        usleep(10000);
    }

    if ($fp) {
        fclose($fp);
    }

	register_shutdown_function(
		function($handle) {
			proc_terminate($handle);
		},
		$handle);
	// don't bother sleeping, server is already up
	// server can take a variable amount of time to be up, so just sleeping a guessed amount of time
	// does not work. this is why tests sometimes pass and sometimes fail. to get a reliable pass
	// sleeping doesn't work.
}
?>

