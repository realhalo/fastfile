/* [fastfile] mod_fastfile.h :: specific prototypes / definitions for mod_fastfile.c
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

static char fastfile_key_validate(unsigned char *);
static unsigned int ff_crc32_64bit(void *, unsigned long long, unsigned int);
static char fastfile_open(request_rec *, char *, apr_file_t **, apr_fileperms_t);
static char fastfile_seek(apr_file_t **, unsigned long long);
static apr_size_t fastfile_write(apr_file_t **, void *, apr_size_t);
static void fastfile_trim(char *);
static char *fastfile_conf_get(char *, request_rec *);
static void fastfile_pipe_to_nothing(request_rec *);
static char fastfile_is_safe(char *, request_rec *);
static int fastfile_pipe_to_keyfile(request_rec *, apr_file_t **, char *);
static int fastfile_pipe_to_datfile(request_rec *, unsigned int, apr_file_t **, unsigned long long, apr_file_t **, unsigned long long);
static void fastfile_read_to_pipe(request_rec *, apr_file_t **);
static int fastfile_handler(request_rec *);
static void fastfile_register_hooks(apr_pool_t *);
