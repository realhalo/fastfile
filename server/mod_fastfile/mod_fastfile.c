/* [fastfile] mod_fastfile.c :: apache2 plugin (server) to save remote files.
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

#include "apr_strings.h"
#include "apr_lib.h"
#include "apr_hash.h"
#include "apr_optional.h"
#include "httpd.h"
#include "http_config.h"
#include "http_protocol.h"
#include "ap_config.h"
#include "http_core.h"

#define FF_INCLUDE_ALL
#include "../../include/fastfile.h"
#include "mod_fastfile.h"

/* functions. */

/* see if a string is a valid fastfile key. */
static char fastfile_key_validate(unsigned char *key) {
	unsigned char *ptr;
	for(ptr=key; *ptr; ptr++) {
		if(!strchr(FF_KEY_CHARS, *ptr))
			return(-1);
	}
	return(ptr==key ? -1 : 0);
}

/* calculate crc32; based on intel's performance boosted crc32. */
static unsigned int fastfile_crc32_64bit(void *data, unsigned long long len, unsigned int pcrc) {
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

/* abstraction of apr_file_open(). */
static char fastfile_open(request_rec *r, char *file, apr_file_t **fh, apr_fileperms_t perms) {
	if(apr_file_open(fh, file, perms, APR_OS_DEFAULT, r->pool) != APR_SUCCESS || !fh)
		return(-1);
	return(0);
}

/* abstraction of apr_file_seek(). */
static char fastfile_seek(apr_file_t **fh, unsigned long long p) {
	apr_off_t pos;
	pos = p;
	if(apr_file_seek(*fh, APR_SET, &pos) != APR_SUCCESS)
		return(-1);
	return(0);
}

/* abstraction of apr_file_write_full(). */
static apr_size_t fastfile_write(apr_file_t **fh, void *buf, apr_size_t size) {
	apr_size_t wsize;
	if(apr_file_write_full(*fh, buf, size, &wsize) != APR_SUCCESS)
		return(-1);
	return(size - wsize);
}

/* trim a buffer's start/ending whitespace. */
static void fastfile_trim(char *buf) {
	int i, j, s;
	for(i=strlen(buf)-1; i >= 0; i--) {
		if(isspace(buf[i]))
			buf[i] = 0;
		else
			break;
	}
	for(j=0,s=strlen(buf); j < s; j++) {
		if(!isspace(buf[j]))
			break;
	}
	if(j > 0 && i >= j)
		memmove(buf, buf+j, i-j+2);
	return;
}

/* read fastfile config for a value and return. (not optimal if a lot of config options get added) */
static char *fastfile_conf_get(char *opt, request_rec *r) {
	char buf[FF_BUFSIZE_LARGE+1], *ptr, *ret;
	apr_file_t *fc=NULL;

	if(fastfile_open(r, FF_CONF_PATH, &fc, APR_FOPEN_READ))
		return(NULL);

	ret = NULL;
	while(apr_file_eof(fc) != APR_EOF && apr_file_gets(buf, FF_BUFSIZE_LARGE, fc) == APR_SUCCESS) {
		if(buf[0] != '#' && (ptr=index(buf, '='))) {
			*ptr++ = 0;
			fastfile_trim(buf);
			fastfile_trim(ptr);
			if(*buf && *ptr && !strcasecmp(buf, opt)) {
				ret = apr_pstrdup(r->pool, ptr);
				break;
			}
		}
	}
	apr_file_close(fc);
	return(ret);
}

/* equivalent of reading to /dev/null. */
static void fastfile_pipe_to_nothing(request_rec *r) {
	unsigned char buf[FF_BUFSIZE_LARGE];
	if(ap_setup_client_block(r, REQUEST_CHUNKED_ERROR) == OK && ap_should_client_block(r))
		while(ap_get_client_block(r, buf, FF_BUFSIZE_LARGE) > 0);
	return;
}

/* check if a file is regular, no soft/hard links or other oddities. */
static char fastfile_is_safe(char *file, request_rec *r) {
	apr_finfo_t finfo;
	if(apr_stat(&finfo, file, APR_FINFO_NLINK | APR_FINFO_LINK | APR_FINFO_TYPE, r->pool) != APR_SUCCESS)
		return(0);

	/* no hardlinks, softlinks, devices, etc for files checked with this. */
	if(finfo.nlink > 1 || finfo.filetype != APR_REG)
		return(0);
	return(1);
}

/* handle our keyfile(manifesto), when a client sends their manifesto to the server it runs through this. */
static int fastfile_pipe_to_keyfile(request_rec *r, apr_file_t **fh, char *datfile){
	int i, rc, len_read;
	unsigned char buf[FF_BUFSIZE_LARGE];
	long long len, w;
	unsigned int chunksize, num_chunks, expected_chunks;
	unsigned long long filesize;
	apr_finfo_t finfo;
	apr_file_t *ft = NULL;

	if((rc = ap_setup_client_block(r, REQUEST_CHUNKED_ERROR)) != OK || !ap_should_client_block(r))
		return(HTTP_INTERNAL_SERVER_ERROR);

	len = r->remaining;
	w = 0;

	if(len > FF_MANIFESTO_MAX_SIZE)
		return(HTTP_REQUEST_URI_TOO_LARGE);

	/* v1's header has a static placement for the first 21(FF_MANIFESTO_BASE_HEADER_SIZE) bytes, if a future v2 is shorter it will fail here or fail by the version check. */
	/* NOTE: FF_BUFSIZE >= 21 should always be true...if it doesn't then this is a major security problem. */
	if(ap_get_client_block(r, buf, FF_MANIFESTO_BASE_HEADER_SIZE) != FF_MANIFESTO_BASE_HEADER_SIZE || buf[0] != FF_PROTOCOL)
		return(HTTP_VERSION_NOT_SUPPORTED);

	len -= FF_MANIFESTO_BASE_HEADER_SIZE;

	/* do some checks below to ensure the manifesto initialization isn't malformed. */
	filesize = (unsigned long long)buf[5];
	filesize += (unsigned long long)buf[6] << 8;
	filesize += (unsigned long long)buf[7] << 16;
	filesize += (unsigned long long)buf[8] << 24;
	filesize += (unsigned long long)buf[9] << 32;
	filesize += (unsigned long long)buf[10] << 40;
	filesize += (unsigned long long)buf[11] << 48;
	filesize += (unsigned long long)buf[12] << 56;

	chunksize = buf[13];
	chunksize += buf[14] << 8;
	chunksize += buf[15] << 16;
	chunksize += buf[16] << 24;

	num_chunks = buf[17];
	num_chunks += buf[18] << 8;
	num_chunks += buf[19] << 16;
	num_chunks += buf[20] << 24;

	if(!filesize || !chunksize || !num_chunks)
		return(HTTP_NOT_ACCEPTABLE);

	/* basic restriction on client/server for optimal crc calculation speed. */
	if(chunksize % 8 || chunksize < FF_BUFSIZE_CHUNKSIZE)
		return(HTTP_NOT_ACCEPTABLE);

	/* the remaining bytes should be crcs for the remaining chunks(4bytes) + status of chunks(1byte), if this isn't correct then it's malformed. */
	expected_chunks = (unsigned int)((unsigned long long)filesize/chunksize) + ( !((unsigned long long)filesize%chunksize) ? 0 : 1);
	if(num_chunks != expected_chunks || expected_chunks * 4 + expected_chunks != len)
		return(HTTP_NOT_ACCEPTABLE);

	/* if a datfile already exists at a certain size that means the file uploaded should be no larger than that size. */
	if(!access(datfile, F_OK)) {
		if(!fastfile_is_safe(datfile, r) || apr_stat(&finfo, datfile, APR_FINFO_SIZE, r->pool) != APR_SUCCESS)
			return(HTTP_INTERNAL_SERVER_ERROR);

		/* 0-byte file means the client can determine the filesize with no bounds, or client has a file smaller than the server allocated for, shrink it down. */
		if(!finfo.size || filesize < finfo.size) {
			if(apr_file_open(&ft, datfile, APR_FOPEN_WRITE | APR_FOPEN_TRUNCATE, APR_OS_DEFAULT, r->pool) != APR_SUCCESS)
				return(HTTP_INTERNAL_SERVER_ERROR);
			if(apr_file_trunc(ft, (apr_off_t)filesize) != APR_SUCCESS)
				return(HTTP_INTERNAL_SERVER_ERROR);
			apr_file_close(ft);
		}

		/* client wants to send a file larger than allowed, reject. */
		else if(filesize > finfo.size)
			return(HTTP_REQUEST_ENTITY_TOO_LARGE);

	}

	/* no datfile exists, this should have been done when the key was created. */
	else
		return(HTTP_PRECONDITION_FAILED);

	/* passed basic structure test, allow it to be written as the manifesto. */
	fastfile_write(fh, buf, FF_MANIFESTO_BASE_HEADER_SIZE);

	while((len_read = ap_get_client_block(r, buf, FF_BUFSIZE_LARGE)) > 0){
		w += len_read;

		if(w > FF_MANIFESTO_MAX_SIZE)
			return(HTTP_REQUEST_URI_TOO_LARGE);

		if(fastfile_write(fh, buf, len_read))
			return(HTTP_INTERNAL_SERVER_ERROR);
	}
	return(HTTP_OK);
}

/* handles chunk data, all chunks filling in our file run through this. */
static int fastfile_pipe_to_datfile(request_rec *r, unsigned int chunk, apr_file_t **fk, unsigned long long fk_len, apr_file_t **fd, unsigned long long fd_len){
	int i, rc, len;
	unsigned char buf[FF_BUFSIZE_LARGE];
	unsigned int crc, crc_chunk, chunksize, num_chunks, expected_chunksize;
	unsigned long long filesize, start;
	apr_size_t klen;

	klen = FF_MANIFESTO_BASE_HEADER_SIZE;
	if(apr_file_read(*fk, buf, &klen) != APR_SUCCESS)
		return(HTTP_INTERNAL_SERVER_ERROR);

	/* do some checks below to ensure the manifesto initialization isn't malformed. (nothing that can make the file grow bigger than allowed) */
	filesize = (unsigned long long)buf[5];
	filesize += (unsigned long long)buf[6] << 8;
	filesize += (unsigned long long)buf[7] << 16;
	filesize += (unsigned long long)buf[8] << 24;
	filesize += (unsigned long long)buf[9] << 32;
	filesize += (unsigned long long)buf[10] << 40;
	filesize += (unsigned long long)buf[11] << 48;
	filesize += (unsigned long long)buf[12] << 56;

	chunksize = buf[13];
	chunksize += buf[14] << 8;
	chunksize += buf[15] << 16;
	chunksize += buf[16] << 24;

	num_chunks = buf[17];
	num_chunks += buf[18] << 8;
	num_chunks += buf[19] << 16;
	num_chunks += buf[20] << 24;

	/* the data we're receiving should be this long. */
	expected_chunksize = (chunk == num_chunks && (filesize % chunksize)) ? (filesize % chunksize) : chunksize;

	/* expected size and basic restriction on client/server for optimal crc calculation speed. (this should have been caught before, but perhaps the user malformed it after creation) */
	if(!expected_chunksize || chunksize % 8 || chunksize < FF_BUFSIZE_CHUNKSIZE)
		return(HTTP_INTERNAL_SERVER_ERROR);

	/* set 'start' to where we want to start reading rge key file at (to get the crc of this chunk) */
	klen = 4;
	start = FF_MANIFESTO_BASE_HEADER_SIZE + ((chunk - 1) * 4);
	if(start + 4 > fk_len || fastfile_seek(fk, start) || apr_file_read(*fk, buf, &klen) != APR_SUCCESS)
		return(HTTP_INTERNAL_SERVER_ERROR);

	crc_chunk = buf[0];
	crc_chunk += buf[1] << 8;
	crc_chunk += buf[2] << 16;
	crc_chunk += buf[3] << 24;

	/* set 'start' to where we want to start writing this chunk. */
	start = (unsigned long long)(chunk - 1) * chunksize;

	/* there is risk of an intentionally corrupt data packet being larger than the file by chunksize-1 at the worst, causing the file to grow slightly beyond set bounds. */
	if(start > filesize || start > fd_len)
		return(HTTP_REQUEST_ENTITY_TOO_LARGE);

	/* size of datafile and size recorded should match, and we need to go where we're seeking or we'll compromise other data in the file. */
	if(fd_len != filesize || fastfile_seek(fd, start))
		return(HTTP_INTERNAL_SERVER_ERROR);

	/* setup to receive the appropriate amount of data. */
	if((rc = ap_setup_client_block(r, REQUEST_CHUNKED_ERROR)) != OK || !ap_should_client_block(r) || expected_chunksize != r->remaining)
		return(HTTP_INTERNAL_SERVER_ERROR);

	crc = FF_CRC_INIT;
	while((len = ap_get_client_block(r, buf, FF_BUFSIZE_LARGE < expected_chunksize ? FF_BUFSIZE_LARGE : expected_chunksize)) > 0){
		/* add this buffer segment to our crc calculation. */
		crc = fastfile_crc32_64bit(buf, len, crc);

		/* proxy data to the destination datfile. */
		if(fastfile_write(fd, buf, len))
			return(HTTP_INTERNAL_SERVER_ERROR);

		/* subtract from needed, or exit if done. (expected_chunksize is not used after this point, no reason to set it to 0) */
		if(len < expected_chunksize)
			expected_chunksize -= len;
		else
			break;
	}

	/* crc of packet match our recorded crc? mark chunk as a success, otherwise pretend nothing happened. */
	if(crc == crc_chunk) {
		start = FF_MANIFESTO_BASE_HEADER_SIZE + (num_chunks * 4) + (chunk - 1);
		if(start + 1 > fk_len || fastfile_seek(fk, start))
			return(HTTP_INTERNAL_SERVER_ERROR);
		buf[0] = FF_CHUNK_SENT;
		if(fastfile_write(fk, buf, 1))
			return(HTTP_INTERNAL_SERVER_ERROR);
	}
	else
		return(HTTP_NOT_ACCEPTABLE);

	return(HTTP_OK);
}

/* read file to client's socket. */
static void fastfile_read_to_pipe(request_rec *r, apr_file_t **fh) {
	unsigned char buf[FF_BUFSIZE_LARGE];
	apr_size_t len = FF_BUFSIZE_LARGE;
	while (apr_file_read(*fh, buf, &len) == APR_SUCCESS) {
		if (ap_rwrite(buf, len, r) < 0)
			break;
	}
	return;
}

/* root handler for mod_fastfile, all client requests run through this. */
static int fastfile_handler(request_rec *r) {
	char *pooldir, *keyfile, *datfile, *req, *key, *type, *ptr;
	unsigned int chunk;
	int rc;
	unsigned long long fk_len, fd_len;
	apr_file_t *fk=NULL, *fd=NULL;
	apr_finfo_t finfo;

	if(!r->handler || strcmp(r->handler, "fastfile"))
		return(DECLINED);
	if(r->header_only)
		return(OK);

	if(r->method_number != M_POST)
		return(HTTP_METHOD_NOT_ALLOWED);

	/* check that the config file exists, the pooldir exists, and check the permissions. (soft sanity check) */
	if(!(pooldir = fastfile_conf_get("pool", r)))
		return(HTTP_SERVICE_UNAVAILABLE);
	if(apr_stat(&finfo, pooldir, APR_FINFO_PROT, r->pool) != APR_SUCCESS || apr_unix_perms2mode(finfo.protection) != FF_POOL_DIR_MODE)
		return(HTTP_SERVICE_UNAVAILABLE);

	/* we need a key to proceed, discount blank '/'. */
	if(!r->path_info || strlen(r->path_info) < 2)
		return(HTTP_BAD_REQUEST);

	/* allocate the key and remove the preceding slash. */
	if(!(req = apr_pstrdup(r->pool, r->path_info)))
		return(HTTP_INTERNAL_SERVER_ERROR);
	if(!(key = index(req, '/')) || !*++key)
		return(HTTP_BAD_REQUEST);

	/* remove trailing slash and see if we have a chunk to process. */
	if((ptr = index(key, '/')) && !(*ptr++ = 0) && *ptr)
		chunk = atoi(ptr);
	else
		chunk = 0;

	/* invalid characters in the key? */
	if(fastfile_key_validate(key))
		return(HTTP_BAD_REQUEST);

	/* create and check for the keyfile's existence. (and can be written to) */
	if(!(keyfile = apr_psprintf(r->pool, "%s/%s", pooldir, key)))
		return(HTTP_INTERNAL_SERVER_ERROR);
	if(!(datfile = apr_psprintf(r->pool, "%s/%s.dat", pooldir, key)))
		return(HTTP_INTERNAL_SERVER_ERROR);
	if(access(keyfile, F_OK))
		return(HTTP_NOT_FOUND);
	if(access(keyfile, R_OK | W_OK))
		return(HTTP_SERVICE_UNAVAILABLE);

	/* no chunk, must be the initialization manifesto. */
	if(!chunk) {
		if(!fastfile_is_safe(keyfile, r) || apr_stat(&finfo, keyfile, APR_FINFO_SIZE, r->pool) != APR_SUCCESS)
			return(HTTP_INTERNAL_SERVER_ERROR);

		/* manifesto already exists, just send what we have back to the client and see if it matches on their end. */
		if((apr_size_t)finfo.size > 0) {
			r->content_type = "application/octet-stream";
			fastfile_pipe_to_nothing(r);
			if(!fastfile_open(r, keyfile, &fk, APR_FOPEN_READ | APR_FOPEN_BINARY)) {
				r->status = HTTP_NOT_MODIFIED;
				fastfile_read_to_pipe(r, &fk);
			}
			else
				return(HTTP_INTERNAL_SERVER_ERROR);
		}

		/* un-initialized manifesto, let them create it. */
		else {
			if(!fastfile_open(r, keyfile, &fk, APR_FOPEN_WRITE | APR_FOPEN_BINARY)) {
				if((rc=fastfile_pipe_to_keyfile(r, &fk, datfile)) == HTTP_OK)
					r->status = HTTP_CREATED;
				else
					return(rc);
			}
			else
				return(HTTP_INTERNAL_SERVER_ERROR);
		}
	}

	/* a chunk, write it to the correct spot in the data file. */
	else {

		/* fetch keyfile/datfile sizes for later use, also make sure they aren't links/etc. */
		if(!fastfile_is_safe(keyfile, r) || apr_stat(&finfo, keyfile, APR_FINFO_SIZE, r->pool) != APR_SUCCESS)
			return(HTTP_INTERNAL_SERVER_ERROR);
		fk_len = (apr_size_t)finfo.size;
		if(!fastfile_is_safe(datfile, r) || apr_stat(&finfo, datfile, APR_FINFO_SIZE, r->pool) != APR_SUCCESS)
			return(HTTP_INTERNAL_SERVER_ERROR);
		fd_len = (apr_size_t)finfo.size;

		/* open keyfile/datfile for later use below. */
		if(fastfile_open(r, keyfile, &fk, APR_FOPEN_WRITE | APR_FOPEN_READ | APR_FOPEN_BINARY))
			return(HTTP_INTERNAL_SERVER_ERROR);
		else if(fastfile_open(r, datfile, &fd, APR_FOPEN_CREATE | APR_FOPEN_WRITE | APR_FOPEN_BINARY)) {
			 apr_file_close(fk);
			return(HTTP_INTERNAL_SERVER_ERROR);
		}
		else {
			if((rc=fastfile_pipe_to_datfile(r, chunk, &fk, fk_len, &fd, fd_len)) == HTTP_OK)
				r->status = HTTP_ACCEPTED;
			else
				return(rc);
		}

	}

	/* rarely will make it here, apr garbage collection should be handling this and cleaning up the early exits above. */
	if(fk)
		apr_file_close(fk);
	if(fd)
		apr_file_close(fd);

	return(OK);
}

/* register our module's hook. */
static void fastfile_register_hooks(apr_pool_t *p) {
	ap_hook_handler(fastfile_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

/* API hooks. */
module AP_MODULE_DECLARE_DATA fastfile_module = { STANDARD20_MODULE_STUFF, NULL, NULL, NULL, NULL, NULL, fastfile_register_hooks };
