<?php
error_reporting(-1);
class Service_Provider {
    protected function __auth($provider, $token) {
        if (!$provider || $provider !== $token) {
            return false;
        } else {
            return true;
        }
    }

    public function normal($uid, $version = "3.6") {
        return $version;
    }
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
