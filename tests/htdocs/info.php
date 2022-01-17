<?php
error_reporting(-1);
class Service_Provider {
	private $result = "okay";
    protected function __info() {
		return <<<HTML
{$this->result}
HTML;
    }
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
