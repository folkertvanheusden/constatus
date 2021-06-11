// (C) 2017-2020 by folkert van heusden, released under AGPL v3.0
#include "config.h"
#include <unistd.h>
#if HAVE_OPENSSL == 1
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

#include "error.h"
#include "interface.h"
#include "http_server.h"
#include "log.h"

#if HAVE_OPENSSL == 1
SSL_CTX *create_context(const ssl_pars_t & sp)
{
	const SSL_METHOD *method = SSLv23_server_method();

	SSL_CTX *ctx = SSL_CTX_new(method);
	if (!ctx) {
		ERR_print_errors_fp(stderr);
		error_exit(false, "Unable to create SSL context");
	}

	SSL_CTX_set_ecdh_auto(ctx, 1);

	if (SSL_CTX_use_certificate_file(ctx, sp.certificate_file.c_str(), SSL_FILETYPE_PEM) <= 0) {
		ERR_print_errors_fp(stderr);
		error_exit(false, "Unable to set certificate file");
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, sp.key_file.c_str(), SSL_FILETYPE_PEM) <= 0 ) {
		ERR_print_errors_fp(stderr);
		error_exit(false, "Unable to set private key file");
	}

	return ctx;
}
#endif

void CLOSE_SSL(h_handle_t & hh)
{
#if HAVE_OPENSSL == 1
	if (hh.sh) {
		SSL_shutdown(hh.sh);
		SSL_free(hh.sh);
	}
#endif

        close(hh.fd);
}

int AVAILABLE_SSL(h_handle_t & hh)
{
#if HAVE_OPENSSL == 1
	if (hh.sh)
		return SSL_pending(hh.sh);
#endif

	return 0;
}

// note: can return less than requested
int READ_SSL(h_handle_t & hh, char *whereto, int len)
{
        for(;;) {
		int rc = -1;
#if HAVE_OPENSSL == 1
		if (hh.sh)
			rc = SSL_read(hh.sh, whereto, len);
		else
			rc = read(hh.fd, whereto, len);
#else
		rc = read(hh.fd, whereto, len);
#endif

		if (rc > 0)
			return rc;

#if HAVE_OPENSSL == 1
		if (hh.sh) {
			int err = SSL_get_error(hh.sh, rc);

			if (err != SSL_ERROR_NONE) {
				log(LL_WARNING, "SSL (read) error: %s", ERR_error_string(err, NULL));
				return -1;
			}
		}
		else
#endif
		{
			if (rc == 0)
				return 0;

			return -1;
		}
        }

	return -1;
}

// note: blocks until everything has been sent
int WRITE_SSL(h_handle_t & hh, const char *wherefrom, int len)
{
        int cnt = len;

        while(len > 0) {
		int rc = -1;

#if HAVE_OPENSSL == 1
		if (hh.sh) {
			rc = SSL_write(hh.sh, wherefrom, len);

			if (SSL_get_error(hh.sh, rc) == SSL_ERROR_WANT_READ) {
				printf("SSL_ERROR_WANT_READ\n");
				continue;
			}
		}
		else {
			rc = write(hh.fd, wherefrom, len);
		}
#else
		rc = write(hh.fd, wherefrom, len);
#endif

                if (rc == -1) {
#if HAVE_OPENSSL == 1
			if (hh.sh)
				log(LL_WARNING, "SSL (write) error: %s", ERR_error_string(ERR_get_error(), NULL));
#endif

			return -1;
		}

                if (rc == 0)
                        return 0;

		wherefrom += rc;
       		len -= rc;
        }

        return cnt;
}

void ACCEPT_SSL(h_handle_t & hh)
{
#if HAVE_OPENSSL == 1
	if (SSL_accept(hh.sh) <= 0)
		log(LL_WARNING, "SSL (accept) error: %s", ERR_error_string(ERR_get_error(), NULL));
#endif
}
