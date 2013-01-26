<?php

/*
// CLIENT EXAMPLE CODE:
// use a remote fastfile key to transfer a local file to a remote destination.
// -------------------------------------------------------------------------------------------------------------------
require_once('./fastfile.php');
try {
	// setup our fastfile client object to send to <destionation> using <key>.
	$client = new Fastfile('HOSTNAME_HERE', 'KEY_HERE');

	// apply any custom/special options to the request. (complete list below)
//	$client	->port(8080)				// alternate HTTP port.
//		->ssl(true)				// use HTTPS. (changes default port to 443, unless specified)
//		->timeout(20)				// give up if idle activity timeout is exceeded. (seconds)
//		->bufSize(16384)			// (write) buffer size.
//		->connections(15)			// number of async connections to send data on.
//		->failedConnections(50)			// give up after X number of failed connections.
//		->ipv(6)				// explicit ipv4 or ipv6 protocol.
//		->location('fastfile')			// alternate mod_fastfile location.
//		->chunkSize(2097152)			// break file into X byte size chunks.
//		->host('alt.domain.com')		// specify explicit "Host" header. (does not effect what is connected to)
//		->header('HTTP-Header', 'data');	// specify arbitrary HTTP headers. (multiple headers allowed)

	// transfer the file to the server.
	$client->send('PATH_TO_LOCAL_FILE_HERE');

	print "Fastfile client successfully transferred file.\n";
}
catch (Exception $e) {
	print "Fastfile client error: ".$e->getMessage()."\n";
}
// -------------------------------------------------------------------------------------------------------------------



// SERVER EXAMPLE CODE:
// make a fastfile key (and associated files) to be handed down to a client via whatever means the programmer desires.
// -------------------------------------------------------------------------------------------------------------------
require_once('./fastfile.php');
try {
	$destFile = 'PATH_TO_DESTINATION_FILE_HERE'; // the file the key is associated with. (where client file will be written)
	$key = Fastfile::server($destFile, 0); // optional 2nd argument to specify the maximum filesize. (0=unlimited)
	print "Key ready to be handed down to the client: $key\n";
}
catch (Exception $e) {
	print "Fastfile::server error: ".$e->getMessage()."\n";
}
// -------------------------------------------------------------------------------------------------------------------
*/

class Fastfile {

	/* imported constants/defines from fastfile.h */
	const FF_CONF_PATH = '/etc/fastfile.conf';
	const FF_POOL_DIR_MODE = 2733;
	const FF_POOL_FILE_MODE = 0660;
	const FF_KEY_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
	const FF_KEY_LEN = 32;
	const FF_CLIENT = 'ffcp';

	/* ------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
	/* client configuration array that holds all options. */
	private $client = array();

	/* CLIENT METHOD: create a new client instance. */
	public function __construct($hostname, $key) {
		$this->client['hostname'] = $hostname;
		$this->client['key'] = $key;
	}

	/* CLIENT METHOD: a catch-all method to handle all client options. */
	public function __call($method, $args) {
		switch($method) {
			case 'ssl':
			case 'location':
			case 'bufSize':
			case 'timeout':
			case 'chunkSize':
			case 'connections':
			case 'failedConnections':
			case 'host':
			case 'port':
			case 'ipv':
				if(count($args) > 0)
					$this->client[$method] = $args[0];
				return($this);

			/* header option supports multiples so it gets special treatment. */
			case 'header':
				if(empty($this->client['header']) || !is_array($this->client['header']))
					$this->client['header'] = array();
				$this->client['header'][$args[0]] = $args[1];
				return($this);
		}
		throw new Exception("unknown fastfile client method: $method");
	}

	/* CLIENT METHOD: send the file to the remote apache/mod_fastfile server. */
	public function send($file) {

		/* make sure the file exists that we want to send. */
		if(!file_exists($file))
			throw new Exception("could not find file to send: $file");

		/* map class options to ffcp command-line options. */
		$map = array(
			'ssl'			=> ' -s',
			'location'		=> ' -l ',
			'bufSize'		=> ' -b ',
			'timeout'		=> ' -t ',
			'chunkSize'		=> ' -c ',
			'connections'		=> ' -n ',
			'failedConnections'	=> ' -f ',
			'host'			=> ' -h ',
			'port'			=> ' -p ',
			'ipv'			=> ' -',
			'header'		=> ' -H '
		);

		/* setup initial command and add any special options using the map above. */
		$cmd = $this->clientFindFfcp();
		$cmd .= ' -q';
		foreach($this->client as $option => $value) {
			if(empty($map[$option]))
				continue;
			switch($option) {
				case 'header':
					foreach($value as $header => $headerData)
						$cmd .= $map[$option].escapeshellarg("$header=$headerData");
					break;
				case 'ssl':
					if(!empty($value))
						 $cmd .= $map[$option];
					break;
				case 'ipv':
					if(!in_array($value, array('4', '6')))
						break; /* continue into default. */
				default:
					$cmd .= $map[$option].escapeshellarg($value);
					break;
			}
		}

		/* append host, key and the file being sent and the command is ready to go. */
		$cmd .= ' '.escapeshellarg($this->client['hostname']).' '.escapeshellarg($this->client['key']).' '.escapeshellarg($file);

		/* prepare and execute the ffcp process. */
		$pipeInfo = array(
			0 => array('pipe', 'r'),
			1 => array('pipe', 'w'),
			2 => array('pipe', 'w')
		);
		$proc = proc_open($cmd, $pipeInfo, $pipes);
		if(!is_resource($proc))
			throw new Exception('lost '.self::FF_CLIENT.' execution process.');

		/* see if we have an error string to store. */
		$ret = stream_get_contents($pipes[2]);

		/* ffcp reported an error occured. */
		if(proc_close($proc)) {
			if($ret)
				throw new Exception(trim($ret));
			else
				throw new Exception(self::FF_CLIENT.' execution process failed');
		}

		/* if we made it to this point without an exception it's a success. */
		return;
	}

	/* CLIENT METHOD: find the ffcp binary path, abort if not found. */
	private function clientFindFfcp() {
		if(($paths = getenv('PATH'))) {
			foreach(explode(':', $paths) as $path) {
				$testPath = $path . '/' . self::FF_CLIENT;
				if(file_exists($testPath))
					return($testPath);
			}
		}
		throw new Exception("could not find '".self::FF_CLIENT."' binary (included with fastfile) required for transmission, check your \$PATH and make sure ".self::FF_CLIENT." is installed.");
	}


	/* ------------------------------------------------------------------------------------------------------------------------------------------------------------------- */
	/* SERVER METHOD: populate the pool directory with a new key, return the newly created key to be handed off the client by whatever means determined by the programmer. */
	public static function server($path, $maxSize=0) {

		/* arrayize fastfile.conf and make sure it's up to snuff. */
		$conf = self::serverParseConf(self::FF_CONF_PATH);

		/* see if "pool" definition exists in the config file. */
		if(empty($conf['pool']))
			throw new Exception("pool directory not set in configuration file: ".self::FF_CONF_PATH);

		/* make sure the pool dir is set up properly. (throws an exception on failure) */
		self::serverValidatePoolDir($conf['pool']);

		/* see if the destination file exists already and overwrite isn't enabled. */
		if(file_exists($path) && (empty($conf['overwrite']) || strtolower($conf['overwrite']) != 'yes'))
			throw new Exception("file already exists: $path");

		return(self::serverGenerateKey($conf['pool'], $path, $maxSize));
	}

	/* SERVER METHOD: create an automatically generated key (and associated files) on the file system for use by the client. */
	private function serverGenerateKey($pool, $path, $maxSize=0) {

		/* loop until we find a key that isn't in use. */
		do {
			/* prepare our files for this key. */
			$key = self::serverMakeKey();
			$poolKey = "$pool/$key";
			$poolDat = "$poolKey.dat";
			$poolLnk = "$poolKey.lnk";


		} while (file_exists($poolKey) || file_exists($poolDat) || file_exists($poolLnk) || is_link($poolLnk));

		/* attempt to create the data file associated with the new key. */
		if(($fh = fopen($poolDat, 'w')) === false)
			throw new Exception("could not create data file in pool directory: $poolDat");

		/* if specified, make the file the specified size for the client to fill in. */
		if($maxSize > 0 && !ftruncate($fh, $maxSize)) {
			fclose($fh);
			unlink($poolDat);
			throw new Exception("could not create data file with the size desired: $poolDat ($maxSize bytes)");
		}
		fclose($fh);

		/* make the link destination, where the file will go when complete. */
		if(!symlink($path, $poolLnk)) {
			unlink($poolDat);
			throw new Exception("could not create symbolic link to destination: $poolLnk -> $path");
		}

		/* create the pool key itself, to be filled in by client data. */
		if(!touch($poolKey)) {
			unlink($poolDat);
			unlink($poolLnk);
			throw new Exception("could not create key file to destination: $poolKey");
		}

		/* set our permissions accordingly for the key and data file. */
		chmod($poolDat, self::FF_POOL_FILE_MODE);
		chmod($poolKey, self::FF_POOL_FILE_MODE);

		return($key);
	}

	/* SERVER METHOD: generate a new (un-validated) fastfile key. */
	private function serverMakeKey() {
		$key = '';
		for($i=0, $len=strlen(self::FF_KEY_CHARS)-1; $i < self::FF_KEY_LEN; $i++)
			$key .= substr(self::FF_KEY_CHARS, mt_rand(0, $len), 1);
		return($key);
	}

	/* SERVER METHOD: validate that the pool directory is setup correctly. */
	private function serverValidatePoolDir($dir) {

		/* see if the pool directory exists. */
		if(!file_exists($dir))
			throw new Exception("pool directory is inaccessible or does not exist: $dir");

		/* make sure the pooldir has the correct permissions.  (pretty funky way of getting octal mode values) */
		if(substr(sprintf("%o", fileperms($dir)), -4) != self::FF_POOL_DIR_MODE)
			throw new Exception("pool directory has incorrect permissions: $dir (should be mode ".self::FF_POOL_DIR_MODE.")");

		return;
	}

	/* SERVER METHOD: all-puroise fastfile.conf config parser, returns all options or specified option. */
	private function serverParseConf($conf, $option=null) {

		/* see if the conf file exists. */
		if(!file_exists($conf) || ($lines = file($conf, FILE_IGNORE_NEW_LINES | FILE_SKIP_EMPTY_LINES)) === false)
			throw new Exception("could not access config file: $conf");

		$ret = array();
		foreach($lines as $line) {

			/* skip empty lines and comments. */
			if(strlen($line) < 1 || strstr('#;', $line[0]))
				continue;

			/* split array up and trim the name and value. */
			$val = array_map('trim', explode('=', $line, 2));

			/* skip empty name=value pairs. */
			if(empty($val[0]) || empty($val[1]))
				continue;

			/* add value to our array of all options. (will overwrite) */
			$ret[$val[0]] = $val[1];
		}

		/* if we're only after a particular option just return that. */
		if(!empty($option)) {
			if(empty($ret[$option]))
				throw new Exception("config file did not contain option: $option ($conf)");

			return($ret[$option]);
		}

		/* ... or just return the whole thing. */
		return($ret);
	}
}

?>
