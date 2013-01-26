Fastfile server contains two parts; an Apache2 module (mod_fastfile) and helper daemon (ffpool).  The following steps illustrate how to get it up and running.

1. Ensure apxs2 (/usr/bin/apxs2) is installed, this should be included in the apache2-dev package.

2. As root, inside of the server subdirectory of this package, run the following:

	./configure --prefix=/usr && make && make install

3. The install process will attempt to do this automatically; verify the following is in your apache2.conf(change the path if appropriate):

	LoadModule fastfile_module /usr/lib/apache2/modules/mod_fastfile.so
	<Location /__ff>
		SetHandler fastfile
	</Location>

4. The install process will attempt to do this automatically; verify the following is in your root crontab:

	*/5 * * * * /usr/sbin/ffpool

5. Review /etc/fastfile.conf and make any changes if needed.  Verify the pool directory in the config exists and has the following applied to it(where www-data is the group that Apache2 is running as):

	chown root:www-data /home/fastfile
	chmod 2733 /home/fastfile

	(this may be done automatically by simply re-saving the /etc/fastfile.conf file which triggers the ffpool daemon to create/fix this directory)

6. Restart Apache2.

7. Test to verify the Apache2 module is operation by running "curl http://localhost/__ff", you should see "405 Method Not Allowed" if the module is functional.

8. Test to verify the ffpool daemon is up by running "ps -C ffpool" or similar.

9. Create your first key to be passed down to a client by running the following shell command(the key will be printed to the screen):

	ffsetfile /tmp/receive_test_file

10. View the api subdirectory of this package to see if your desired programming language is supported.
