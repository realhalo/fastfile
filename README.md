1. What is Fastfile?

	Fastfile is a way to quickly and programmatically send files between two machines where network optimization and firewall rules are out of the hands of the programmer.  The transmission logic of Fastfile is the same as most download accelerators, by breaking files up into chunks and simultaneously uploading them to their destination.  Fastfile uses key-based (user-less) authentication, this is done by the server-side creating an arbitrary key to be associated with a specific file.  The means by which the server-side gives the key to the client-side is completely up to the programmer, but typically this would be done over a webservice.

2. Why fastfile?

	Fastfile was created as a hack-ish remedy to bypass intranet issues/configurations when sending large files over networks.  A secondary design consideration for Fastfile was to allow programmers to seamlessly send files back and forth using a language-independent design, without user-based authentication or other protocol hindrances.

2. Is Fastfile fast?

	Fastfile can obtain speed increases around 100% (2x faster) depending on the Fastfile configuration and network infastructure involved, in other cases you may notice no speed increase at all.

3. How do I set up and use Fastfile server?

	Please read the README.md file in the server subdirectory for more information.

4. How do i set up and use Fastfile client?

	Please read the README.md file in the client subdirectory for more information.

5. What platform is Fastfile intended for?

	Fastfile is designed around UNIX-based operating systems and languages, though the client can be used on windows via cygwin.
