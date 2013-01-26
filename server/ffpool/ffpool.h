/* [fastfile] ffpool.h :: specific prototypes / definitions for ffpool.c
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

void ff_exit(int);
void ff_error(unsigned char, char *, ...);
unsigned int ff_crc32_64bit(void *, unsigned long long, unsigned int);
char ff_pool_setup(char *);
void ff_conf_trim(char *);
char *ff_conf_init();
int ff_lock(char *, int, int, mode_t);
int ff_lock_pidfile(char *);
void ff_handle_keyfile(char *, struct stat);
char ff_is_keyfile(unsigned char *);
void ff_process_pool();
void ff_pool_loop();
