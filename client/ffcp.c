/* [fastfile] ffcp.c :: command-line (client) utility to copy files to mod_fastfile.
** Copyright (C) 2012 fakehalo [v9@fakehalo.us]
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
**/

#define FF_INCLUDE_ALL
#include "config.h"
#include "../include/fastfile.h" /* common defines used throughout fastfile. */
#include "ffcp.h" /* prototypes specifically related to ffcp. */


/* globals / externals. */
struct ff_data_s ff_data;
extern char *optarg;


/* functions. */

/* update the column width, for use with the percentage bar. */
void ff_display_col_update() {
	unsigned int col;
	struct winsize win;

	if(ff_data.quiet != FF_QUIET_NONE)
		return;

	col = ff_data.display_col;
#ifdef TIOCGWINSZ
	if(!ioctl(STDOUT_FILENO, TIOCGWINSZ, &win) && win.ws_col >= 1)
		ff_data.display_col = win.ws_col;
	else
#endif
		ff_data.display_col = 80;

	/* clear the line if width changed. */
	if(col && col != ff_data.display_col)
		printf("\n");

	return;
}

/* percentage bar update. */
void ff_display_update(char force) {
	int i, plen, pfill;
	unsigned int hr, mn, sc;
	float pct, age, ps;
	char *pct_str, *ps_ext, eta_str[128];
	struct timeval tv;

	/* only update the screen once a second unless forced. */
	if(!force && ff_data.display_time == time(NULL))
		return;
	ff_data.display_time = time(NULL);

	pct = ((float)ff_data.sent / ff_data.size) * 100;

	if(ff_data.quiet == FF_QUIET_API_MODE) {
		ff_display_api(FF_API_MODE_SEND, (unsigned char)pct);
		return;
	}
	else if(ff_data.quiet != FF_QUIET_NONE || !ff_data.has_term)
		return;

	ff_display_col_update();

	/* ansi code voodoo to go backwards the amount of characters of the previous line, print spaces, and go back again. */
	if(ff_data.display_len > 0) {
                printf("\033[%uD%*s\033[%uD", ff_data.display_len, ff_data.display_len, "", ff_data.display_len);
                fflush(stdout);
		ff_data.display_len = 0;
	}
	gettimeofday(&tv, NULL);
	age = tv.tv_sec - ff_data.start_tv.tv_sec + ((tv.tv_usec - ff_data.start_tv.tv_usec) / 1000000.0);

	/* related to displaying of the progress bar. */
	plen = (ff_data.display_col - 50);
	pfill = ((float)ff_data.sent / ff_data.size) * plen;

	ps = 0;
	hr = mn = sc = 0;
	ps_ext = "?/s";
	memset(eta_str, 0, 128);

	/* generate bps-related data. */
	if((long long)ff_data.sent-ff_data.sent_ignore > 0) {
		if(age > 0) {
			ps = (ff_data.sent - ff_data.sent_ignore) / age;
			if(ps > 0)
				sc = (ff_data.size - ff_data.sent) / ps;
		}

		if(!ff_data.display_bytes)
			ps *= 8;
		for(i=0; ps > 1024 && i < 5; ps /= 1024)
			i++;
		if(ff_data.display_bytes) {
			switch(i) {
				case 0: ps_ext = "B/s"; break;
				case 1: ps_ext = "KB/s"; break;
				case 2: ps_ext = "MB/s"; break;
				case 3: ps_ext = "GB/s"; break;
				case 4: ps_ext = "TB/s"; break;
				default: ps_ext = "?/s"; break;
			}
		}
		else {
			switch(i) {
				case 0: ps_ext = "b/s"; break;
				case 1: ps_ext = "Kb/s"; break;
				case 2: ps_ext = "Mb/s"; break;
				case 3: ps_ext = "Gb/s"; break;
				case 4: ps_ext = "Tb/s"; break;
				default: ps_ext = "?/s"; break;
			}
		}
	}

	/* eta display. */
	while(sc >= 60) {
		sc -= 60;
		mn++;
	}
	while(mn >= 60) {
		mn -= 60;
		hr++;
	}
	if(hr > 0) sprintf(eta_str+strlen(eta_str), "%uh ", hr);
	if(mn > 0) sprintf(eta_str+strlen(eta_str), "%um ", mn);
	sprintf(eta_str+strlen(eta_str), "%us ", sc);

	/* if the percentage bar has too few characters to make a decent display with just skip it. */
	if(plen < 10)
		ff_data.display_len = printf("%5.1f%% RATE: %.1f%s - ETA: %s", pct, ps, ps_ext, eta_str);
	else {
		if(!(pct_str = malloc(plen + 1)))
			ff_error(FF_MSG_ERR, "failed to allocate memory for display.");
		memset(pct_str, ' ', plen);
		memset(pct_str, '|', pfill);
		pct_str[plen] = 0;
		ff_data.display_len = printf("%5.1f%% [%s] RATE: %.1f%s - ETA: %s", pct, pct_str, ps, ps_ext, eta_str);
		free(pct_str);
	}

	fflush(stdout);
	return;
}

/* all-purpose print output, makes life easier. */
void ff_display_print(char *fmt, ...) {
	char buf[FF_BUFSIZE_LARGE + 1];
	va_list ap;

	if(ff_data.quiet != FF_QUIET_NONE)
		return;

	memset(buf, 0, FF_BUFSIZE_LARGE);
	va_start(ap, fmt);
	vsnprintf(buf, FF_BUFSIZE_LARGE, fmt, ap);
	va_end(ap);

	printf("%s", buf);
	fflush(stdout);

	return;
}

/* simple logic for the -a option to allow programs to easily read the output of ffcp. */
/* each character to stdout represents a status of ffcp: */
/* 0-100 represents the building of the manifesto percentage. */
/* 100-200 represents the sending percentage.  ie. 199 = 99% sent. (201-255 are reserved for future options) */
void ff_display_api(unsigned char mode, unsigned char status) {
	fputc(mode==FF_API_MODE_SEND ? status+100 : status, stdout);
	fflush(stdout);
	return;
}

/* all exits go through this. */
void ff_exit(int e) {
#ifdef TCSANOW
	if(ff_data.has_term)
		tcsetattr(STDIN_FILENO, TCSANOW, &ff_data.term_orig);
#endif
	exit(e);
}

/* all-purpose error handler, makes life easier. */
void ff_error(unsigned char type, char *fmt, ...) {
	char buf[FF_BUFSIZE_LARGE + 1];
	va_list ap;

	if(ff_data.quiet != FF_QUIET_ALL) {
		memset(buf, 0, FF_BUFSIZE_LARGE);
		va_start(ap, fmt);
		vsnprintf(buf, FF_BUFSIZE_LARGE, fmt, ap);
		va_end(ap);

		fprintf(ff_data.stdout_error ? stdout : stderr, "%s%s: %s\n", (ff_data.has_term ? "\n" : ""), (type == FF_MSG_ERR ? "error" : "warning"), buf);
		fflush(ff_data.stdout_error ? stdout : stderr);
	}

	if(type == FF_MSG_ERR)
		ff_exit(FF_EXIT_ERROR);

	return;
}

/* get the chunksize of an individual chunk. */
unsigned int ff_get_chunksize(unsigned int chunk) {
	if(chunk >= ff_data.chunks)
		return(!(ff_data.size%ff_data.chunksize) ? ff_data.chunksize : (ff_data.size%ff_data.chunksize));
	else
		return(ff_data.chunksize);
}

/* convert a "host:port" string to sockaddr_in entry. */
struct sockaddr_in ff_atos(char *str) {
	char *buf, *ptr;
	struct sockaddr_in sa;
	struct hostent *t;
	struct servent *se;

	if(!(buf = strdup(str)))
		ff_error(FF_MSG_ERR, "failed to duplicate memory for hostname:port structure conversion.");

	/* OBSOLETE; spliced out by the time it gets to this function. */
	for(ptr = buf; *ptr; ptr++) {
		if(*ptr == '@') {
			*ptr++ = 0;
			break;
		}
	}

	/* zero it out for good measure. */
	memset((char *)&sa, 0, sizeof(struct sockaddr_in));

	/* gotta have this if we want to actually use it. */
	sa.sin_family = AF_INET;

	/* handle the hostname/ip, and put it in the struct. */
	sa.sin_addr.s_addr = inet_addr(buf);
	if((signed int)sa.sin_addr.s_addr == 0 || (signed int)sa.sin_addr.s_addr == -1) {
		sa.sin_addr.s_addr = 0;
		if((t = gethostbyname(buf)))
			memcpy((char *)&sa.sin_addr.s_addr, (char *)t->h_addr, sizeof(sa.sin_addr.s_addr));
		if(sa.sin_addr.s_addr == 0)

			/* unresolved? guess 127.0.0.1 then. */
			sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	}

	/* got a port? */
	if(*ptr) {

		/* string representation of a port? */
		if(!isdigit((unsigned char)*ptr) && (se = getservbyname(ptr, "tcp")))
			sa.sin_port = se->s_port;

		/* nope; handle the port, and put it in the struct. (could be zero, would just fail) */
		else
			sa.sin_port = htons(atoi(ptr));
	}
	else
		sa.sin_port = htons(ff_data.port);

	free(buf);
	return(sa);
}

/* convert a "host:port" string to sockaddr_in entry. */
#ifdef FF_HAVE_IPV6
struct sockaddr_in6 ff_atos6(char *str) {
	char *buf, *ptr;
	struct sockaddr_in6 sa;
	struct hostent *t;
	struct servent *se;

	if(!(buf = strdup(str)))
		ff_error(FF_MSG_ERR, "failed to duplicate memory for hostname:port structure conversion.");

	/* can't use ':' for port seperation since ipv6 ips use :.  -p command-line argument more likely than this obscure way. */
	/* OBSOLETE; spliced out by the time it gets to this function. */
	for(ptr = buf; *ptr; ptr++) {
		if(*ptr == '@') {
			*ptr++ = 0;
			break;
		}
	}

	/* zero it out for good measure. */
	memset((char *)&sa, 0, sizeof(sa));

	/* gotta have this if we want to actually use it. */
	sa.sin6_family = AF_INET6;

	/* handle the hostname/ip, and put it in the struct. */
	if(inet_pton(AF_INET6, buf, &sa.sin6_addr) < 1) {
		if((t = gethostbyname2(buf, AF_INET6)))
			memcpy((char *)&sa.sin6_addr, t->h_addr, t->h_length);
		else
			/* unresolved? guess ::1 then. */
			sa.sin6_addr = in6addr_loopback;
	}

	/* got a port? */
	if(*ptr) {

		/* string representation of a port? */
		if(!isdigit((unsigned char)*ptr) && (se = getservbyname(ptr, "tcp")))
			sa.sin6_port = se->s_port;

		/* nope; handle the port, and put it in the struct. (could be zero, would just fail) */
		else
			sa.sin6_port = htons(atoi(ptr));
	}
	else
		sa.sin6_port = htons(ff_data.port);

	free(buf);
	return(sa);
}
#endif

/* calculate crc32; based on intel's performance boosted crc32. */
unsigned int ff_crc32_64bit(void *data, unsigned long long len, unsigned int pcrc) {
	unsigned int one, two, *ccrc = (unsigned int *)data, crc = ~pcrc;
	unsigned char *c;

	while(len >= 8) {
		one = *ccrc++ ^ crc;
		two = *ccrc++;
		crc =	ff_crc32_slice_table[7][one & 0xFF] ^
			ff_crc32_slice_table[6][(one>>8) & 0xFF] ^
			ff_crc32_slice_table[5][(one>>16) & 0xFF] ^
			ff_crc32_slice_table[4][one>>24] ^
			ff_crc32_slice_table[3][two & 0xFF] ^
			ff_crc32_slice_table[2][(two>>8) & 0xFF] ^
			ff_crc32_slice_table[1][(two>>16) & 0xFF] ^
			ff_crc32_slice_table[0][two>>24];
		len -= 8;
	}

	c = (unsigned char *)ccrc;
	while(len--)
		crc = (crc >> 8) ^ ff_crc32_slice_table[0][(crc & 0xFF) ^ *c++];

	return(~crc);
}

/* for use when writing manifestos. */
void ff_32bit2buf(unsigned int val, unsigned char *buf) {
	buf[0] = (val & 0x000000ff);
	buf[1] = (val & 0x0000ff00) >> 8;
	buf[2] = (val & 0x00ff0000) >> 16;
	buf[3] = (val & 0xff000000) >> 24;
	return;
}

/* for use when writing manifestos. */
void ff_64bit2buf(unsigned long long val, unsigned char *buf) {
	buf[0] = (val & 0x00000000000000ff);
	buf[1] = (val & 0x000000000000ff00) >> 8;
	buf[2] = (val & 0x0000000000ff0000) >> 16;
	buf[3] = (val & 0x00000000ff000000) >> 24;
	buf[4] = (val & 0x000000ff00000000) >> 32;
	buf[5] = (val & 0x0000ff0000000000) >> 40;
	buf[6] = (val & 0x00ff000000000000) >> 48;
	buf[7] = (val & 0xff00000000000000) >> 56;
	return;
}

/* when no chunksize is provided this function guesses, this is hokey logic that definitely needs to be optimized. */
unsigned int ff_chunksize_guess(unsigned long long size) {
	int i;
	unsigned long j;
	float f;
	for(i = FF_CHUNKSIZE_GUESS_BASE; i >= 0; i--) {
		j = FF_CHUNKSIZE_GUESS_MIN_CHUNKSIZE << i;
		f = (float)size / j;
		if(f > FF_CHUNKSIZE_GUESS_TOLERANCE)
			break;
	}
	if(j > size)
		j = size;
	return(j);
}

/* creates the manifesto to be sent to mod_fastfile. */
/* [VERSION(1)][COMPLETE_CRC32(4)][FILESIZE(8)][CHUNKSIZE(4)][CHUNK_NUM_CHUNKS(4)][CRC_OF_CHUNK(4x...)][STATUS_OF_CHUNK{1x...}] */
void ff_client_manifesto(char *file, unsigned int chunksize) {
	unsigned char *buf, *ptr, rbuf[FF_BUFSIZE_LARGE];
	unsigned int crc, ccrc;
	int c, i, j, num_chunks, buf_size;
	long len;
	size_t r;

	ff_display_print("generating manifesto for: %s", basename(ff_data.file));

	/* using a file stream for the initial crcing of the file. (this ran faster than mmap() in tests) */
	if(!(ff_data.fh=fopen(file, "rb")))
		ff_error(FF_MSG_ERR, "could not open: %s (%s)", file, strerror(errno));

	fseek(ff_data.fh, 0, SEEK_END);
	ff_data.size = ftell(ff_data.fh);
	fseek(ff_data.fh, 0, SEEK_SET);

	/* prepare the map for the reading/proxying of chunks to the remote destiniation (mod_fastfile) later. */
	if((ff_data.map = mmap(NULL, ff_data.size, PROT_READ, MAP_SHARED, fileno(ff_data.fh), 0)) == MAP_FAILED)
		ff_error(FF_MSG_ERR, "could not map file: %s (%s)", file, strerror(errno));

	if(!chunksize)
		chunksize = ff_chunksize_guess(ff_data.size);

	/* this should be caught before this, but just in case. */
	if(chunksize < FF_BUFSIZE_CHUNKSIZE || chunksize % 8)
		ff_error(FF_MSG_ERR, "improper chunksize, must be a multiple of 8 and equal or greater to %u", FF_BUFSIZE_CHUNKSIZE);

	num_chunks = (ff_data.size/chunksize) + (!(ff_data.size%chunksize) ? 0 : 1);
	if(!num_chunks)
		ff_error(FF_MSG_ERR, "malformed manifesto: zero chunks (empty file?)");

	/* we'll never need more connections than chunks, lower the number of connections here (ff_client_manifesto() should always be ran before ff_data.socks are allocated) */
	if(num_chunks < ff_data.connections)
		ff_data.connections = num_chunks;

	/* save the number of chunks and prepare the chunk status buffer. */
	ff_data.chunks = num_chunks;
	if(!(ff_data.chunk_status = malloc(ff_data.chunks)))
		ff_error(FF_MSG_ERR, "failed to allocate memory for chunk status.");
	memset(ff_data.chunk_status, FF_CHUNK_UNSENT, ff_data.chunks);

	buf_size = 1 + 4 + 8 + 4 + 4 + (num_chunks * 4) + num_chunks;

	buf = malloc(buf_size);
	buf[0] = FF_PROTOCOL;

	/* buf[1..4] = complete crc, filled in at the end. */

	ff_64bit2buf(ff_data.size, buf+5);
	ff_32bit2buf(chunksize, buf+13);
	ff_32bit2buf(num_chunks, buf+17);

	ptr = buf+21;
	ccrc = crc = FF_CRC_INIT;
	i = j = 0;

	if(ff_data.quiet == FF_QUIET_API_MODE)
		ff_display_api(FF_API_MODE_BUILD, 0);
	else if(ff_data.has_term)
                ff_display_print(" [  0%%]");

	/* reading chunks of data into a buffer runs faster than getc()-style. */
	while(!feof(ff_data.fh) && (r=fread(rbuf, 1, chunksize - i > FF_BUFSIZE_CHUNKSIZE ? FF_BUFSIZE_CHUNKSIZE : chunksize - i, ff_data.fh)) > 0) {

		/* 8byte crc calcs at a time. */
		crc = ff_crc32_64bit(rbuf, r, crc);
		ccrc = ff_crc32_64bit(rbuf, r, ccrc);

		i += r;
		if(i >= chunksize) {
			if(++j > num_chunks)
				ff_error(FF_MSG_ERR, "\nmalformed manifesto: more chunks exist than calculated");
			ff_32bit2buf(crc, ptr);
				ptr += 4;

			if(ff_data.quiet == FF_QUIET_API_MODE)
				ff_display_api(FF_API_MODE_BUILD, (unsigned char) ((float)j / num_chunks * 100));
			else if(ff_data.has_term)
		                ff_display_print("\033[5D%3.0f%%]", (float)j / num_chunks * 100);
			else
				ff_display_print(".");

			crc = FF_CRC_INIT;
			i = 0;
		}
	}

	/* final partial block. */
	if(i > 0) {
		if(++j > num_chunks)
			ff_error(FF_MSG_ERR, "\nmalformed manifesto: more chunks exist than calculated");
		ff_32bit2buf(crc, ptr);
		ptr += 4;

		if(ff_data.quiet == FF_QUIET_API_MODE)
			ff_display_api(FF_API_MODE_BUILD, (unsigned char)((float)j / num_chunks * 100));
		else if(ff_data.has_term)
	                ff_display_print("\033[5D%3.0f%%]", (float)j / num_chunks * 100);
		else
			ff_display_print(".");
	}

	ff_display_print("\n");

	if(j != num_chunks)
		ff_error(FF_MSG_ERR, "malformed manifesto: more chunks allocated than used");

	/* filler, assume we've sent nothing even if this is a resume. (it will be filled in upon http status HTTP_NOT_MODIFIED) */
	memset(ptr, FF_CHUNK_UNSENT, num_chunks);

	ff_32bit2buf(ccrc, buf+1); /* fill in the complete crc at the start of the manifesto. */

	ff_data.manifesto_buf = buf;
	ff_data.manifesto_len = buf_size;

	if(ff_data.manifesto_len > FF_MANIFESTO_MAX_SIZE)
	ff_error(FF_MSG_ERR, "manifesto too large: %u > %u", ff_data.manifesto_len, FF_MANIFESTO_MAX_SIZE);

	ff_display_print("manifesto created: %u bytes (%u chunk%s, %u chunksize)\n", ff_data.manifesto_len, ff_data.chunks, ff_data.chunks==1?"":"s", ff_data.chunksize);

	return;
}

/* add a header to be used on all connections. */
void ff_set_header(char *name, char *value) {
	char *ptr;
	unsigned int i;

	/* check for any funny business. */
	for(ptr=name; *ptr; ptr++) { if(!isprint(*ptr) || *ptr==':') ff_error(FF_MSG_ERR, "malformed header name: %s", name); }
	for(ptr=value; *ptr; ptr++) { if(!isprint(*ptr)) ff_error(FF_MSG_ERR, "malformed header value: %s", value); }

	/* special case for content-length, that is always generated by the program. */
	if(!strcasecmp(name, "content-length"))
		return;

	/* see if the header is already set, if so overwrite. */
	for(i=0; i < ff_data.headers_i; i++) {
		if(!strcmp(ff_data.headers[i]->name, name)) {
			free(ff_data.headers[i]->value);
			if(!(ff_data.headers[i]->value = strdup(value)))
				ff_error(FF_MSG_ERR, "failed to duplicate memory.");
			return;
		}
	}

	/* first header, create. */
	if(!ff_data.headers || !ff_data.headers_i) {
		if(!(ff_data.headers = (struct ff_header_s **)malloc(sizeof(struct ff_header_s) * 2)))
			ff_error(FF_MSG_ERR, "failed to allocate memory.");
	}

	/* adding a new header. */
	else if(!(ff_data.headers = (struct ff_header_s **)realloc(ff_data.headers, sizeof(struct ff_header_s *) * (ff_data.headers_i + 2))))
		ff_error(FF_MSG_ERR, "failed to re-allocate memory.");

	/* allocate and add the new header. */
	if(!(ff_data.headers[ff_data.headers_i] = (struct ff_header_s *)malloc(sizeof(struct ff_header_s))))
		ff_error(FF_MSG_ERR, "failed to allocate memory.");
	if(!(ff_data.headers[ff_data.headers_i]->name = strdup(name)))
		ff_error(FF_MSG_ERR, "failed to duplicate memory.");
	if(!(ff_data.headers[ff_data.headers_i]->value = strdup(value)))
		ff_error(FF_MSG_ERR, "failed to duplicate memory.");

	ff_data.headers_i++;
	ff_data.headers[ff_data.headers_i] = NULL;
	return;
}

/* add a header to be used on all connections. (string=string parsing) */
void ff_set_header_str(char *str) {
	char *buf, *ptr;
	if(!(buf = strdup(str)))
		ff_error(FF_MSG_ERR, "failed to duplicate memory.");
	if((ptr=index(buf, '=')) && buf != ptr && !(*ptr++ = 0) && *ptr)
		ff_set_header(buf, ptr);
	else
		ff_error(FF_MSG_ERR, "erroneous header format.");
	free(buf);
	return;
}

/* allocates and returns a buffer with all the headers. (to be free'd later) */
char *ff_get_headers(unsigned int content_length) {
	char *buf;
	unsigned int i, s;
	for(s=i=0; i < ff_data.headers_i; i++) {
		s += strlen(ff_data.headers[i]->name);
		s += 2; /* ": " */
		s += strlen(ff_data.headers[i]->value);
		s += 2; /* "\r\n" */
	}
	if(content_length)
		s += 28; /* strlen(Content-Length: UINT_MAX\r\n); */
	if(!(buf = malloc(s + 1)))
		ff_error(FF_MSG_ERR, "failed to allocate memory.");
	memset(buf, 0, s + 1);
	for(i=0; i < ff_data.headers_i; i++)
		sprintf(buf+strlen(buf), "%s: %s\r\n", ff_data.headers[i]->name, ff_data.headers[i]->value);
	if(content_length)
		sprintf(buf+strlen(buf), "Content-Length: %u\r\n", content_length);
	return(buf);
}

/* allocates and returns the method/location string. (to be free'd later) */
char *ff_get_request_string(unsigned int chunk) {
	char *req;
	unsigned int s;
	s = 6 + strlen(ff_data.location) + 1 + strlen(ff_data.key) + 11; /* strlen(POST /location/key HTTP/1.1\r\n) */
	if(chunk)
		s += 11; /* "/" + UINT_MAX */
	if(!(req = malloc(s + 1)))
		ff_error(FF_MSG_ERR, "failed to allocate memory.");
	memset(req, 0, s + 1);
	if(chunk)
		sprintf(req, "POST /%s/%s/%u HTTP/1.1\r\n", ff_data.location, ff_data.key, chunk);
	else
		sprintf(req, "POST /%s/%s HTTP/1.1\r\n", ff_data.location, ff_data.key);
	return(req);
}

/* a (blocking) socket the sends the initial manifesto/instruction data to the remote mod_fastfile. */
void ff_send_manifesto() {
	unsigned char buf[FF_BUFSIZE_LARGE], *data;
	char *req, *headers;
	int fd, status, c;
	unsigned int i, len, val;
	ssize_t r;
#ifdef FF_HAVE_SSL
	SSL *ssl_handle;
	SSL_CTX *ssl_context;
#endif

	status = 0;

	ff_client_manifesto(ff_data.file, ff_data.chunksize);
	if(!ff_data.manifesto_len || !ff_data.manifesto_buf)
		ff_error(FF_MSG_ERR, "manifesto: could not create: %s", ff_data.file);

	req = ff_get_request_string(0);
	headers = ff_get_headers(ff_data.manifesto_len);

	ff_display_print("sending manifesto to: %s:%u (synchronous connection) [%s]\n", ff_data.host, ff_data.port, ff_data.ssl?"SSL":"standard");

	alarm(ff_data.timeout);

	if(ff_data.domain==AF_INET) {
		ff_data.sa = ff_atos(ff_data.host);
		if((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
			ff_error(FF_MSG_ERR, "failed to allocate socket.");
		if (connect(fd, (struct sockaddr *)&ff_data.sa, sizeof(ff_data.sa)) < 0)
			ff_error(FF_MSG_ERR, "could not establish connection with: %s", ff_data.host);
	}
#ifdef FF_HAVE_IPV6
	else if(ff_data.domain==AF_INET6) {
		ff_data.sa6 = ff_atos6(ff_data.host);
		if((fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP)) < 0)
			ff_error(FF_MSG_ERR, "failed to allocate socket. (ipv6)");
		if (connect(fd, (struct sockaddr *)&ff_data.sa6, sizeof(ff_data.sa6)) < 0)
			ff_error(FF_MSG_ERR, "could not establish connection with: %s (ipv6)", ff_data.host);
	}
#endif

	/* setup the SSL connection and write if applicable. */
#ifdef FF_HAVE_SSL
	if(ff_data.ssl) {
		if(!(ssl_context = SSL_CTX_new(SSLv23_client_method())))
			ff_error(FF_MSG_ERR, "could not create SSL context.");
		if(!(ssl_handle = SSL_new(ssl_context)))
			ff_error(FF_MSG_ERR, "could not create SSL handle.");
		if(!SSL_set_fd(ssl_handle, fd))
			ff_error(FF_MSG_ERR, "could not associate socket to SSL handle.");
		if((SSL_connect(ssl_handle)) != 1)
			ff_error(FF_MSG_ERR, "could not create SSL connection: %s", ff_data.host);
		if(
			SSL_write(ssl_handle, req, strlen(req)) != strlen(req) ||
			SSL_write(ssl_handle, headers, strlen(headers)) != strlen(headers) ||
			SSL_write(ssl_handle, "\r\n", 2) != 2 ||
			SSL_write(ssl_handle, ff_data.manifesto_buf, ff_data.manifesto_len) != ff_data.manifesto_len
		)
			ff_error(FF_MSG_ERR, "failed to write (manifesto) https request to the socket. (SSL)");

	}
	else
#endif

	/* (non-SSL) write our manifesto to the socket and make sure it all makes it out. */
	if(
		write(fd, req, strlen(req)) != strlen(req) ||
		write(fd, headers, strlen(headers)) != strlen(headers) ||
		write(fd, "\r\n", 2) != 2 ||
		write(fd, ff_data.manifesto_buf, ff_data.manifesto_len) != ff_data.manifesto_len
	)
		ff_error(FF_MSG_ERR, "failed to write (manifesto) http request to the socket.");

	free(req);
	free(headers);

	memset(buf, 0, 13); /* make sure there's an extra null-byte for atoi() */

	if((
#ifdef FF_HAVE_SSL
	ff_data.ssl ? SSL_read(ssl_handle, buf, 12) :
#endif
	read(fd, buf, 12))== 12)

		status = atoi((char *)buf+9);

	switch(status) {
		case 201: /* HTTP_CREATED (the server accepted the creation) */
			ff_display_print("manifesto: accepted, sending data. (using %u %s) [%s]\n", ff_data.connections, ff_data.connections==1?"connection":"asynchronous connections", ff_data.ssl?"SSL":"standard");
			break;
		case 304: /* HTTP_NOT_MODIFIED (the server already has a manifesto associated with this key, see if we can resume) */

			/* ignore the response headers.  (wait for consecutive \ns) */
			c = 0;
			while((
#ifdef FF_HAVE_SSL
			ff_data.ssl ? SSL_read(ssl_handle, buf, 1) :
#endif
			read(fd, buf, 1)) == 1) {
				if(buf[0]=='\n') {
					if(++c >= 2)
						break;
				}
				else if(buf[0] != '\r') /* ignore \r. */
					c = 0;
			}

			/* we have data to read. */
			if(c >= 2) {
				len = 0;
				data = NULL;
				while((r=(
#ifdef FF_HAVE_SSL
				ff_data.ssl ? SSL_read(ssl_handle, buf, FF_BUFSIZE_LARGE) :
#endif
				read(fd, buf, FF_BUFSIZE_LARGE))) > 0) {
					if(!(data = realloc(data, len + r)))
						ff_error(FF_MSG_ERR, "failed to (re-)allocate memory for manifesto comparison.");
					memcpy(data + len, buf, r);
					len += r;
				}

				/* make sure we have the minimum manifesto/buffer size needed to do comparisons. */
				if(!data || len < FF_MANIFESTO_BASE_HEADER_SIZE)
					ff_error(FF_MSG_ERR, "malformed fastfile v1 manifesto returned.");

				/* validate fastfile version */
				if(data[0] != FF_PROTOCOL)
					ff_error(FF_MSG_ERR, "manifesto already on server: incompatable fastfile version: v%u (server has v%u)", FF_PROTOCOL, data[0]);

				/* validate crc */
				if(memcmp(ff_data.manifesto_buf+1, data+1, 4))
					ff_error(FF_MSG_ERR, "manifesto already on server: CRC does not match: %s", ff_data.file);

				/* validate filesize */
				if(memcmp(ff_data.manifesto_buf+5, data+5, 8))
					ff_error(FF_MSG_ERR, "manifesto already on server: filesize does not match: %s", ff_data.file);

				/* validate chunksize (display the difference as it might be recoverable with the correct chunksize value) */
				val = data[13];
				val += data[14] << 8;
				val += data[15] << 16;
				val += data[16] << 24;
				if(val != ff_data.chunksize)
					ff_error(FF_MSG_ERR, "manifesto already on server: incorrect chunksize: %u (server has %u)", ff_data.chunksize, val);

				/* validate the number of chunks */
				val = data[17];
				val += data[18] << 8;
				val += data[19] << 16;
				val += data[20] << 24;
				if(val != ff_data.chunks)
					ff_error(FF_MSG_ERR, "manifesto already on server: incorrect number of chunks: %u (server has %u)", ff_data.chunks, val);

				/* validate overall structure */
				if(len != ff_data.manifesto_len)
					ff_error(FF_MSG_ERR, "manifesto already on server: differing manifesto sizes: %u (server has %u)", ff_data.manifesto_len, len);

				/* copy the chunk statuses from the server, so we don't resend successful chunks.  the checks above should remove security problems by this point. */
				memcpy(ff_data.chunk_status, data + len - ff_data.chunks, ff_data.chunks);

				/* count as sent already in the overall byte count. */
				for(i = 0; i < ff_data.chunks; i++) {
					if(ff_data.chunk_status[i] == FF_CHUNK_SENT) {
						ff_data.sent += ff_get_chunksize(i+1);
						ff_data.sent_ignore += ff_get_chunksize(i+1); /* don't count towards bps. */
					}
				}

				free(data);
			}
			else
				ff_error(FF_MSG_ERR, "malformed fastfile v1 manifesto returned.");

			ff_display_print("manifesto: matched existing, resuming. (using %u %s) [%s]\n", ff_data.connections, ff_data.connections==1?"connection":"asynchronous connections", ff_data.ssl?"SSL":"standard");
			break;
		case 406: /* HTTP_NOT_ACCEPTABLE */
			ff_error(FF_MSG_ERR, "malformed manifesto.\n");
			break;
		case 412: /* HTTP_PRECONDITION_FAILED */
			ff_error(FF_MSG_ERR, "accompanying data file was not made with key file. (server-side error)\n");
			break;
		case 413: /* HTTP_REQUEST_ENTITY_TOO_LARGE */
			ff_error(FF_MSG_ERR, "filesize of local file larger than this fastfile server allocated.\n");
			break;
		case 414: /* HTTP_REQUEST_URI_TOO_LARGE */
			ff_error(FF_MSG_ERR, "keyfile too large. (try to use a larger chunksize)\n");
			break;
		case 500: /* HTTP_INTERNAL_SERVER_ERROR */
			ff_error(FF_MSG_ERR, "server-side error processing fastfile request.\n");
			break;
		case 505: /* HTTP_VERSION_NOT_SUPPORTED */
			ff_error(FF_MSG_ERR, "fastfile version not supported.\n");
			break;
		case 503: /* HTTP_SERVICE_UNAVAILABLE */
			ff_error(FF_MSG_ERR, "fastfile server is misconfigured.\n");
			break;
		case 400: /* HTTP_BAD_REQUEST */
		case 404: /* HTTP_NOT_FOUND */
			ff_error(FF_MSG_ERR, "fastfile key could not be found on server, or not a valid fastfile location: /%s/%s\n", ff_data.location, ff_data.key);
			break;
		case 301: /* HTTP_MOVED_PERMANENTLY */
		case 302: /* HTTP_MOVED_TEMPORARILY */
		case 200: /* HTTP_OK */
			ff_error(FF_MSG_ERR, "invalid fastfile location: %s/%s\n", ff_data.host, ff_data.location);
			break;
		case 0:
			ff_error(FF_MSG_ERR, "manifesto: unexpected (non-http status) response\n", status);
			break;
		default:
			ff_error(FF_MSG_ERR, "manifesto: unexpected http status code: %d\n", status);
			break;
	}

	/* we made it, clean up and carry on. */
	alarm(0);
	close(fd);
#ifdef FF_HAVE_SSL
	if(ff_data.ssl) {
		SSL_shutdown(ssl_handle);
		SSL_free(ssl_handle);
		SSL_CTX_free(ssl_context);
	}
#endif
	return;
}

/* parse the host parameter, check for port and auto-detect ipv6 ips. (potentially truncates original buffer) */
void ff_parse_host_str(char *str) {
	char *ptr;
	ptr = str;

	/* set port and clean up hostname if '@' exists, not if it's the first character though. */
	if((ptr=index(ff_data.host, '@')) && str != ptr) {
		*ptr++ = 0;
		if(*ptr)
			ff_data.port = atoi(ptr);
	}
#ifdef FF_HAVE_IPV6
	/* see if we have two colons in the host, if so make it ipv6 automatically. */
	if((ptr=index(ff_data.host, ':')) && *++ptr && index(ptr, ':'))
		ff_data.domain = AF_INET6;
#endif
	return;
}

/* overall signal/event handler. */
void ff_sig_handler(int sig){
	switch(sig) {
		case SIGALRM:
			ff_error(FF_MSG_ERR, "connection timed out: %s (%u)", ff_data.host, ff_data.timeout);
			break;
		case SIGINT:
		case SIGTSTP:
			ff_error(FF_MSG_ERR, "user interrupt!");
			break;
	}
	return;
}

/* send data to mod_fastfile. */
char ff_sock_send_data(int s) {
	long len;
	ssize_t w;
	if(s > ff_data.connections || ff_data.socks[s]->status != FF_SOCK_CONNECTED)
		return(1);
	len = ff_data.bufsize > ff_data.socks[s]->remaining ? ff_data.socks[s]->remaining : ff_data.bufsize;
	if((w=(
#ifdef FF_HAVE_SSL
	ff_data.ssl ? SSL_write(ff_data.socks[s]->ssl_handle, ff_data.socks[s]->map_ptr, len) :
#endif
	write(ff_data.socks[s]->fd, ff_data.socks[s]->map_ptr, len))) > 0) {
		ff_data.socks[s]->map_ptr += w;
		ff_data.socks[s]->remaining -= w;
		ff_data.socks[s]->sent += w;
		ff_data.sent += w;
	}
	return(0);
}

/* send the http method/location/headers to mod_fastfile. */
char ff_sock_send_request(int s) {
	char ret, *req, *headers;
	if(s > ff_data.connections || ff_data.socks[s]->status != FF_SOCK_CONNECTED)
		return(1);
	ret = 0;
	req = ff_get_request_string(ff_data.socks[s]->chunk);
	headers = ff_get_headers(ff_get_chunksize(ff_data.socks[s]->chunk));
#ifdef FF_HAVE_SSL
	if(ff_data.ssl) {
		if(SSL_write(ff_data.socks[s]->ssl_handle, req, strlen(req)) != strlen(req) ||
		SSL_write(ff_data.socks[s]->ssl_handle, headers, strlen(headers)) != strlen(headers) ||
		SSL_write(ff_data.socks[s]->ssl_handle, "\r\n", 2) != 2)
			ret = 1;
	}
	else {
#endif
		if(write(ff_data.socks[s]->fd, req, strlen(req)) != strlen(req) ||
		write(ff_data.socks[s]->fd, headers, strlen(headers)) != strlen(headers) ||
		write(ff_data.socks[s]->fd, "\r\n", 2) != 2)
			ret = 1;
#ifdef FF_HAVE_SSL
	}
#endif
	free(req);
	free(headers);
	return(ret);
}

/* highest fd for select(). */
int ff_sock_highest_fd() {
	int s, h;
	for(s=0, h=-1; s < ff_data.connections; s++) {
		if(h < ff_data.socks[s]->fd)
			h = ff_data.socks[s]->fd;
	}
	return(h);
}

/* all asynchronous (data) sockets will have this applied, the manifesto socket is blocking and does not. */
void ff_sock_nonblock(int s) {
	int opts;
	if ((opts = fcntl(s, F_GETFL)) < 0)
		ff_error(FF_MSG_ERR, "failed to set socket to nonblocking.");
	opts = (opts | O_NONBLOCK);
	if (fcntl(s, F_SETFL, opts) < 0)
		ff_error(FF_MSG_ERR, "failed to set socket to nonblocking.");
	return;
}

/* free an (asynchronous) connection and set the mode of the chunk. */
void ff_sock_free(int s, unsigned char mode) {

	/* one too many failed connections, exit. */
	if(!ff_data.connections_failed)
		ff_error(FF_MSG_ERR, "exceeded maximum number of failed connections: %s", ff_data.host);

	/* count as a failed connection unless set to success/sent. */
	if(mode != FF_CHUNK_SENT) {
		ff_data.connections_failed--;

		/* discount what was sent from the overall total. */
		if(ff_data.socks[s]->sent > 0)
			ff_data.sent -= ff_data.socks[s]->sent;
	}

	if(ff_data.socks[s]->fd >= 0) {
		close(ff_data.socks[s]->fd);
		ff_data.socks[s]->fd = -1;
	}
#ifdef FF_HAVE_SSL
	if(ff_data.ssl) {
		if(ff_data.socks[s]->ssl_handle) {
			SSL_shutdown(ff_data.socks[s]->ssl_handle);
			SSL_free(ff_data.socks[s]->ssl_handle);
		}
		ff_data.socks[s]->ssl_handle = NULL;
		if(ff_data.socks[s]->ssl_context)
			SSL_CTX_free(ff_data.socks[s]->ssl_context);
		ff_data.socks[s]->ssl_context = NULL;
	}
#endif
	ff_data.socks[s]->status = FF_SOCK_INIT;
	ff_data.socks[s]->remaining = 0;
	ff_data.socks[s]->sent = 0;

	/* if this hasn't been set to FF_CHUNK_SENT before this free function was called assume failure. */
	if(ff_data.socks[s]->chunk > 0 && ff_data.chunk_status[ff_data.socks[s]->chunk-1] == FF_CHUNK_ACTIVE)
		ff_data.chunk_status[ff_data.socks[s]->chunk-1] = mode;
	ff_data.socks[s]->chunk = 0;
	return;
}

/* gets the next open chunk, can set it as active. */
int ff_get_next_chunk(char active) {
	int i;
	for(i = 0; i < ff_data.chunks; i++) {
		if(ff_data.chunk_status[i] == FF_CHUNK_UNSENT) {
			if(active)
				ff_data.chunk_status[i] = FF_CHUNK_ACTIVE;
			return(i+1);
		}
	}
	return(-1);
}

/* check that our SSL handshake is complete. (1=connected, 0=pending, -1=error) */
#ifdef FF_HAVE_SSL
int ff_ssl_handshake(int s) {
	int sc, ret;

	ret = -1; /* assume the worst. */

	/* if we're not in this state we shouldn't be here. */
	if(ff_data.socks[s]->status != FF_SOCK_SSL_CONNECTING)
		return(ret);

	switch((sc = SSL_connect(ff_data.socks[s]->ssl_handle))) {
		case 1: /* successful non-blocking SSL handshake complete. */
			ff_data.socks[s]->status = FF_SOCK_CONNECTED;
			ret = ff_sock_send_request(s) ? -1 : 1;
			break;
		case 0: /* non WANT_READ/WANT_WRITE return, failed. */
			ret = -1;
			break;
		default: /* < 0, action needed to continue? */
			switch(SSL_get_error(ff_data.socks[s]->ssl_handle, sc)) {

				case SSL_ERROR_WANT_READ: /* still waiting on SSL_connect. */
				case SSL_ERROR_WANT_WRITE:
					ret = 0;
					break;
				case SSL_ERROR_NONE: /* good to go. */
					ff_data.socks[s]->status = FF_SOCK_CONNECTED;
					ret = ff_sock_send_request(s) ? -1 : 1;
					break;
				default: /* busted. */
					ret = -1;
					break;
			}
			break;
	}
	return(ret);
}
#endif

/* create an (asynchronous) connection to mod_fastfile. */
char ff_sock_connect(int s) {
	int c;

	/* socket structure hasn't been prepared to be used, or is in another state. */
	if(s > ff_data.connections || ff_data.socks[s]->status != FF_SOCK_INIT)
		return(FF_SOCK_CONNECT_ERROR);

	/* no available chunks to send, do a check here or elsewhere to determine we're done. (all FF_CHUNK_SENT) */
	if((c = ff_get_next_chunk(1)) < 0)
		return(FF_SOCK_CONNECT_NO_CHUNKS);

	ff_data.socks[s]->chunk = (unsigned int)c;

	/* set the amount of data this chunk will be, content-length. */
	ff_data.socks[s]->remaining = ff_get_chunksize(ff_data.socks[s]->chunk);

	ff_data.socks[s]->map_ptr = ff_data.map + ((unsigned long long)(c - 1) * ff_data.chunksize);
	if((ff_data.socks[s]->fd = socket(ff_data.domain, SOCK_STREAM, IPPROTO_TCP)) < 0)
		ff_error(FF_MSG_ERR, "failed to allocate socket.");
	ff_sock_nonblock(ff_data.socks[s]->fd);

#ifdef FF_HAVE_IPV6
	if((c=connect(ff_data.socks[s]->fd, (ff_data.domain==AF_INET ? (struct sockaddr *)&ff_data.sa : (struct sockaddr *)&ff_data.sa6), (ff_data.domain==AF_INET ? sizeof(ff_data.sa) : sizeof(ff_data.sa6)))) == -1) {
#else
	if((c=connect(ff_data.socks[s]->fd, (struct sockaddr *)&ff_data.sa, sizeof(ff_data.sa))) == -1) {
#endif
		/* normal non-blocking "in progress", not officially connected yet. */
		if(errno == EINPROGRESS)
			ff_data.socks[s]->status = FF_SOCK_CONNECTING;

		/* connection failed. */
		else {
			ff_error(FF_MSG_WARN, "could not establish connection with: %s", ff_data.host);
			ff_sock_free(s, FF_CHUNK_UNSENT);
			return(FF_SOCK_CONNECT_ERROR);
		}
	}

	/* add in SSL support if applicable. */
#ifdef FF_HAVE_SSL
	if(ff_data.ssl) {

		/* setup the basics for the new SSL instance. */
		if(!(ff_data.socks[s]->ssl_context = SSL_CTX_new(SSLv23_client_method())))
			ff_error(FF_MSG_WARN, "could not create SSL context.");
		else if(!(ff_data.socks[s]->ssl_handle = SSL_new(ff_data.socks[s]->ssl_context)))
			ff_error(FF_MSG_WARN, "could not create SSL handle.");
		else if(!SSL_set_fd(ff_data.socks[s]->ssl_handle, ff_data.socks[s]->fd))
			ff_error(FF_MSG_WARN, "could not associate socket to SSL handle.");
		else /* made it here without error, go into handshake mode. */
			ff_data.socks[s]->status = FF_SOCK_SSL_CONNECTING;

		/* error connecting, and not a waiting issue. (if status isn't FF_SOCK_SSL_CONNECTING from above this will auto-fail) */
		if(ff_ssl_handshake(s) < 0) {
			ff_sock_free(s, FF_CHUNK_UNSENT);
			return(FF_SOCK_CONNECT_ERROR);
		}
	}
#endif

	/* immediate connection. (not likely to ever be seen) */
	if(!c) {
		ff_data.socks[s]->status = (ff_data.ssl ? FF_SOCK_SSL_CONNECTING : FF_SOCK_CONNECTED);

		/* only occurs if FF_SOCK_CONNECTED. */
		if(ff_sock_send_request(s)) {
			ff_sock_free(s, FF_CHUNK_UNSENT);
			return(FF_SOCK_CONNECT_ERROR);
		}
	}

	return(0);
}

/* create a fd_set list of all of our read sockets for select(). */
fd_set ff_sock_fd_set() {
	int s;
	fd_set fds;
	FD_ZERO(&fds);

	/* check our connections. */
	for(s=0; s < ff_data.connections; s++) {

		/* shouldn't happen, but just in case. */
		if(ff_data.socks[s]->fd < 0)
			continue;

		/* from connect() attempt. */
		if(ff_data.socks[s]->status == FF_SOCK_CONNECTED || ff_data.socks[s]->status == FF_SOCK_SSL_CONNECTING || ff_data.socks[s]->status == FF_SOCK_CONNECTED_READONLY)
			FD_SET(ff_data.socks[s]->fd, &fds);

	}

	return(fds);
}

/* create a fd_set list of all of our write/connecting sockets for select(). */
fd_set ff_sock_wfd_set() {
	int s;
	fd_set wfds;
	FD_ZERO(&wfds);

	/* check our connections. */
	for(s=0; s < ff_data.connections; s++) {

		/* shouldn't happen, but just in case. */
		if(ff_data.socks[s]->fd < 0)
			continue;

		/* from connect() attempt. */
		if(ff_data.socks[s]->status == FF_SOCK_CONNECTING || ff_data.socks[s]->status == FF_SOCK_SSL_CONNECTING || ff_data.socks[s]->status == FF_SOCK_CONNECTED)
			FD_SET(ff_data.socks[s]->fd, &wfds);
	}

	return(wfds);
}

/* run through all the sockets in fd_sets, passed from select(). */
void ff_sock_handler(fd_set fds, fd_set wfds) {
	int r, s, se, status;
	socklen_t selen;
	unsigned char mode;
	char buf[13];

	for(s=0; s < ff_data.connections; s++) {

		/* shouldn't happen, but just in case. */
		if(ff_data.socks[s]->fd < 0)
			continue;

		/* read activity? */
		if(FD_ISSET(ff_data.socks[s]->fd, &fds)) {

			/* read activity, but still haven't completed handshake for SSL. */
#ifdef FF_HAVE_SSL
			if(ff_data.socks[s]->status == FF_SOCK_SSL_CONNECTING) {
				if(ff_ssl_handshake(s) < 0)
					ff_sock_free(s, FF_CHUNK_UNSENT);
			}

			/* normal read activity. (non-ssl handshake) */
			else {
#endif
				memset(buf, 0, 13);

				/* connection closed remotely(or error). */
				if((r = (
#ifdef FF_HAVE_SSL
				ff_data.ssl ? SSL_read(ff_data.socks[s]->ssl_handle, buf, 12) :
#endif
				recv(ff_data.socks[s]->fd, buf, 12, MSG_DONTWAIT))) < 1)
					ff_sock_free(s, FF_CHUNK_UNSENT);

				/* got data from the server */
				else {
					mode = FF_CHUNK_UNSENT;
					if(r == 12) {
						status = atoi(buf+9);
						switch(status) {
							case 202: /* HTTP_ACCEPTED */
								mode = FF_CHUNK_SENT;
								break;
							default:
								ff_error(FF_MSG_WARN, "chunk %u: server rejected chunk.", ff_data.socks[s]->chunk);
								break;
						}
					}
					ff_sock_free(s, mode);
				}
#ifdef FF_HAVE_SSL
			}
#endif
		}
		/* write activity / initial connection */
		else if(FD_ISSET(ff_data.socks[s]->fd, &wfds)) {

			/* initial connection */
			if(ff_data.socks[s]->status == FF_SOCK_CONNECTING) {
				se = 0;
				selen = sizeof(se);

				/* socket error? */
				if (getsockopt(ff_data.socks[s]->fd, SOL_SOCKET, SO_ERROR, &se, &selen) == -1 || se != 0) {
					ff_error(FF_MSG_WARN, "connection socket error: %s (%s)", ff_data.host, strerror(se));
					ff_sock_free(s, FF_CHUNK_UNSENT);
				}

				/* officially connected. */
				else {
					ff_data.socks[s]->status = (ff_data.ssl ? FF_SOCK_SSL_CONNECTING : FF_SOCK_CONNECTED);
					if(ff_sock_send_request(s))
						ff_sock_free(s, FF_CHUNK_UNSENT);
				}
			}

			/* write activity, but still haven't completed handshake for SSL. */
#ifdef FF_HAVE_SSL
			if(ff_data.socks[s]->status == FF_SOCK_SSL_CONNECTING) {
				if(ff_ssl_handshake(s) < 0)
					ff_sock_free(s, FF_CHUNK_UNSENT);
			}
#endif
			/* write activity. */
			if(ff_data.socks[s]->status == FF_SOCK_CONNECTED) {
				ff_sock_send_data(s);
				if(!ff_data.socks[s]->remaining)
					ff_data.socks[s]->status = FF_SOCK_CONNECTED_READONLY;
			}
		}
	}
}

/* the (asynchronous) event loop while sending to mod_fastfile. */
void ff_sock_loop() {
	int high, s, x;
	fd_set fds, wfds;
	struct timeval tv;

	/* don't do the progress bar if we don't detect a terminal. */
	if(!ff_data.has_term)
		ff_display_print("sending chunks to: %s:%u\n", ff_data.host, ff_data.port);
	else
		ff_display_print("\n");

	while(1) {
		if(ff_data.timeout) {
			tv.tv_sec = ff_data.timeout;
			tv.tv_usec = 0;
		}

		fds = ff_sock_fd_set();
		wfds = ff_sock_wfd_set();
		high = ff_sock_highest_fd() + 1;

		/* if 0 (-1 + 1) is the highest fd there are no fds/sockets really, just carry on to exit from here. */
		if(high > 0) {
			switch(select(high, &fds, &wfds, (fd_set *)0, (ff_data.timeout ? &tv : NULL))) {
				case -1:
					ff_error(FF_MSG_ERR, "connection error. (select())");
					break;
				case 0: /* timeout occured. */
					ff_sig_handler(SIGALRM); /* force an alarm()-style timeout. */
					break;
			}

			/* handle select() outcome. */
			ff_sock_handler(fds, wfds);
		}

		/* see if any new sockets are available to use. */
		for(x=s=0; s < ff_data.connections; s++) {
			if(ff_data.socks[s]->status == FF_SOCK_INIT) {
				switch(ff_sock_connect(s)) {
					/* nothing left to do but wait, so make this connection go idle. */
					case FF_SOCK_CONNECT_NO_CHUNKS:
						ff_data.socks[s]->status = FF_SOCK_IDLE;
						break;
					case FF_SOCK_CONNECT_ERROR:
						break;
				}
			}
			/* we have at least one connection going, carry on. */
			if(!x && ff_data.socks[s]->status != FF_SOCK_IDLE)
				x = 1;
		}

		/* nothing left to send, force the last percentage display and break the loop. */
		if(!x) {
			ff_display_update(1);
			break;
		}

		ff_display_update(0);
	}
	if(ff_data.has_term)
		ff_display_print("\n\n");
	return;
}

/* blank out all the initial (asynchronous) connections and start them off. */
void ff_sock_init() {
	unsigned int s;
	if(!(ff_data.socks = (struct ff_sockdata_s **)malloc(sizeof(struct ff_sockdata_s *) * ff_data.connections + 1)))
		ff_error(FF_MSG_ERR, "failed to allocate connections. (%u)", ff_data.connections);
	for(s=0; s < ff_data.connections; s++) {
		if(!(ff_data.socks[s] = (struct ff_sockdata_s *)malloc(sizeof(struct ff_sockdata_s))))
			ff_error(FF_MSG_ERR, "failed to allocate connection. (%u of %u)", s, ff_data.connections);
		ff_data.socks[s]->status = FF_SOCK_INIT;
		ff_data.socks[s]->fd = -1;
		ff_data.socks[s]->sent = 0;
#ifdef FF_HAVE_SSL
		if(ff_data.ssl) {
			ff_data.socks[s]->ssl_handle = NULL;
			ff_data.socks[s]->ssl_context = NULL;
		}
#endif
		(void)ff_sock_connect(s);
	}
	ff_data.socks[s] = NULL;
	return;
}

/* MAIN */
int main(int argc, char **argv) {
	int chr, i;

	/* make annoying warning when referencing windows/dos style path names go bye-bye. */
#ifdef __CYGWIN__
	setenv("CYGWIN", "nodosfilewarning", 1);
#endif

	/* catch signals. */
	signal(SIGALRM, ff_sig_handler);
	signal(SIGINT, ff_sig_handler);
	signal(SIGTSTP, ff_sig_handler);
	signal(SIGPIPE, SIG_IGN);

	/* defaults and initialization. */
	ff_data.domain = AF_INET;
	ff_data.file = NULL;
	ff_data.key = NULL;
	ff_data.host = NULL;
	ff_data.location = NULL;
	ff_data.headers = NULL;
	ff_data.timeout = FF_TIMEOUT_DEFAULT;
	ff_data.connections = FF_CONNECTIONS_DEFAULT;
	ff_data.connections_failed = FF_CONNECTIONS_FAILED_DEFAULT;
	ff_data.bufsize = FF_BUFSIZE_DEFAULT;
	ff_data.chunksize = FF_CHUNKSIZE_DEFAULT;
	ff_data.quiet = FF_QUIET_NONE;
	ff_data.port = 0;
	ff_data.ssl = 0;
	ff_data.manifesto_len = 0;
	ff_data.headers_i = 0;
	ff_data.sent = 0;
	ff_data.sent_ignore = 0;
	ff_data.display_len = 0;
	ff_data.display_bytes = 0;
	ff_data.stdout_error = 0;
	ff_data.has_term = (isatty(STDOUT_FILENO) && isatty(STDERR_FILENO) && getenv("TERM")) ? 1 : 0;

	/* disable user input. */
#ifdef TCSANOW
	if(ff_data.has_term) {
		if(!tcgetattr(STDIN_FILENO, &ff_data.term_orig)) {
			ff_data.term = ff_data.term_orig;
			ff_data.term.c_lflag &= ~(ICANON | ECHO);
			tcsetattr(STDIN_FILENO, TCSANOW, &ff_data.term);
		}
		else
			ff_data.has_term = 0;
	}
#endif

	if(argc == 2 && !strcmp(argv[1], "-v")) {
		printf("%s: version %s (protocol v%u)\n", argv[0], FF_VERSION, FF_PROTOCOL);
		ff_exit(FF_EXIT_SUCCESS);
	}
	else if(argc > 3) {
		if(!(ff_data.host = strdup(argv[argc-3])))
			ff_error(FF_MSG_ERR, "failed to duplicate memory.");
		if(!(ff_data.key = strdup(argv[argc-2])))
			ff_error(FF_MSG_ERR, "failed to duplicate memory.");
		if(!(ff_data.file = strdup(argv[argc-1])))
			ff_error(FF_MSG_ERR, "failed to duplicate memory.");

		/* where applicable, hide the key used from the command-line to avoid being seen in 'ps' commmands. */
		memset(argv[argc-2], '*', strlen(argv[argc-2]));

		ff_parse_host_str(ff_data.host);
		ff_set_header("Host", ff_data.host);
		ff_set_header("Connection", "close");

		while((chr = getopt(argc - 3, argv, "v46sBeqap:h:H:l:t:n:c:f:b:")) != EOF) {
			switch(chr) {
				case 'v':
					printf("%s: version %s (protocol v%u)\n", argv[0], FF_VERSION, FF_PROTOCOL);
					ff_exit(FF_EXIT_SUCCESS);
					break;
				case '4':
					ff_data.domain = AF_INET;
					break;
				case '6':
#ifdef FF_HAVE_IPV6
					ff_data.domain = AF_INET6;
#else
					ff_error(FF_MSG_ERR, "IPv6 support was not included at compile time, remove -6 flag.");
#endif
					break;
				case 's':
#ifdef FF_HAVE_SSL
					ff_data.ssl = 1;
#else
					ff_error(FF_MSG_ERR, "SSL support was not included at compile time, remove -s flag.");
#endif
					break;
				case 'B':
					ff_data.display_bytes = 1;
					break;
				case 'e':
					ff_data.stdout_error = 1;
					break;
				case 'q':
					/* make api-mode overrule -q arguments. */
					if(ff_data.quiet != FF_QUIET_API_MODE)
						ff_data.quiet = (ff_data.quiet==FF_QUIET_ERRORS_ONLY ? FF_QUIET_ALL : FF_QUIET_ERRORS_ONLY);
					break;
				case 'a':
					ff_data.quiet = FF_QUIET_API_MODE;
					break;
				case 'p':
					ff_data.port = atoi(optarg);
					break;
				case 'h':
					ff_set_header("Host", optarg);
					break;
				case 'H':
					ff_set_header_str(optarg);
					break;
				case 'l':
					if(ff_data.location) free(ff_data.location);
					if(!(ff_data.location = strdup(optarg)))
						ff_error(FF_MSG_ERR, "failed to duplicate memory.");
					break;
				case 't':
					ff_data.timeout = atoi(optarg);
					break;
				case 'n':
					ff_data.connections = atoi(optarg);
					break;
				case 'c':
					ff_data.chunksize = atoi(optarg);

					/* make sure we meet some requirements for chunksizes, auto-correct. */
					if(ff_data.chunksize < FF_BUFSIZE_CHUNKSIZE) {
						ff_error(FF_MSG_WARN, "chunksize must be equal or greater than %u, compensating: %u (changed to %u)", FF_BUFSIZE_CHUNKSIZE, ff_data.chunksize, FF_BUFSIZE_CHUNKSIZE);
						ff_data.chunksize = FF_BUFSIZE_CHUNKSIZE;
					}

					/* round up to the nearest 8. */
					else if(ff_data.chunksize % 8) {
						ff_error(FF_MSG_WARN, "chunksize needs to be a multiple of 8, compensating: %u (adding %u)", ff_data.chunksize, 8 - (ff_data.chunksize % 8));
						ff_data.chunksize += 8 - (ff_data.chunksize % 8);
					}
					break;
				case 'f':
					ff_data.connections_failed = atoi(optarg);
					break;
				case 'b':
					i = atoi(optarg);
					if(i > 65535 || 1 > i)
						ff_error(FF_MSG_ERR, "invalid buffer size. (1-65535)");
					ff_data.bufsize = i;
					break;
			}
		}
	}

	if(!ff_data.file || !ff_data.key || !ff_data.host) {
		printf("usage: %s [-v46sBeqabltcnfhHp] host[@port] key file\n\n", argv[0]);
		printf("\t-v\t\t: print fastfile version information.\n");
		printf("\t-4\t\t: force use of IPv4.\n");
		printf("\t-6\t\t: force use of IPv6.\n");
		printf("\t-s\t\t: SSL; use https instead of http. (can be slower)\n");
		printf("\t-B\t\t: display transfer rate in bytes instead of bits.\n");
		printf("\t-e\t\t: output errors to stdout instead of stderr.\n");
		printf("\t-q\t\t: quiet output. (use -qq to quiet errors)\n");
		printf("\t-a\t\t: API-friendly output. (overrides -q)\n");
		printf("\t-b bufsize\t: set (write) buffer size. (%u)\n", FF_BUFSIZE_DEFAULT);
		printf("\t-l location\t: set mod_fastfile apache location. (\"%s\")\n", FF_LOCATION_DEFAULT);
		printf("\t-t timeout\t: exit after idle socket activity, in seconds. (%us)\n", FF_TIMEOUT_DEFAULT);
		printf("\t-c chunksize\t: set chunksize for manifesto.\n");
		printf("\t-n connections\t: number of asynchronous connections to use. (%u)\n", FF_CONNECTIONS_DEFAULT);
		printf("\t-f connections\t: exit after number of failed connections. (%u)\n", FF_CONNECTIONS_FAILED_DEFAULT);
		printf("\t-h host\t\t: specify alternate host http header.\n");
		printf("\t-H name=value\t: specify arbitrary http header.\n");
		printf("\t-p port\t\t: specify port mod_fastfile is running on. (%u)\n\n", FF_PORT_DEFAULT);

		/* allow user input. */
#ifdef TCSANOW
		if(ff_data.has_term)
			tcsetattr(STDIN_FILENO, TCSANOW, &ff_data.term_orig);
#endif

		/* error exit; just in case the command-line is malformed and this is in an automated environment. */
		ff_exit(FF_EXIT_ERROR);
	}

	/* ensure defaults post-argument parsing. */
	if(!ff_data.port)
		ff_data.port = (ff_data.ssl ? FF_SSL_PORT_DEFAULT : FF_PORT_DEFAULT);
	if(!ff_data.location)
		ff_data.location = FF_LOCATION_DEFAULT;

#ifdef FF_HAVE_SSL
	if(ff_data.ssl) {

		/* register the error strings for libcrypto & libssl, and available ciphers/digests. */
		SSL_load_error_strings();
		SSL_library_init();
	}
#endif

	/* this function will exit if anything goes sour during the initial creation. */
	ff_send_manifesto();

	/* used for eta/rate display. (start time) */
	gettimeofday(&ff_data.start_tv, NULL);

	ff_sock_init();
	ff_sock_loop();

	if(ff_data.map && munmap(ff_data.map, ff_data.size) == -1)
		ff_error(FF_MSG_ERR, "failed to unmap file: %s", ff_data.file);

	ff_display_print("successfully sent %s!\n", basename(ff_data.file), ff_data.host, ff_data.port);

	ff_exit(FF_EXIT_SUCCESS);

	/* never makes it here, silences compiler. */
	return(0);
}
