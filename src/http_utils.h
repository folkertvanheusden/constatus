// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#pragma once
#include "config.h"

#if HAVE_OPENSSL == 1
SSL_CTX *create_context(const ssl_pars_t & sp);
#endif
void CLOSE_SSL(h_handle_t & hh);
int AVAILABLE_SSL(h_handle_t & hh);
int READ_SSL(h_handle_t & hh, char *whereto, int len);
int WRITE_SSL(h_handle_t & hh, const char *wherefrom, int len);
void ACCEPT_SSL(h_handle_t & hh);
