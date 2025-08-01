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
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

#include <getopt.h>

#include <openssl/conf.h>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/engine.h>

#include <ccan/talloc/talloc.h>

#include "idc.h"
#include "image.h"
#include "fileio.h"

static const char *toolname = "sbsign";

struct sign_context {
	struct image *image;
	const char *infilename;
	const char *outfilename;
	int verbose;
	int detached;
};

static struct option options[] = {
	{ "output", required_argument, NULL, 'o' },
	{ "cert", required_argument, NULL, 'c' },
	{ "key", required_argument, NULL, 'k' },
	{ "detached", no_argument, NULL, 'd' },
	{ "verbose", no_argument, NULL, 'v' },
	{ "help", no_argument, NULL, 'h' },
	{ "version", no_argument, NULL, 'V' },
	{ "engine", required_argument, NULL, 'e'},
	{ "keyform", required_argument, NULL, 'f'},
	{ "addcert", required_argument, NULL, 'a'},
	{ NULL, 0, NULL, 0 },
};

static void usage(void)
{
	printf("Usage: %s [options] --key <keyfile> --cert <certfile> "
			"<efi-boot-image>\n"
		"Sign an EFI boot image for use with secure boot.\n\n"
		"Options:\n"
		"\t--engine <eng>     use the specified engine to load the key\n"
		"\t--keyform <format> key format: PEM, DER, or engine\n"
		"\t--key <keyfile>    signing key (PEM-encoded RSA "
						"private key)\n"
		"\t--cert <certfile>  certificate (x509 certificate)\n"
		"\t--addcert <addcertfile> additional intermediate certificates in a file\n"
		"\t--detached         write a detached signature, instead of\n"
		"\t                    a signed binary\n"
		"\t--output <file>    write signed data to <file>\n"
		"\t                    (default <efi-boot-image>.signed,\n"
		"\t                    or <efi-boot-image>.pk7 for detached\n"
		"\t                    signatures)\n",
		toolname);
}

static void version(void)
{
	printf("%s %s\n", toolname, VERSION);
}

static void set_default_outfilename(struct sign_context *ctx)
{
	const char *extension;

	extension = ctx->detached ? "pk7" : "signed";

	ctx->outfilename = talloc_asprintf(ctx, "%s.%s",
			ctx->infilename, extension);
}

static int add_intermediate_certs(PKCS7 *p7, const char *filename)
{
	STACK_OF(X509_INFO) *certs;
	X509_INFO *cert;
	BIO *bio = NULL;
	int i;

	bio = BIO_new(BIO_s_file());
	if (!bio || BIO_read_filename(bio, filename) <=0) {
		fprintf(stderr,
			"error in reading intermediate certificates file\n");
		ERR_print_errors_fp(stderr);
		return -1;
	}

	certs = PEM_X509_INFO_read_bio(bio, NULL, NULL, NULL);
	if (!certs) {
		fprintf(stderr,
			"error in parsing intermediate certificates file\n");
		ERR_print_errors_fp(stderr);
		return -1;
	}

	for (i = 0; i < sk_X509_INFO_num(certs); i++) {
		cert = sk_X509_INFO_value(certs, i);
		PKCS7_add_certificate(p7, cert->x509);
	}

	sk_X509_INFO_pop_free(certs, X509_INFO_free);
	BIO_free_all(bio);

	return 0;
}

int main(int argc, char **argv)
{
	const char *keyfilename, *certfilename, *addcertfilename, *engine, *keyform;
	struct sign_context *ctx;
	uint8_t *buf, *tmp;
	int rc, c, sigsize;
	EVP_PKEY *pkey;

	ctx = talloc_zero(NULL, struct sign_context);

	keyfilename = NULL;
	certfilename = NULL;
	addcertfilename = NULL;
	engine = NULL;
	keyform = "PEM";

	for (;;) {
		int idx;
		c = getopt_long(argc, argv, "o:c:k:dvVhe:f:a:", options, &idx);
		if (c == -1)
			break;

		switch (c) {
		case 'o':
			ctx->outfilename = talloc_strdup(ctx, optarg);
			break;
		case 'c':
			certfilename = optarg;
			break;
		case 'k':
			keyfilename = optarg;
			break;
		case 'd':
			ctx->detached = 1;
			break;
		case 'v':
			ctx->verbose = 1;
			break;
		case 'V':
			version();
			return EXIT_SUCCESS;
		case 'h':
			usage();
			return EXIT_SUCCESS;
		case 'e':
			engine = optarg;
			break;
		case 'f':
			keyform = optarg;
			break;
		case 'a':
			addcertfilename = optarg;
			break;
		}
	}

	if (argc != optind + 1) {
		usage();
		return EXIT_FAILURE;
	}

	ctx->infilename = argv[optind];
	if (!ctx->outfilename)
		set_default_outfilename(ctx);

	if (!certfilename) {
		fprintf(stderr,
			"error: No certificate specified (with --cert)\n");
		usage();
		return EXIT_FAILURE;
	}
	if (!keyfilename) {
		fprintf(stderr,
			"error: No key specified (with --key)\n");
		usage();
		return EXIT_FAILURE;
	}

	ctx->image = image_load(ctx->infilename);
	if (!ctx->image)
		return EXIT_FAILURE;

	talloc_steal(ctx, ctx->image);

	ERR_load_crypto_strings();
	ERR_load_BIO_strings();
	OpenSSL_add_all_digests();
	OpenSSL_add_all_ciphers();
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	OPENSSL_config(NULL);
#else
	OPENSSL_init_crypto(OPENSSL_INIT_LOAD_CONFIG, NULL);
#endif
	/* here we may get highly unlikely failures or we'll get a
	 * complaint about FIPS signatures (usually becuase the FIPS
	 * module isn't present).  In either case ignore the errors
	 * (malloc will cause other failures out lower down */
	ERR_clear_error();
	
	// Initialize engines if needed
	ENGINE *signing_engine = NULL;
	if (engine) {
		ENGINE_load_builtin_engines();
		ENGINE_register_all_complete();
		signing_engine = ENGINE_by_id(engine);
		if (signing_engine) {
			ENGINE_init(signing_engine);
			// Set default engine for RSA operations during signing
			ENGINE_set_default_RSA(signing_engine);
			ENGINE_set_default(signing_engine, ENGINE_METHOD_ALL);
		}
	}
	
	pkey = fileio_read_pkey_engine(keyfilename, engine, keyform);
	if (!pkey) {
		if (signing_engine) {
			ENGINE_finish(signing_engine);
			ENGINE_free(signing_engine);
		}
		return EXIT_FAILURE;
	}

	X509 *cert = fileio_read_cert(certfilename);
	if (!cert)
		return EXIT_FAILURE;

	const EVP_MD *md = EVP_get_digestbyname("SHA256");

	/* set up the PKCS7 object */
	PKCS7 *p7 = PKCS7_new();
	PKCS7_set_type(p7, NID_pkcs7_signed);

	PKCS7_SIGNER_INFO *si = PKCS7_sign_add_signer(p7, cert,
			pkey, md, PKCS7_BINARY);
	if (!si) {
		fprintf(stderr, "error in key/certificate chain\n");
		ERR_print_errors_fp(stderr);
		return EXIT_FAILURE;
	}

	PKCS7_content_new(p7, NID_pkcs7_data);

	rc = IDC_set(p7, si, ctx->image);
	if (rc)
		return EXIT_FAILURE;

	if (addcertfilename && add_intermediate_certs(p7, addcertfilename))
		return EXIT_FAILURE;

	sigsize = i2d_PKCS7(p7, NULL);
	tmp = buf = talloc_array(ctx->image, uint8_t, sigsize);
	i2d_PKCS7(p7, &tmp);
	ERR_print_errors_fp(stdout);

	image_add_signature(ctx->image, buf, sigsize);

	if (ctx->detached) {
		int i;
		uint8_t *buf;
		size_t len;

		for (i = 0; !image_get_signature(ctx->image, i, &buf, &len); i++)
			;
		image_write_detached(ctx->image, i - 1, ctx->outfilename);
	} else
		image_write(ctx->image, ctx->outfilename);

	talloc_free(ctx);

	// Cleanup engines
	if (signing_engine) {
		ENGINE_finish(signing_engine);
		ENGINE_free(signing_engine);
		ENGINE_cleanup();
	}

	return EXIT_SUCCESS;
}

