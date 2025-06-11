/*
 * Copyright (C) 2012 Jeremy Kerr <jeremy.kerr@canonical.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 3
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the OpenSSL
 * library under certain conditions as described in each individual source file,
 * and distribute linked combinations including the two.
 *
 * You must obey the GNU General Public License in all respects for all
 * of the code used other than OpenSSL. If you modify file(s) with this
 * exception, you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do
 * so, delete this exception statement from your version. If you delete
 * this exception statement from all source files in the program, then
 * also delete it here.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/engine.h>

#include <ccan/talloc/talloc.h>
#include <ccan/read_write_all/read_write_all.h>

#include "fileio.h"

static void print_openssl_errors(void)
{
	unsigned long err;
	while ((err = ERR_get_error()) != 0) {
		char err_buf[256];
		ERR_error_string_n(err, err_buf, sizeof(err_buf));
		fprintf(stderr, "OpenSSL error: %s\n", err_buf);
	}
}

#define FLAG_NOERROR	(1<<0)

static int ui_read(UI *ui, UI_STRING *uis)
{
	char password[128];

	if (UI_get_string_type(uis) != UIT_PROMPT)
		return 0;

	EVP_read_pw_string(password, sizeof(password), "Enter engine key pass phrase:", 0);
	UI_set_result(ui, uis, password);
	return 1;
}

EVP_PKEY *fileio_read_engine_key(const char *engine, const char *filename)
{
	UI_METHOD *ui;
	ENGINE *e;
	EVP_PKEY *pkey = NULL;

	ENGINE_load_builtin_engines();
	e = ENGINE_by_id(engine);

	if (!e) {
		fprintf(stderr, "Failed to load engine: %s\n", engine);
		print_openssl_errors();
		return NULL;
	}

	ui = UI_create_method("sbsigntools");
	if (!ui) {
		fprintf(stderr, "Failed to create UI method\n");
		print_openssl_errors();
		goto out_free;
	}
	UI_method_set_reader(ui, ui_read);

	if (!ENGINE_init(e)) {
		fprintf(stderr, "Failed to initialize engine %s\n", engine);
		print_openssl_errors();
		goto out_free;
	}

	pkey = ENGINE_load_private_key(e, filename, ui, NULL);
	// ENGINE_finish(e);

 out_free:
	ENGINE_free(e);
	return pkey;
}

EVP_PKEY *fileio_read_pkey_engine(const char *filename, const char *engine, const char *keyform)
{
	EVP_PKEY *pkey = NULL;
	
	if (engine) {
		// Load key using engine
		if (keyform && strcmp(keyform, "engine") == 0) {
			pkey = fileio_read_engine_key(engine, filename);
		} else {
			fprintf(stderr, "Warning: engine specified but keyform is not 'engine', using file-based loading\n");
			pkey = fileio_read_pkey(filename);
		}
	} else {
		// Traditional file-based key loading
		pkey = fileio_read_pkey(filename);
	}
	
	return pkey;
}

EVP_PKEY *fileio_read_pkey(const char *filename)
{
	EVP_PKEY *key = NULL;
	BIO *bio;

	bio = BIO_new_file(filename, "r");
	if (!bio)
		goto out;

	key = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);

out:
	BIO_free_all(bio);
	if (!key) {
		fprintf(stderr, "Can't load key from file '%s'\n", filename);
		print_openssl_errors();
	}
	return key;
}

X509 *fileio_read_cert(const char *filename)
{
	X509 *cert = NULL;
	BIO *bio;

	bio = BIO_new_file(filename, "r");
	if (!bio)
		goto out;

	cert = PEM_read_bio_X509(bio, NULL, NULL, NULL);

out:
	BIO_free_all(bio);
	if (!cert) {
		fprintf(stderr, "Can't load certificate from file '%s'\n",
				filename);
		print_openssl_errors();
	}
	return cert;
}

static int __fileio_read_file(void *ctx, const char *filename,
		 uint8_t **out_buf, size_t *out_len, int flags)
{
	struct stat statbuf;
	uint8_t *buf;
	size_t len;
	int fd, rc;

	rc = -1;

	fd = open(filename, O_RDONLY);
	if (fd < 0)
		goto out;

	rc = fstat(fd, &statbuf);
	if (rc)
		goto out;

	len = statbuf.st_size;

	buf = talloc_array(ctx, uint8_t, len);
	if (!buf)
		goto out;

	if (!read_all(fd, buf, len))
		goto out;

	rc = 0;

out:
	if (fd >= 0)
		close(fd);
	if (rc) {
		if (!(flags & FLAG_NOERROR))
			fprintf(stderr, "Error reading file %s: %s\n",
					filename, strerror(errno));
	} else {
		*out_buf = buf;
		*out_len = len;
	}
	return rc;

}

int fileio_read_file(void *ctx, const char *filename,
		 uint8_t **out_buf, size_t *out_len)
{
	return __fileio_read_file(ctx, filename, out_buf, out_len, 0);
}

int fileio_read_file_noerror(void *ctx, const char *filename,
		 uint8_t **out_buf, size_t *out_len)
{
	return __fileio_read_file(ctx, filename, out_buf, out_len,
			FLAG_NOERROR);
}

int fileio_write_file(const char *filename, uint8_t *buf, size_t len)
{
	int fd;

	fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		perror("open");
		return -1;
	}

	if (!write_all(fd, buf, len)) {
		perror("write_all");
		close(fd);
		return -1;
	}

	close(fd);
	return 0;
}
