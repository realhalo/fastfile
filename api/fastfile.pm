# -------------------------------------------------------------------------------------------------------------------
# CLIENT EXAMPLE CODE:
# use a remote fastfile key to transfer a local file to a remote destination.
# -------------------------------------------------------------------------------------------------------------------
=pod
use fastfile;
eval {
	# setup our fastfile client object to send to <destionation> using <key>.
	my $client = Fastfile->new('HOSTNAME_HERE', 'KEY_HERE');

	# apply any custom/special options to the request. (complete list below)
	#$client ->set('port', 8080)			# alternate HTTP port.
	#	->set('ssl', 1)				# use HTTPS. (changes default port to 443, unless specified)
	#	->set('timeout', 20)			# give up if idle activity timeout is exceeded. (seconds)
	#	->set('bufSize', 16384)			# (write) buffer size.
	#	->set('connections', 15)		# number of async connections to send data on.
	#	->set('failedConnections', 50)		# give up after X number of failed connections.
	#	->set('ipv', 6)				# explicit ipv4 or ipv6 protocol.
	#	->set('location', 'fastfile')		# alternate mod_fastfile location.
	#	->set('chunkSize', 2097152)		# break file into X byte size chunks.
	#	->set('host', 'alt.domain.com')		# specify explicit "Host" header. (does not effect what is connected to)
	#	->set('header', 'HTTP-Header', 'data');	# specify arbitrary HTTP headers. (multiple headers allowed)

	# transfer the file to the server.
	$client->send('PATH_TO_LOCAL_FILE_HERE');

	print "Fastfile client successfully transferred file.\n";
};
print "Fastfile client error: $@\n" if $@;
=cut


# -------------------------------------------------------------------------------------------------------------------
# SERVER EXAMPLE CODE:
# make a fastfile key (and associated files) to be handed down to a client via whatever means the programmer desires.
# -------------------------------------------------------------------------------------------------------------------
=pod
use fastfile;
eval {
	my $destFile = 'PATH_TO_DESTINATION_FILE_HERE'; # the file the key is associated with. (where client file will be written)
	my $key = Fastfile::server($destFile, 0); # optional 2nd argument to specify the maximum filesize. (0=unlimited)
	print "Key ready to be handed down to the client: $key\n";
};
print "Fastfile::server error: $@\n" if $@;
=cut


# -------------------------------------------------------------------------------------------------------------------
# FASTFILE MODULE / PACKAGE
# -------------------------------------------------------------------------------------------------------------------
package Fastfile;

use strict;
use warnings;


# imported constants/defines from fastfile.h.
$Fastfile::FF_CONF_PATH		= '/etc/fastfile.conf';
$Fastfile::FF_POOL_DIR_MODE	= 2733;
$Fastfile::FF_POOL_FILE_MODE	= 0660;
$Fastfile::FF_KEY_CHARS		= 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789';
$Fastfile::FF_KEY_LEN		= 32;
$Fastfile::FF_CLIENT		= 'ffcp';


# -------------------------------------------------------------------------------------------------------------------
# CLIENT METHOD: create a new client instance.
sub new {
	my ($class, $hostname, $key) = @_;
	my $self = {
		hostname => $hostname,
		key => $key,
		header => {}
	};
	bless($self, $class);
	return($self);
}

# CLIENT METHOD: set all client options.
sub set {
	my $self = shift;
	my ($name, $value, $value2) = @_;
	die "no value supplied to set for: $name" unless $value;
	if($name ~~ ['ssl', 'location', 'bufSize', 'timeout', 'chunkSize', 'connections', 'failedConnections', 'host', 'port', 'ipv']) {
		$self->{$name} = $value;
		return($self);
	}

	# header option supports multiples so it gets special treatment.
	elsif($name eq 'header') {
		$self->{header}->{$value} = $value2;
		return($self);
	}
	die "no such fastfile option: $name";
}

# CLIENT METHOD: send the file to the remote apache/mod_fastfile server.
sub send {
	my $self = shift;
	my ($file) = @_;

	# make sure the file exists that we want to send. */
	die "could not find file to send: $file" unless -e $file;

	# map class options to ffcp command-line options. */
	my %map = (
		'ssl',			'-s',
		'location',		'-l',
		'bufSize',		'-b',
		'timeout',		'-t',
		'chunkSize',		'-c',
		'connections',		'-n',
		'failedConnections',	'-f',
		'host',			'-h',
		'port',			'-p',
		'ipv',			'-',
		'header',		'-H'
	);

	# setup initial command and add any special options using the map above. (errors to stdout and quiet mode)
	my @cmd = ($self->clientFindFfcp(), '-e', '-q');
	foreach my $key (keys %{$self}) {

		next unless defined $map{$key};

		if($key eq 'header') {
			foreach my $hKey (keys $self->{header}) {
				push(@cmd, $map{$key});
				push(@cmd, $hKey.'='.$self->{header}{$hKey});
			}
		}
		elsif($key eq 'ssl') {
			push(@cmd, $map{$key}) if $self->{$key};
		}
		elsif($key eq 'ipv') {
			push(@cmd, $map{$key}.$self->{$key}) if $self->{$key} ~~ ['4', '6'];
		}
		else {
			push(@cmd, $map{$key});
			push(@cmd, $self->{$key});
		}
	}

	# append host, key and the file being sent and the command is ready to go. 
	push(@cmd, $self->{hostname});
	push(@cmd, $self->{key});
	push(@cmd, $file);

	# prepare and execute the ffcp process, dump errors on failure.
	open(FFCP, '-|', @cmd);
	my $last;
	while(<FFCP>) {
		chomp($_);
		$last = $_ if $_;
	}
	close(FFCP);
	die "$Fastfile::FF_CLIENT execution process failed: $last" if $?;
}

# CLIENT METHOD: find the ffcp binary path, abort if not found.
sub clientFindFfcp {
	my $self = shift;
	foreach my $path (split(':', $ENV{PATH})) {
		my $testPath = $path . '/' . $Fastfile::FF_CLIENT;
		return($testPath) if (-e $testPath);
	}
	die "could not find '$Fastfile::FF_CLIENT' binary (included with fastfile) required for transmission, check your \$PATH and make sure $Fastfile::FF_CLIENT is installed.";
}


# -------------------------------------------------------------------------------------------------------------------
# SERVER METHOD: populate the pool directory with a new key, return the newly created key to be handed off the client by whatever means determined by the programmer.
sub server {
	my ($path, $maxSize) = @_;
	my (%conf);

	# arrayize fastfile.conf and make sure it's up to snuff.
	%conf = serverParseConf($Fastfile::FF_CONF_PATH);

	# see if "pool" definition exists in the config file. */
	die "pool directory not set in configuration file: $Fastfile::FF_CONF_PATH" unless $conf{'pool'};

	# make sure the pool dir is set up properly. (throws an exception on failure) */
	serverValidatePoolDir($conf{'pool'});
	# see if the destination file exists already and overwrite isn't enabled. */
	die "file already exists: $path" if(-e $path && (!$conf{'overwrite'} || lc($conf{'overwrite'}) ne 'yes'));

	serverGenerateKey($conf{'pool'}, $path, $maxSize);
}

# SERVER METHOD: generate a new (un-validated) fastfile key.
sub serverGenerateKey {
	my ($pool, $path, $maxSize) = @_;
	my ($key, $poolKey, $poolDat, $poolLnk);

	# loop until we find a key that isn't in use.
	do {
		# prepare our files for this key.
		$key = serverMakeKey();
		$poolKey = "$pool/$key";
		$poolDat = "$poolKey.dat";
		$poolLnk = "$poolKey.lnk";
	} while(-e $poolKey || -e $poolDat || -e $poolLnk || -l $poolLnk);


	# attempt to create the data file associated with the new key.
	open(FH, ">", $poolDat) || die "could not create data file in pool directory: $poolDat";

	# if specified, make the file the specified size for the client to fill in.
	if($maxSize && $maxSize > 0 && !truncate(FH, $maxSize)) {
		close(FH);
		unlink($poolDat);
		die "could not create data file with the size desired: $poolDat ($maxSize bytes)";
	}
	close(FH);

	# make the link destination, where the file will go when complete.
	if(!symlink($path, $poolLnk)) {
		unlink($poolDat);
		die "could not create symbolic link to destination: $poolLnk -> $path";
	}

	# create the pool key itself, to be filled in by client data.
	if(!open(FH, ">", $poolKey)) {
		unlink($poolDat);
		unlink($poolLnk);
		die "could not create key file to destination: $poolKey";
	}
	close(FH);

	# set our permissions accordingly for the key and data file. 
	chmod($Fastfile::FF_POOL_FILE_MODE, $poolDat);
	chmod($Fastfile::FF_POOL_FILE_MODE, $poolKey);

	$key;
}

# SERVER METHOD: generate a new (un-validated) fastfile key.
sub serverMakeKey {
	my $key = '';
	for(1 .. $Fastfile::FF_KEY_LEN) {
		$key .= substr($Fastfile::FF_KEY_CHARS, int(rand(length($Fastfile::FF_KEY_CHARS))), 1);
	}
	$key;
}

# SERVER METHOD: validate that the pool directory is setup correctly.
sub serverValidatePoolDir {
	my ($dir) = @_;
	my @stat;

	# see if the pool directory exists.
	die "pool directory is inaccessible or does not exist: $dir" unless -e $dir;

	# see if the pool directory can be stat'd.
	die "pool directory permissions could not be accessed: $dir" unless (@stat = stat($dir));

	# make sure the pooldir has the correct permissions.
	die "pool directory has incorrect permissions: $dir (should be mode $Fastfile::FF_POOL_DIR_MODE)" unless substr(sprintf("%04o", $stat[2]), -4) == $Fastfile::FF_POOL_DIR_MODE;
}

# SERVER METHOD: all-puroise fastfile.conf config parser, returns all options or specified option.
sub serverParseConf {
	my ($conf, $option) = @_;
	my (@val, %ret);

	open(F, $conf) || die "could not open $conf: $!\n";
	foreach my $line (<F>) {
		chomp($line);

		# skip empty lines and comments.
		next if(length($line) < 1 || index('#;', substr($line, 0, 1)) >= 0);

		# split array up and trim the name and value.
		@val = split('=', $line, 2);
		$val[0] =~ s/^\s+//; $val[0] =~ s/\s+$//;
		$val[1] =~ s/^\s+//; $val[1] =~ s/\s+$//;

		# add value to our array of all options, if there is a name and value.
		$ret{$val[0]} = $val[1] unless (length($val[0]) < 1 || length($val[1]) < 1);
	}
	close(F);

	# make sure we have the option if specified.
	die "config file did not contain option: $option ($conf)" if ($option && !$ret{$option});

	# return array of options or specified option.
	$option ? $ret{$option} : %ret;
}


# so the require or use succeeds.
1;
