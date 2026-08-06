--TEST--
Yar server info page renders long prototypes correctly
--SKIPIF--
<?php
if (!extension_loaded("yar")) {
    print "skip";
}
?>
--FILE--
<?php
include "yar.inc";

/* a prototype rendering longer than the initial 1024 bytes buffer forces
   REALLOC_BUF_IF_EXCEED to grow it; that macro used to leave the offset
   pointer dangling after erealloc() (use-after-free) */
$params = array();
for ($i = 0; $i < 64; $i++) {
	$params[] = '$p' . sprintf("%02d", $i) . ' = "default value for padding ' . sprintf("%02d", $i) . '"';
}
$code = "<?php\nclass Long_Signature_Service {\n"
	. "\tpublic function many(" . implode(", ", $params) . ") {\n\t\treturn 1;\n\t}\n"
	. "}\n"
	. '$yar = new Yar_Server(new Long_Signature_Service());' . "\n"
	. '$yar->handle();' . "\n";

yar_server_start($code);

$html = file_get_contents(YAR_API_ADDRESS); /* GET renders the info page */

var_dump(strpos($html, "Long_Signature_Service::many") !== false);
var_dump(strpos($html, '$p63') !== false);
/* string defaults are truncated to 10 chars followed by "..." */
var_dump(strpos($html, "'default va...'") !== false);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
