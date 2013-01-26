Fastfile client is the "ffcp" shell command.  The following steps illustrate how to get it up and running.

1. Ensure OpenSSL and crypto libraries are installed if SSL (-s argument of ffcp) is expected to be supported.  (not required)

2. As root, inside of the client subdirectory of this package, run the following:

		./configure --prefix=/usr && make && make install

	Or, for cygwin:

		./configure CFLAGS=-static && make

3. Test functionality using a key provided by a remote (server-side) fastfile installation:

	ffcp server.com <KEY_HERE> <PATH_TO_FILE_TO_SEND_HERE>

4. View the api subdirectory of this package to see if your desired programming language is supported.
