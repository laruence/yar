<?php
error_reporting(-1);
class Service_Provider {
    protected function __info($markup) {
		if (strpos($markup, "Service_Provider")) {
			return "okay";
		}
    }
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
