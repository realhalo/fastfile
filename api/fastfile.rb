# -------------------------------------------------------------------------------------------------------------------
# CLIENT EXAMPLE CODE:
# use a remote fastfile key to transfer a local file to a remote destination.
# -------------------------------------------------------------------------------------------------------------------
=begin
require "./fastfile.rb"
begin
	# setup our fastfile client object to send to <destionation> using <key>.
	client = Fastfile.new("HOSTNAME_HERE", "KEY_HERE")

	# apply any custom/special options to the request. (complete list below)
	#client.port(8080)				# alternate HTTP port.
	#client.ssl(true)				# use HTTPS. (changes default port to 443, unless specified)
	#client.timeout(20)				# give up if idle activity timeout is exceeded. (seconds)
	#client.bufSize(16384)				# (write) buffer size.
	#client.connections(15)				# number of async connections to send data on.
	#client.failedConnections(50)			# give up after X number of failed connections.
	#client.ipv(6)					# explicit ipv4 or ipv6 protocol.
	#client.location("fastfile")			# alternate mod_fastfile location.
	#client.chunkSize(2097152)			# break file into X byte size chunks.
	#client.host("alt.domain.com")			# specify explicit "Host" header. (does not effect what is connected to)
	#client.header("HTTP-Header", "data")		# specify arbitrary HTTP headers. (multiple headers allowed)

	# transfer the file to the server.
	client.send("PATH_TO_LOCAL_FILE_HERE")

	puts "Fastfile client successfully transferred file."
rescue Exception => e
	puts "Fastfile client error: #{e.message}"
end
=end


# -------------------------------------------------------------------------------------------------------------------
# SERVER EXAMPLE CODE:
# make a fastfile key (and associated files) to be handed down to a client via whatever means the programmer desires.
# -------------------------------------------------------------------------------------------------------------------
=begin
require "./fastfile.rb"
begin
	destFile = "PATH_TO_DESTINATION_FILE_HERE" # the file the key is associated with. (where client file will be written)
	key = Fastfile::server(destFile, 0) # optional 2nd argument to specify the maximum filesize. (0=unlimited)
	puts "Key ready to be handed down to the client: #{key}"
rescue Exception => e
	puts "Fastfile::server error: #{e.message}"
end
=end


# -------------------------------------------------------------------------------------------------------------------
# FASTFILE CLASS
# -------------------------------------------------------------------------------------------------------------------
class Fastfile

	# imported constants/defines from fastfile.h
	FF_CONF_PATH = "/etc/fastfile.conf"
	FF_POOL_DIR_MODE = 2733
	FF_POOL_FILE_MODE = 0660
	FF_KEY_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
	FF_KEY_LEN = 32
	FF_CLIENT = "ffcp"


	# -------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# CLIENT METHOD: create a new client instance.
	def initialize(hostname, key)
		@client = {}
		@client["header"] = {}
		@client["hostname"] = hostname
		@client["key"] = key
	end

	# CLIENT METHOD: a catch-all method to handle all client options.
	def method_missing(*args)
		method = args[0].to_s
		case method
			when "ssl", "location", "bufSize", "timeout", "chunkSize", "connections", "failedConnections", "host", "port", "ipv"
				@client[method] = args[1] if args.length == 2
			when "header"
				@client["header"][args[1]] = args[2] if args.length == 3
		end
		self
	end

	# CLIENT METHOD: send the file to the remote apache/mod_fastfile server.
	def send(file)

		# make sure the file exists that we want to send.
		raise "could not find file to send: #{file}" if !File.exists?(file)

		# map class options to ffcp command-line options.
		map = {
			"ssl"			=> "-s",
			"location"		=> "-l",
			"bufSize"		=> "-b",
			"timeout"		=> "-t",
			"chunkSize"		=> "-c",
			"connections"		=> "-n",
			"failedConnections"	=> "-f",
			"host"			=> "-h",
			"port"			=> "-p",
			"ipv"			=> "-",
			"header"		=> "-H"
		}

		# setup initial command and add any special options using the map above. 
		cmd = [self.clientFindFfcp]
		cmd << "-q" # quiet (normal) output.
		@client.each { |name, value|
			case name
				when "header"
					@client["header"].each { |hName, hValue| cmd << map[name] << hName + "=" + hValue }
				when "ssl"
					cmd << map[name] if value.class.to_s == "TrueClass"
				when "ipv"
					cmd << map[name] + value.to_s if value.class.to_s == "Fixnum" and [4,6].include?(value)
				else
					cmd << map[name] << value.to_s if !map[name].nil? and ["Fixnum","String"].include?(value.class.to_s)
			end 
		}

		# append host, key and the file being sent and the command is ready to go.
		cmd << @client["hostname"] << @client["key"] << file

		# execute the ffcp process.  errors, if any, will dump to stderr. (shell escape not necessary as array-based system call uses no shell)
		system *cmd
		raise Fastfile::FF_CLIENT + " execution process failed: " + cmd.join(" ") + " (CHECK STDERR FOR DETAILS)" if !$?.success?

	end

	# CLIENT METHOD: find the ffcp binary path, abort if not found.
	def clientFindFfcp
		ENV["PATH"].split(":").each { |path|
			testPath = path + "/" + Fastfile::FF_CLIENT
			return testPath if File.executable?(testPath)
		} if !ENV["PATH"].nil?
		raise "could not find #{Fastfile::FF_CLIENT} binary (included with fastfile) required for transmission, check your $PATH and make sure #{Fastfile::FF_CLIENT} is installed."
	end


	# -------------------------------------------------------------------------------------------------------------------------------------------------------------------
        # SERVER METHOD: populate the pool directory with a new key, return the newly created key to be handed off the client by whatever means determined by the programmer.
	def self.server(path, maxSize=0)

		# arrayize fastfile.conf and make sure it's up to snuff.
		conf = self::serverParseConf(self::FF_CONF_PATH)

		# see if "pool" definition exists in the config file.
		raise "pool directory not set in configuration file: #{self::FF_CONF_PATH}" if conf["pool"].nil?
		
		# make sure the pool dir is set up properly. (throws an exception on failure)
		self::serverValidatePoolDir(conf["pool"])

		# see if the destination file exists already and overwrite isn't enabled.
		raise "file already exists: #{path}" if File.exists?(path) && (conf["overwrite"].nil? || conf["overwrite"] != "yes")

		self::serverGenerateKey(conf["pool"], path, maxSize)
	end

	# SERVER METHOD: create an automatically generated key (and associated files) on the file system for use by the client.
	def self.serverGenerateKey(pool, path, maxSize=0)

		# loop until we find a key that isn't in use.
		begin
			key = self::serverMakeKey
			poolKey = "#{pool}/#{key}"
			poolDat = "#{poolKey}.dat"
			poolLnk = "#{poolKey}.lnk"

		end while File.exists?(poolKey) || File.exists?(poolDat) || File.exists?(poolLnk) || File.symlink?(poolLnk)

		# attempt to create the data file associated with the new key.
		begin
			fh = File.open(poolDat, 'w')
		rescue
			raise "could not create data file in pool directory: #{poolDat}"
		end

		# if specified, make the file the specified size for the client to fill in.
		if maxSize > 0 then
			begin
				fh.truncate(maxSize)
			rescue
				fh.close
				File.unlink(poolDat)
				raise "could not create data file with the size desired: #{poolDat} (#{maxSize} bytes)"
			end
		end
		fh.close

		# make the link destination, where the file will go when complete.
		begin
			File.symlink(path, poolLnk)
		rescue
                        File.unlink(poolDat)
                        raise "could not create symbolic link to destination: #{poolLnk} -> #{path}"
		end

		# create the pool key itself, to be filled in by client data.
		begin
			File.open(poolKey, 'w') { }
		rescue
			File.unlink(poolDat)
			File.unlink(poolLnk)
			raise "could not create key file to destination: #{poolKey}"
		end

		# set our permissions accordingly for the key and data file.
		File.chmod(self::FF_POOL_FILE_MODE, poolDat, poolKey)

		key
	end

	# SERVER METHOD: generate a new (un-validated) fastfile key.
	def self.serverMakeKey
		(0..self::FF_KEY_LEN-1).map{
			self::FF_KEY_CHARS[rand(self::FF_KEY_CHARS.length), 1] 
		}.join
	end

	# SERVER METHOD: validate that the pool directory is setup correctly.
	def self.serverValidatePoolDir(dir)

		# see if the pool directory exists.
		raise "pool directory is inaccessible or does not exist: #{dir}" if !File.exists?(dir)

		# make sure the pooldir has the correct permissions.
		raise "pool directory has incorrect permissions: #{dir} (should be mode #{self::FF_POOL_DIR_MODE.to_s})" if ("%04o" % File.stat(dir).mode)[-4,4] != self::FF_POOL_DIR_MODE.to_s
	end

	# SERVER METHOD: all-puroise fastfile.conf config parser, returns all options or specified option. 
	def self.serverParseConf(conf, option=nil)
		ret = {}
		File.open(conf).each do |line|
			line.chomp!

			# skip empty lines and comments.
			next if line.length < 1 || !line.index(/[#;]/).nil?

			# split array up and trim the name and value.
			val = line.split('=', 2).map { |x| x.strip! }

			# add value to our array of all options, if there is a name and value.
			ret[val[0]] = val[1] unless val[0].length < 1 || val[1].length < 1
		end

		# make sure we have the option if specified.
		raise "config file did not contain option: #{option} (#{conf}) " if !option.nil? && ret[option].nil?

		# return array of options or specified option.
		option.nil? ? ret : ret[option]
	end
end
