<?php
error_reporting(-1);
class Service_Provider Extends SplFixedArray {
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
