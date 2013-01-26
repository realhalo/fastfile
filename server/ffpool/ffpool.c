/* [fastfile] ffpool.c :: daemon to monitor pool directory for mod_fastfile.
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
#include "../config.h" /* configure output. */
#include "../../include/fastfile.h" /* common defines used throughout fastfile. */
#include "ffpool.h" /* prototypes specifically related to ffpool. */

/* globals / externals. */
struct ff_pool_s ff_pool;


/* functions. */

/* all exits pass through this. */
void ff_exit(int e) {
	exit(e);
}

/* all-purpose error handler, makes life easier. */
void ff_error(unsigned char type, char *fmt, ...) {
	char buf[FF_BUFSIZE_LARGE + 1];
	va_list ap;

	memset(buf, 0, FF_BUFSIZE_LARGE);
	va_start(ap, fmt);
	vsnprintf(buf, FF_BUFSIZE_LARGE, fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s: %s\n", (type == FF_MSG_ERR ? "error" : "warning"), buf);
	fflush(stderr);

	if(type == FF_MSG_ERR)
		ff_exit(FF_EXIT_ERROR);

	return;
}

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

/* setup the pooldir, create and set modes if needed. */
char ff_pool_setup(char *pool) {
	char buf[PATH_MAX+1], **ptr;
	gid_t gid;
	struct stat st;
	struct group *gr;

	/* check that the path isn't an illegial path. */
	memset(buf, 0, PATH_MAX+1);
	realpath(pool, buf);
	for(ptr=ff_pool_illegal_dirs; *ptr; *ptr++) {
		if(!strcasecmp(*ptr, buf) || !strcasecmp(*ptr, pool))
			return(-1);
	}

	/* file/directory doesnt exist, try to make it. (this can fail in many ways but will be caught afterwords) */
	if(lstat(pool, &st))
		mkdir(pool, 0);

	/* no directory exists. */
	if(lstat(pool, &st) || !(S_ISDIR(st.st_mode)))
		return(-1);

	/* see if apache_group is a numeric gid, otherwise get the group entry. */
	if(*ff_pool.apache_group && isdigit(*ff_pool.apache_group))
		gid = atoi(ff_pool.apache_group);
	else if(!(gr = getgrnam(ff_pool.apache_group)))
		gid = FF_CONF_OPTION_APACHE_GID_DEFAULT;
	else
		gid = gr->gr_gid;

	/* root:apache_group(www-data) */
	chown(pool, 0, gid);

	/* drwx-ws-wx */
	chmod(pool, FF_POOL_DIR_MODE);

	return(0);
}

/* trim whitepsace of the start/end of a string. */
void ff_conf_trim(char *buf) {
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

/* read the conf file and (re-)initialize. (or update/overwrite) */
char *ff_conf_init() {
	char *buf, *ptr;
	FILE *fh;

	if(!(fh=fopen(FF_CONF_PATH, "r")))
		ff_error(FF_MSG_WARN, "fastfile configuration is not accessible: %s");
	if(!(buf = malloc(FF_BUFSIZE_LARGE + 1)))
		ff_error(FF_MSG_ERR, "failed to allocate memory.");
	while(!feof(fh) && fgets(buf, FF_BUFSIZE_LARGE, fh)) {
		if(buf[0] != '#' && (ptr=index(buf, '='))) {
			*ptr++ = 0;
			ff_conf_trim(buf);
			ff_conf_trim(ptr);
			if(*buf && *ptr) {

				/* set pooldir, there will always be a previous buffer to free(). */
				if(!strcasecmp(buf, "pool")) {
					free(ff_pool.dir);
					if(!(ff_pool.dir = strdup(ptr)))
						ff_error(FF_MSG_ERR, "failed to duplicate memory.");
				}

				/* set apache_group, on error www-data will be used later. */
				else if(!strcasecmp(buf, "apache_group")) {
					free(ff_pool.apache_group);
					if(!(ff_pool.apache_group = strdup(ptr)))
						ff_error(FF_MSG_ERR, "failed to duplicate memory.");
				}

				/* set ttl, if 0 is set ttl will be ignored. */
				else if(!strcasecmp(buf, "ttl"))
					ff_pool.ttl = atoi(ptr);

				/* set mode, if unset a default is used. */
				else if(!strcasecmp(buf, "mode"))
					ff_pool.mode = strtol(ptr, 0, 8);

				/* set overwrite, if default is no. */
				else if(!strcasecmp(buf, "overwrite"))
					ff_pool.overwrite = (strcasecmp(ptr, "yes") ? 0 : 1);
			}
		}
	}
	free(buf);
	fclose(fh);

	// make sure if the conf changed the pool directory we handle accordingly.
	if(ff_pool_setup(ff_pool.dir))
		ff_error(FF_MSG_ERR, "could not create/use pool directory: %s", ff_pool.dir);
	if(chdir(ff_pool.dir))
		ff_error(FF_MSG_ERR, "could not change to pool directory: %s", ff_pool.dir);

	return(NULL);
}

/* attempt to (write/test) lock a file, return the fd if successful for non-tests. */
int ff_lock(char *file, int cmd, int flags, mode_t mode) {
	int fd;
	if(!(fd = open(file, flags, mode)) < 0)
		return(-1);
	if(lockf(fd, cmd, 0) < 0) {
		close(fd);
		return(-1);
	}

	/* test doesn't want to keep this open. */
	if(cmd == F_TEST) {
		close(fd);
		fd = 0;
	}
	return(fd);
}

/* the main lockfile for ffpool, makes sure we're only running once. */
int ff_lock_pidfile(char *file) {
	char buf[16];
	if((ff_pool.lock_fd = ff_lock(file, F_TLOCK, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR)) < 0)
		return(-1);
	sprintf(buf, "%d\n", getpid());
	ftruncate(ff_pool.lock_fd, 0);
	write(ff_pool.lock_fd, buf, strlen(buf));
	return(ff_pool.lock_fd);
}

/* reads the keyfile, determines if it's complete, then handles datfile and lnkfile accordingly. */
void ff_handle_keyfile(char *file, struct stat st) {
	struct passwd *pw;
	struct stat st2;
	char *path, dest[FF_BUFSIZE_LARGE+1];
	unsigned char buf[FF_BUFSIZE_LARGE+1];
	int i, ws, fd;
	unsigned int val, crc;
	long long fs;
	size_t r;
	uid_t uid;
	gid_t gid;
	pid_t pid;
	FILE *fh;

	/* see if we already have something locked on this file. (save a potential fork(), -1 = locked) */
	if(ff_lock(file, F_TEST, O_RDWR, 0) < 0)
		return;

	/* first of two forks, this fork will let a parent (as root) and a child (as the user) deal with the keyfile and cleanup accordingly while the rest of the program continues. */
	if(!fork()) {

		/* see if we already have something locked on this file, and lock it if not. */
		if((fd = ff_lock(file, F_TLOCK, O_RDWR, 0)) < 0)
			_exit(FF_EXIT_ERROR);

		/* we want the following fork to report back to the parent. */
		signal(SIGCHLD, SIG_DFL);

		/* we do all handling of the keyfile as the user that created the file. */
		/* parent will wait for the child to exit. (main process continues on, while the parent holds the lock) */
		switch((pid=fork())) {
			case -1: /* fork() failed. */
				break;
			case 0: /* child process. */
				close(STDIN_FILENO);
				close(STDOUT_FILENO);
				close(STDERR_FILENO);

				/* try to get the proper gid of the user back if possible. (otherwise the gid would remain www-data) */
				if((pw = getpwuid(st.st_uid))) {
					gid = pw->pw_gid;
					initgroups(pw->pw_name, gid);
				}
				else
					gid = st.st_gid;
				uid = st.st_uid;

				/* drop privileges. */
				setgid(gid);
				setegid(gid);
				setuid(uid);
				seteuid(uid);

				/* make sure we dropped privileges. */
				if(getuid() != uid || geteuid() != uid || getgid() != gid || getegid() != gid)
					_exit(FF_EXIT_ERROR);

				if(!(fh=fopen(file, "rb")))
					_exit(FF_EXIT_ERROR);

				/* get filesize. */
				fseek(fh, 0, SEEK_END);
				fs = ftell(fh);
				fseek(fh, 0, SEEK_SET);

				/* malformed manifesto or incorrect version. (FF_BUFSIZE_LARGE > FF_MANIFESTO_BASE_HEADER_SIZE). */
				if(fs < FF_MANIFESTO_BASE_HEADER_SIZE || fread(buf, 1, FF_MANIFESTO_BASE_HEADER_SIZE, fh) != FF_MANIFESTO_BASE_HEADER_SIZE || buf[0] != FF_PROTOCOL) {
					fclose(fh);
					_exit(FF_EXIT_ERROR);
				}

				/* get the number of chunks in the keyfile. */
				val = buf[17];
				val += buf[18] << 8;
				val += buf[19] << 16;
				val += buf[20] << 24;

				/* the statuses from all the chunks should be at the end of the file. */
				if(val > fs || fseek(fh, -(long)val, SEEK_END)) {
					fclose(fh);
					_exit(FF_EXIT_ERROR);
				}

				/* check all the chunk statuses. */
				while((i=fgetc(fh)) != EOF) {

					/* not finished receiving all chunks. */
					if((char)i != FF_CHUNK_SENT) {
						fclose(fh);
						_exit(FF_EXIT_ERROR);
					}
				}
				fclose(fh);

				/* -- assume the file is complete from here on, delete on error after this point -- */

				/* see if the link file exists and find the destination if it does.  */
				if(!(path = malloc(strlen(file) + 5)))
					_exit(FF_EXIT_ERROR);
				sprintf(path, "%s.lnk", file);

				/* make sure lnkfile exists and is a symlink. */
				memset(dest, 0, FF_BUFSIZE_LARGE+1);
				if(!lstat(path, &st2) && S_ISLNK(st2.st_mode) && readlink(path, dest, FF_BUFSIZE_LARGE) > 0) {

					/* reusing path buffer for the datfile, same string length. */
					sprintf(path, "%s.dat", file);

					/* make sure datfile is a normal/non-hardlinked file, and make sure we own it. */
					if(!lstat(path, &st2) && S_ISREG(st2.st_mode) && st2.st_nlink < 2) {

						/* basic effort to make sure the datfile matches the user at this point. */
						if(getuid() == st.st_uid) {

							/* get the overall crc for the file from the manifesto. (make sure it isn't corrupt, buf contains the first 21 bytes of the manifesto from earlier) */
							val = buf[1];
							val += buf[2] << 8;
							val += buf[3] << 16;
							val += buf[4] << 24;

							/* open the datfile to compare it against the crc in the manifesto. */
							if(!(fh=fopen(path, "rb")))
								syslog(LOG_ERR, "file completed, but data file could not be opened for reading[uid=%d,gid=%d]: %s", getuid(), getgid(), path);
							else {

								/* compute the crc of the data file. */
								crc = FF_CRC_INIT;
								while(!feof(fh) && (r=fread(buf, 1, FF_BUFSIZE_LARGE, fh)) > 0)
									crc = ff_crc32_64bit(buf, r, crc);
								fclose(fh);

								/* do the comparison. */
								if(val == crc) {

									/* try to return the user's gid back to their dat file. */
									chown(path, -1, gid);

									/* change the file permissions to stock default (0600) or "mode" value provided in fastfile.conf. */
									chmod(path, ff_pool.mode);

									if(!ff_pool.overwrite && !lstat(dest, &st2))
										syslog(LOG_ERR, "file completed, but destination exists and overwrite is disabled[uid=%d,gid=%d]: %s -> %s", getuid(), getgid(), path, dest);
									else if(!rename(path, dest))
										syslog(LOG_INFO, "file completed[uid=%d,gid=%d]: %s -> %s (%lld bytes, %08X crc)", getuid(), getgid(), path, dest, (long long)st2.st_size, crc);
									else
										syslog(LOG_ERR, "file completed, but could not be moved[uid=%d,gid=%d]: %s -> %s (%lld bytes)", getuid(), getgid(), path, dest, (long long)st2.st_size);
								}

								/* crc comparison failed. */
								else
									syslog(LOG_ERR, "file completed, but the recorded crc does not match the data file[uid=%d,gid=%d]: %s (%08X != %08X)", getuid(), getgid(), path, val, crc);
							}
						}

						/* uid of the datfile is incorrect. */
						else
							syslog(LOG_ERR, "file completed, but data file was an the incorrect uid[uid=%d,gid=%d]: %s (uid=%d)", getuid(), getgid(), path, st.st_uid);
					}

					/* stat on datfile failed. */
					else
						syslog(LOG_ERR, "file completed, but data file was an improper file[uid=%d,gid=%d]: %s", getuid(), getgid(), path);
				}

				/* stat on lnkfile failed. */
				else
					syslog(LOG_ERR, "file completed, but link file was an improper file[uid=%d,gid=%d]: %s", getuid(), getgid(), path);

				free(path);

				/* even if the file failed to be moved a success implies we're done here. */
				_exit(FF_EXIT_SUCCESS);
				break;
			default: /* parent process. */

				/* wait, and only proceed if the child returned FF_EXIT_SUCCESS. */
				if(waitpid(pid, &ws, 0) > 0 && WIFEXITED(ws) && !WEXITSTATUS(ws)) {

					/* if FF_EXIT_SUCCESS was returned from the child it implies we are done with the key. */
					/* delete all associated files as root to ensure they disappear. (as to not be infinitely re-processed) */
					if(!(path = malloc(strlen(file) + 5)))
						_exit(FF_EXIT_ERROR);
					sprintf(path, "%s", file);
					unlink(path); /* ditch the key file. */
					sprintf(path, "%s.dat", file);
					unlink(path); /* datfile may not be here anymore, but try anyways. */
					sprintf(path, "%s.lnk", file);
					unlink(path); /* ditch the link file. */
					free(path);
				}
				break;
		}

		/* free our lock on the keyfile. (if it's still there) */
		lockf(fd, F_UNLCK, 0);
		close(fd);

		_exit(FF_EXIT_SUCCESS);
	} /* end fork(). */
	return;
}

/* verify filename is a valid keyfile. */
char ff_is_keyfile(unsigned char *key) {
	unsigned char *ptr;
	for(ptr=key; *ptr; ptr++) {
		if(!strchr(FF_KEY_CHARS, *ptr))
			return(0);
	}
	return(ptr==key ? 0 : 1);
}

/* read the pool directory and handle accordingly. */
void ff_process_pool() {
	char *buf, *ptr;
	struct stat st;
	struct dirent *e;
	DIR *d;

	if(!(d = opendir(".")))
		return;

	while((e = readdir(d))) {

		/* we completely ignore any subdirectories, we'll need info about all files from this point so lstat() now. */
		if(e->d_type == DT_DIR || lstat(e->d_name, &st))
			continue;

		/* see if we should treat this file as a keyfile */
 		if(ff_is_keyfile((unsigned char *)e->d_name) && S_ISREG(st.st_mode) && st.st_nlink < 2 && st.st_mtime >= ff_pool.time)
			ff_handle_keyfile(e->d_name, st);

		/* file's age is past the ttl setting? delete. (if we're on the first run don't delete yet, get it on the second run incase ffpool hasn't been running) */
		if(ff_pool.time && ff_pool.ttl && (long long)time(0) - (long long)st.st_mtime > (long long)ff_pool.ttl) {

			/* if this is a datfile or lnkfile check the root keyfile to make sure it isn't in use. (could possibly be moving the file still) */
			if((ptr=index(e->d_name, '.')) && ptr - e->d_name > 0) {
				if(!(buf = malloc(ptr - e->d_name + 1)))
					ff_error(FF_MSG_ERR, "failed to allocate memory.");
				memset(buf, 0, ptr - e->d_name + 1);
				memcpy(buf, e->d_name, ptr - e->d_name);

				/* no lock on the keyfile? delete. (-1 = locked) */
				if(!ff_lock(buf, F_TEST, O_RDWR, 0) >= 0)
					unlink(e->d_name);
				free(buf);
			}

			/* the keyfile itself? see if it's locked and delete if not. */
			else if(!ff_lock(e->d_name, F_TEST, O_RDWR, 0) >= 0)
				unlink(e->d_name);
		}
	}
	closedir(d);

	return;
}

/* main process loop. */
void ff_pool_loop() {
	struct stat st;

	/* loop as long as we have our lock. */
	while(!lockf(ff_pool.lock_fd, F_TEST, 0)) {

		/* see if fastfile.conf has been updated. (removes the need for SIGHUP recaching) */
		if(stat(FF_CONF_PATH, &st))
			ff_error(FF_MSG_ERR, "configuration file inaccessible: %s", FF_CONF_PATH);
		else if(st.st_mtime >= ff_pool.time)
			ff_conf_init();

		/* handle all files in pool dir. */
		ff_process_pool();

		/* wait till the next run. */
		ff_pool.time = time(0);
		sleep(FF_POOL_CHECK_INTERVAL);
	}
	return;
}

/* MAIN */
int main(int argc, char **argv) {

	/* set default values. */
	ff_pool.time = (time_t)0;
	ff_pool.ttl = FF_CONF_OPTION_TTL_DEFAULT;
	ff_pool.mode = FF_CONF_OPTION_MODE_DEFAULT;
	ff_pool.overwrite = FF_CONF_OPTION_OVERWRITE_DEFAULT;
	if(!(ff_pool.dir = strdup(FF_CONF_OPTION_POOL_DEFAULT))) /* strdup'd to keep free() consistency. */
		ff_error(FF_MSG_ERR, "failed to duplicate memory.");
	if(!(ff_pool.apache_group = strdup(FF_CONF_OPTION_APACHE_GROUP_DEFAULT))) /* strdup'd to keep free() consistency. */
		ff_error(FF_MSG_ERR, "failed to duplicate memory.");

	/* variety of basic checks. */
	if(seteuid(0) || setuid(0) || setegid(0) || setgid(0))
		ff_error(FF_MSG_ERR, "this program needs to run as root.");

	/* daemonize ffpool. */
	switch(fork()) {
		case -1: /* error forking. */
			ff_error(FF_MSG_ERR, "failed to daemonize ffpool.");
			break;
		case 0: /* child process. (soon to be the real parent) */

			/* detatch / new group leader. */
			setsid();

			/* we won't be needing these. */
			close(STDIN_FILENO);
			close(STDOUT_FILENO);
			close(STDERR_FILENO);

			/* locked pidfile? already running, exit. */
			if(ff_lock_pidfile(FF_POOL_LOCKFILE) < 0)
				_exit(FF_EXIT_SUCCESS);

			/* call once to make sure everything is on the up and up. (this can be re-called later) */
			ff_conf_init();

			/* open syslog for potentially useful information. */
			openlog("ffpool", LOG_PID, LOG_USER);

			/* we want nothing to do with fork() returns unless otherwise stated...and any other signals we don't care about. */
			signal(SIGCHLD, SIG_IGN);
			signal(SIGHUP, SIG_IGN);
			signal(SIGPIPE, SIG_IGN);

			/* monitor directory and check for conf updates. (currently infinitely loops here unless the lockfile fails) */
			ff_pool_loop();

			/* release the lock on the pidfile. */
			lockf(ff_pool.lock_fd, F_UNLCK, 0);
			close(ff_pool.lock_fd);

			closelog();
			_exit(FF_EXIT_SUCCESS);

			break;
		default: /* parent process. (soon to be gone) */
			ff_exit(FF_EXIT_ERROR);
			break;
	}

	/* never makes it here, silences compiler. */
	return(0);
}
