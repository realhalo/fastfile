/* [fastfile] ffcp.h :: specific prototypes / definitions for ffcp.c
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

void ff_display_col_update();
void ff_display_update(char);
void ff_display_print(char *, ...);
void ff_display_api(unsigned char, unsigned char);
void ff_exit(int);
void ff_error(unsigned char, char *, ...);
unsigned int ff_get_chunksize(unsigned int);
struct sockaddr_in ff_atos(char *);
#ifdef FF_HAVE_IPV6
struct sockaddr_in6 ff_atos6(char *);
#endif
unsigned int ff_crc32_64bit(void *, unsigned long long, unsigned int);
void ff_32bit2buf(unsigned int, unsigned char *);
void ff_64bit2buf(unsigned long long, unsigned char *);
unsigned int ff_chunksize_guess(unsigned long long);
void ff_client_manifesto(char *, unsigned int);
void ff_set_header(char *, char *);
void ff_set_header_str(char *);
char *ff_get_headers(unsigned int);
char *ff_get_request_string(unsigned int);
void ff_send_manifesto();
void ff_parse_host_str(char *);
void ff_sig_handler(int);
char ff_sock_send_data(int);
char ff_sock_send_request(int);
int ff_sock_highest_fd();
void ff_sock_nonblock(int);
void ff_sock_free(int, unsigned char);
int ff_get_next_chunk(char);
#ifdef FF_HAVE_SSL
int ff_ssl_handshake(int);
#endif
char ff_sock_connect(int);
fd_set ff_sock_fd_set();
fd_set ff_sock_wfd_set();
void ff_sock_handler(fd_set, fd_set);
void ff_sock_loop();
void ff_sock_init();
