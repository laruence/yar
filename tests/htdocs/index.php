<?php
error_reporting(-1);
class Service_Provider {
    /**
     * @param interge $uid
     * @param string $version
     * @return string
     */
    public function normal($uid, $version = "3.6") {
        return $version;
    }

    public function exception() {
        throw new Exception("server exceptin");
    }

    public function output() {
        echo "output";
        return "success";
	}

	public function strlen($str) {
		return strlen($str);
	}

	public function header($name) {
		$key = "HTTP_" . strtoupper($name);
		return isset($_SERVER[$key])? $_SERVER[$key] : NULL;
	}

	public function usleep($usec) {
		usleep($usec);
		return 1;
	}

    protected function invisible() {
    }
}

$yar = new Yar_Server(new Service_Provider());
$yar->handle();
