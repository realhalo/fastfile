# -------------------------------------------------------------------------------------------------------------------
# CLIENT EXAMPLE CODE:
# use a remote fastfile key to transfer a local file to a remote destination.
# -------------------------------------------------------------------------------------------------------------------
''' 
from fastfile import Fastfile
try:
	# setup our fastfile client object to send to <destionation> using <key>.
	client = Fastfile("HOSTNAME_HERE", "KEY_HERE")

	# apply any custom/special options to the request. (complete list below)
	#client.port(8080)				# alternate HTTP port.
	#client.ssl(True)				# use HTTPS. (changes default port to 443, unless specified)
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

	print "Fastfile client successfully transferred file."
except Exception, e:
	print "Fastfile client error: " + e.message
'''


# -------------------------------------------------------------------------------------------------------------------
# SERVER EXAMPLE CODE:
# make a fastfile key (and associated files) to be handed down to a client via whatever means the programmer desires.
# -------------------------------------------------------------------------------------------------------------------
''' 
from fastfile import Fastfile
try:
	destFile = "PATH_TO_DESTINATION_FILE_HERE" # the file the key is associated with. (where client file will be written)
	key = Fastfile.server(destFile, 0) # optional 2nd argument to specify the maximum filesize. (0=unlimited)
	print "Key ready to be handed down to the client: " + key
except Exception, e:
	print "Fastfile.server error: " + e.message
'''


# -------------------------------------------------------------------------------------------------------------------
# FASTFILE CLASS
# -------------------------------------------------------------------------------------------------------------------
import os, sys, random, subprocess
class Fastfile:

	# imported constants/defines from fastfile.h
	FF_CONF_PATH = "/etc/fastfile.conf"
	FF_POOL_DIR_MODE = 2733
	FF_POOL_FILE_MODE = 0660
	FF_KEY_CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"
	FF_KEY_LEN = 32
	FF_CLIENT = "ffcp"


	# -------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# CLIENT METHOD: create a new client instance.
	def __init__(self, hostname, key):
		self.client = {"hostname": hostname, "key": key, "header": {}}

	# CLIENT METHOD: a catch-all method to handle all client options.
	def __getattr__(self, name):
		if name in ["ssl", "location", "bufSize", "timeout", "chunkSize", "connections", "failedConnections", "host", "port", "ipv", "header"]:
			def method(*args):
				if name == "header":
					if len(args) == 2:
						self.client["header"][str(args[0])] = str(args[1])
				elif len(args) == 1:
					self.client[name] = str(args[0])
				return self
			return method
                raise Exception("unsupported fastfile attribute: " + name)

	# CLIENT METHOD: send the file to the remote apache/mod_fastfile server.
	def send(self, file):
		if not os.path.exists(file):
			raise Exception("could not find file to send: " + file)

		# map class options to ffcp command-line options.
		map = {
			"ssl"			: "-s",
			"location"		: "-l",
			"bufSize"		: "-b",
			"timeout"		: "-t",
			"chunkSize"		: "-c",
			"connections"		: "-n",
			"failedConnections"	: "-f",
			"host"			: "-h",
			"port"			: "-p",
			"ipv"			: "-",
			"header"		: "-H"
		}

		# setup initial command and add any special options using the map above.
		cmd = [self.clientFindFfcp(), "-q"]

		for (name, value) in self.client.items():
			if not map.get(name):
				continue
			elif name == "header":
				for (hName, hValue) in self.client["header"].items():
					cmd.append(map[name])
					cmd.append(hName + "=" + hValue)
			elif name == "ssl":
				if value:
					cmd.append(map[name])
			elif name == "ipv":
				if value in ['4', '6']:
					cmd.append(map[name] + value)
			else:
				cmd.append(map[name])
				cmd.append(value)

		# append host, key and the file being sent and the command is ready to go.
		cmd.append(self.client["hostname"])
		cmd.append(self.client["key"])
		cmd.append(file)
				
		# execute the ffcp process.  errors from ffcp, if any, will be return in the exception. (shell escape not necessary as popen is set to not use a shell)
		try:
			proc = subprocess.Popen(cmd, shell=False, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
			ret = proc.communicate()
			if proc.returncode != 0:
				raise Exception(ret[0])
		except Exception, e:
			raise Exception(Fastfile.FF_CLIENT + " execution process failed: " + e.message)

	# CLIENT METHOD: find the ffcp binary path, abort if not found.
	def clientFindFfcp(self):
		paths = os.environ.get("PATH")
		if isinstance(paths, str):
			for path in paths.split(":"):
				testPath = path + "/" + Fastfile.FF_CLIENT
				if os.path.exists(testPath):
					return testPath
		raise Exception("could not find " + Fastfile.FF_CLIENT + " binary (included with fastfile) required for transmission, check your $PATH and make sure " + Fastfile.FF_CLIENT + " is installed.")


	# -------------------------------------------------------------------------------------------------------------------------------------------------------------------
	# SERVER METHOD: populate the pool directory with a new key, return the newly created key to be handed off the client by whatever means determined by the programmer.
	@staticmethod
	def server(path, maxSize=0):

		# arrayize fastfile.conf and make sure it's up to snuff.
		conf = Fastfile.serverParseConf(Fastfile.FF_CONF_PATH)

                # see if "pool" definition exists in the config file.
		if conf.get("pool") == None:
			raise Exception("pool directory not set in configuration file: " + Fastfile.FF_CONF_PATH)

		# make sure the pool dir is set up properly. (throws an exception on failure)
		Fastfile.serverValidatePoolDir(conf.get("pool"))

                # see if the destination file exists already and overwrite isn't enabled.
		if os.path.exists(path) and conf.get("overwrite") != "yes":
	                raise Exception("file already exists: " + path)

		# generate the key and return it.
		return Fastfile.serverGenerateKey(conf.get("pool"), path, maxSize)

	# SERVER METHOD: create an automatically generated key (and associated files) on the file system for use by the client.
	@staticmethod
	def serverGenerateKey(pool, path, maxSize=0):

		# loop until we find a key that isn't in use.
		while True:
			key = Fastfile.serverMakeKey()
			poolKey = pool + "/" + key
			poolDat = poolKey + ".dat"
			poolLnk = poolKey + ".lnk"
			if not os.path.exists(poolKey) and not os.path.exists(poolDat) and not os.path.exists(poolLnk) and not os.path.islink(poolLnk):
				break

		# attempt to create the data file associated with the new key.
		try:
			fh = open(poolDat, "w")
		except:
			raise Exception("could not create data file in pool directory: " + poolDat)

		# if specified, make the file the specified size for the client to fill in.
		if maxSize > 0:
			try:
				fh.truncate(maxSize)
			except:
				try:
					fh.close()
					os.unlink(poolDat)
				except:
					pass
				raise Exception("could not create data file with the size desired: " + poolDat + " (" + str(maxSize) + " bytes)")
		fh.close()

		# make the link destination, where the file will go when complete.
		try:
			os.symlink(path, poolLnk)
		except:
			try:
				os.unlink(poolDat)
			except:
				pass
			raise Exception("could not create symbolic link to destination: " + poolLnk + " -> " + path)

		# create the pool key itself, to be filled in by client data.
		try:
			fh = open(poolKey, "w")
		except:
			try:
				fh.close()
				os.unlink(poolDat)
				os.unlink(poolLnk)
			except:
				pass
			raise Exception("could not create key file to destination: " + poolKey)

		# set our permissions accordingly for the key and data file.
		try:
			os.chmod(poolDat, Fastfile.FF_POOL_FILE_MODE)
			os.chmod(poolKey, Fastfile.FF_POOL_FILE_MODE)
		except:
			raise Exception("could not assign file permissions to pool data or key file: " + key)

		return key

	# SERVER METHOD: generate a new (un-validated) fastfile key.
	@staticmethod
	def serverMakeKey():
		key = ""
		for i in range(Fastfile.FF_KEY_LEN):
			key += random.choice(Fastfile.FF_KEY_CHARS)
		return key

	# SERVER METHOD: validate that the pool directory is setup correctly.
	@staticmethod
	def serverValidatePoolDir(dir):
		if not os.path.exists(dir):
			raise Exception("pool directory is inaccessible or does not exist: " + dir)
		if int(("%o" % os.stat(dir).st_mode)[-4:]) != Fastfile.FF_POOL_DIR_MODE:
			raise Exception("pool directory has incorrect permissions: " + dir + " (should be mode " + str(Fastfile.FF_POOL_DIR_MODE) + ")")

	# SERVER METHOD: all-puroise fastfile.conf config parser, returns all options or specified option.
	@staticmethod
	def serverParseConf(conf, option=None):
		ret = {}
		try:
			f = open(conf, "r")
			try:
				for line in f.readlines():
					line = line.strip()

					# skip empty lines and comments.
					if len(line) < 1 or line.startswith(';') or line.startswith('#'):
						continue

					# split array up and trim the name and value.
					val = map(lambda s: s.strip(), line.split('=', 1))

					# add value to our array of all options, if there is a name and value
					if len(val[0]) > 0 and len(val[1]) > 0:
						ret[val[0]] = val[1]
			finally:
				f.close()

			# make sure we have the option if specified.
			if option != None and ret.get(option) == None:
				raise Exception("config file did not contain option: " + option + " (" + conf + ")")

			# return array of options or specified option.
			return ret if option == None else ret[option]

		except:
			raise Exception("could not access or read fastfile config file: " + Fastfile.FF_CONF_PATH);	
