/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.] */

#include <stdio.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include "ssl_locl.h"

static int ssl_set_cert(CERT *c, X509 *x509);
static int ssl_set_pkey(CERT *c, EVP_PKEY *pkey);
#ifndef OPENSSL_NO_TLSEXT
static int ssl_set_authz(CERT *c, unsigned char *authz,
			 size_t authz_length);
#endif
int SSL_use_certificate(SSL *ssl, X509 *x)
	{
	if (x == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_certificate, ERR_R_PASSED_NULL_PARAMETER);
		return(0);
		}
	if (!ssl_cert_inst(&ssl->cert))
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_certificate, ERR_R_MALLOC_FAILURE);
		return(0);
		}
	return(ssl_set_cert(ssl->cert,x));
	}

#ifndef OPENSSL_NO_STDIO
int SSL_use_certificate_file(SSL *ssl, const char *file, int type)
	{
	int reason_code;
	BIO *in;
	int ret=0;
	X509 *x=NULL;

	in=BIO_new(BIO_s_file());
	if (in == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_certificate_file, ERR_R_BUF_LIB);
		goto end;
		}

	if (BIO_read_filename(in,file) <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_certificate_file, ERR_R_SYS_LIB);
		goto end;
		}
	if (type == SSL_FILETYPE_ASN1)
		{
		reason_code =ERR_R_ASN1_LIB;
		x=d2i_X509_bio(in,NULL);
		}
	else if (type == SSL_FILETYPE_PEM)
		{
		reason_code=ERR_R_PEM_LIB;
		x=PEM_read_bio_X509(in,NULL,ssl->ctx->default_passwd_callback,ssl->ctx->default_passwd_callback_userdata);
		}
	else
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_certificate_file, SSL_R_BAD_SSL_FILETYPE);
		goto end;
		}

	if (x == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_certificate_file,reason_code);
		goto end;
		}

	ret=SSL_use_certificate(ssl,x);
end:
	if (x != NULL) X509_free(x);
	if (in != NULL) BIO_free(in);
	return(ret);
	}
#endif

int SSL_use_certificate_ASN1(SSL *ssl, const unsigned char *d, int len)
	{
	X509 *x;
	int ret;

	x=d2i_X509(NULL,&d,(long)len);
	if (x == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_certificate_ASN1, ERR_R_ASN1_LIB);
		return(0);
		}

	ret=SSL_use_certificate(ssl,x);
	X509_free(x);
	return(ret);
	}

#ifndef OPENSSL_NO_RSA
int SSL_use_RSAPrivateKey(SSL *ssl, RSA *rsa)
	{
	EVP_PKEY *pkey;
	int ret;

	if (rsa == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_RSAPrivateKey, ERR_R_PASSED_NULL_PARAMETER);
		return(0);
		}
	if (!ssl_cert_inst(&ssl->cert))
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_RSAPrivateKey, ERR_R_MALLOC_FAILURE);
		return(0);
		}
	if ((pkey=EVP_PKEY_new()) == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_RSAPrivateKey, ERR_R_EVP_LIB);
		return(0);
		}

	RSA_up_ref(rsa);
	EVP_PKEY_assign_RSA(pkey,rsa);

	ret=ssl_set_pkey(ssl->cert,pkey);
	EVP_PKEY_free(pkey);
	return(ret);
	}
#endif

static int ssl_set_pkey(CERT *c, EVP_PKEY *pkey)
	{
	int i;
	/* Special case for DH: check two DH certificate types for a match.
	 * This means for DH certificates we must set the certificate first.
	 */
	if (pkey->type == EVP_PKEY_DH)
		{
		X509 *x;
		i = -1;
		x = c->pkeys[SSL_PKEY_DH_RSA].x509;
		if (x && X509_check_private_key(x, pkey))
				i = SSL_PKEY_DH_RSA;
		x = c->pkeys[SSL_PKEY_DH_DSA].x509;
		if (i == -1 && x && X509_check_private_key(x, pkey))
				i = SSL_PKEY_DH_DSA;
		ERR_clear_error();
		}
	else 
		i=ssl_cert_type(NULL,pkey);
	if (i < 0)
		{
		OPENSSL_PUT_ERROR(SSL, ssl_set_pkey, SSL_R_UNKNOWN_CERTIFICATE_TYPE);
		return(0);
		}

	if (c->pkeys[i].x509 != NULL)
		{
		EVP_PKEY *pktmp;
		pktmp =	X509_get_pubkey(c->pkeys[i].x509);
		EVP_PKEY_copy_parameters(pktmp,pkey);
		EVP_PKEY_free(pktmp);
		ERR_clear_error();

                /* TODO(fork): remove this? */
#if 0
#ifndef OPENSSL_NO_RSA
		/* Don't check the public/private key, this is mostly
		 * for smart cards. */
		if ((pkey->type == EVP_PKEY_RSA) &&
			(RSA_flags(pkey->pkey.rsa) & RSA_METHOD_FLAG_NO_CHECK))
			;
		else
#endif
#endif
		if (!X509_check_private_key(c->pkeys[i].x509,pkey))
			{
			X509_free(c->pkeys[i].x509);
			c->pkeys[i].x509 = NULL;
			return 0;
			}
		}

	if (c->pkeys[i].privatekey != NULL)
		EVP_PKEY_free(c->pkeys[i].privatekey);
	CRYPTO_add(&pkey->references,1,CRYPTO_LOCK_EVP_PKEY);
	c->pkeys[i].privatekey=pkey;
	c->key= &(c->pkeys[i]);

	c->valid=0;
	return(1);
	}

#ifndef OPENSSL_NO_RSA
#ifndef OPENSSL_NO_STDIO
int SSL_use_RSAPrivateKey_file(SSL *ssl, const char *file, int type)
	{
	int reason_code,ret=0;
	BIO *in;
	RSA *rsa=NULL;

	in=BIO_new(BIO_s_file());
	if (in == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_RSAPrivateKey_file, ERR_R_BUF_LIB);
		goto end;
		}

	if (BIO_read_filename(in,file) <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_RSAPrivateKey_file, ERR_R_SYS_LIB);
		goto end;
		}
	if	(type == SSL_FILETYPE_ASN1)
		{
		reason_code=ERR_R_ASN1_LIB;
		rsa=d2i_RSAPrivateKey_bio(in,NULL);
		}
	else if (type == SSL_FILETYPE_PEM)
		{
		reason_code=ERR_R_PEM_LIB;
		rsa=PEM_read_bio_RSAPrivateKey(in,NULL,
			ssl->ctx->default_passwd_callback,ssl->ctx->default_passwd_callback_userdata);
		}
	else
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_RSAPrivateKey_file, SSL_R_BAD_SSL_FILETYPE);
		goto end;
		}
	if (rsa == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_RSAPrivateKey_file,reason_code);
		goto end;
		}
	ret=SSL_use_RSAPrivateKey(ssl,rsa);
	RSA_free(rsa);
end:
	if (in != NULL) BIO_free(in);
	return(ret);
	}
#endif

int SSL_use_RSAPrivateKey_ASN1(SSL *ssl, unsigned char *d, long len)
	{
	int ret;
	const unsigned char *p;
	RSA *rsa;

	p=d;
	if ((rsa=d2i_RSAPrivateKey(NULL,&p,(long)len)) == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_RSAPrivateKey_ASN1, ERR_R_ASN1_LIB);
		return(0);
		}

	ret=SSL_use_RSAPrivateKey(ssl,rsa);
	RSA_free(rsa);
	return(ret);
	}
#endif /* !OPENSSL_NO_RSA */

int SSL_use_PrivateKey(SSL *ssl, EVP_PKEY *pkey)
	{
	int ret;

	if (pkey == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_PrivateKey, ERR_R_PASSED_NULL_PARAMETER);
		return(0);
		}
	if (!ssl_cert_inst(&ssl->cert))
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_PrivateKey, ERR_R_MALLOC_FAILURE);
		return(0);
		}
	ret=ssl_set_pkey(ssl->cert,pkey);
	return(ret);
	}

#ifndef OPENSSL_NO_STDIO
int SSL_use_PrivateKey_file(SSL *ssl, const char *file, int type)
	{
	int reason_code,ret=0;
	BIO *in;
	EVP_PKEY *pkey=NULL;

	in=BIO_new(BIO_s_file());
	if (in == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_PrivateKey_file, ERR_R_BUF_LIB);
		goto end;
		}

	if (BIO_read_filename(in,file) <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_PrivateKey_file, ERR_R_SYS_LIB);
		goto end;
		}
	if (type == SSL_FILETYPE_PEM)
		{
		reason_code=ERR_R_PEM_LIB;
		pkey=PEM_read_bio_PrivateKey(in,NULL,
			ssl->ctx->default_passwd_callback,ssl->ctx->default_passwd_callback_userdata);
		}
	else if (type == SSL_FILETYPE_ASN1)
		{
		reason_code = ERR_R_ASN1_LIB;
		pkey = d2i_PrivateKey_bio(in,NULL);
		}
	else
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_PrivateKey_file, SSL_R_BAD_SSL_FILETYPE);
		goto end;
		}
	if (pkey == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_PrivateKey_file,reason_code);
		goto end;
		}
	ret=SSL_use_PrivateKey(ssl,pkey);
	EVP_PKEY_free(pkey);
end:
	if (in != NULL) BIO_free(in);
	return(ret);
	}
#endif

int SSL_use_PrivateKey_ASN1(int type, SSL *ssl, const unsigned char *d, long len)
	{
	int ret;
	const unsigned char *p;
	EVP_PKEY *pkey;

	p=d;
	if ((pkey=d2i_PrivateKey(type,NULL,&p,(long)len)) == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_PrivateKey_ASN1, ERR_R_ASN1_LIB);
		return(0);
		}

	ret=SSL_use_PrivateKey(ssl,pkey);
	EVP_PKEY_free(pkey);
	return(ret);
	}

int SSL_CTX_use_certificate(SSL_CTX *ctx, X509 *x)
	{
	if (x == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_certificate, ERR_R_PASSED_NULL_PARAMETER);
		return(0);
		}
	if (!ssl_cert_inst(&ctx->cert))
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_certificate, ERR_R_MALLOC_FAILURE);
		return(0);
		}
	return(ssl_set_cert(ctx->cert, x));
	}

static int ssl_set_cert(CERT *c, X509 *x)
	{
	EVP_PKEY *pkey;
	int i;

	pkey=X509_get_pubkey(x);
	if (pkey == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, ssl_set_cert, SSL_R_X509_LIB);
		return(0);
		}

	i=ssl_cert_type(x,pkey);
	if (i < 0)
		{
		OPENSSL_PUT_ERROR(SSL, ssl_set_cert, SSL_R_UNKNOWN_CERTIFICATE_TYPE);
		EVP_PKEY_free(pkey);
		return(0);
		}

	if (c->pkeys[i].privatekey != NULL)
		{
		EVP_PKEY_copy_parameters(pkey,c->pkeys[i].privatekey);
		ERR_clear_error();

                /* TODO(fork): remove this? */
#if 0
#ifndef OPENSSL_NO_RSA
		/* Don't check the public/private key, this is mostly
		 * for smart cards. */
		if ((c->pkeys[i].privatekey->type == EVP_PKEY_RSA) &&
			(RSA_flags(c->pkeys[i].privatekey->pkey.rsa) &
			 RSA_METHOD_FLAG_NO_CHECK))
			 ;
		else
#endif /* OPENSSL_NO_RSA */
#endif
		if (!X509_check_private_key(x,c->pkeys[i].privatekey))
			{
			/* don't fail for a cert/key mismatch, just free
			 * current private key (when switching to a different
			 * cert & key, first this function should be used,
			 * then ssl_set_pkey */
			EVP_PKEY_free(c->pkeys[i].privatekey);
			c->pkeys[i].privatekey=NULL;
			/* clear error queue */
			ERR_clear_error();
			}
		}

	EVP_PKEY_free(pkey);

	if (c->pkeys[i].x509 != NULL)
		X509_free(c->pkeys[i].x509);
	CRYPTO_add(&x->references,1,CRYPTO_LOCK_X509);
	c->pkeys[i].x509=x;
	c->key= &(c->pkeys[i]);

	c->valid=0;
	return(1);
	}

#ifndef OPENSSL_NO_STDIO
int SSL_CTX_use_certificate_file(SSL_CTX *ctx, const char *file, int type)
	{
	int reason_code;
	BIO *in;
	int ret=0;
	X509 *x=NULL;

	in=BIO_new(BIO_s_file());
	if (in == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_certificate_file, ERR_R_BUF_LIB);
		goto end;
		}

	if (BIO_read_filename(in,file) <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_certificate_file, ERR_R_SYS_LIB);
		goto end;
		}
	if (type == SSL_FILETYPE_ASN1)
		{
		reason_code=ERR_R_ASN1_LIB;
		x=d2i_X509_bio(in,NULL);
		}
	else if (type == SSL_FILETYPE_PEM)
		{
		reason_code=ERR_R_PEM_LIB;
		x=PEM_read_bio_X509(in,NULL,ctx->default_passwd_callback,ctx->default_passwd_callback_userdata);
		}
	else
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_certificate_file, SSL_R_BAD_SSL_FILETYPE);
		goto end;
		}

	if (x == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_certificate_file,reason_code);
		goto end;
		}

	ret=SSL_CTX_use_certificate(ctx,x);
end:
	if (x != NULL) X509_free(x);
	if (in != NULL) BIO_free(in);
	return(ret);
	}
#endif

int SSL_CTX_use_certificate_ASN1(SSL_CTX *ctx, int len, const unsigned char *d)
	{
	X509 *x;
	int ret;

	x=d2i_X509(NULL,&d,(long)len);
	if (x == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_certificate_ASN1, ERR_R_ASN1_LIB);
		return(0);
		}

	ret=SSL_CTX_use_certificate(ctx,x);
	X509_free(x);
	return(ret);
	}

#ifndef OPENSSL_NO_RSA
int SSL_CTX_use_RSAPrivateKey(SSL_CTX *ctx, RSA *rsa)
	{
	int ret;
	EVP_PKEY *pkey;

	if (rsa == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_RSAPrivateKey, ERR_R_PASSED_NULL_PARAMETER);
		return(0);
		}
	if (!ssl_cert_inst(&ctx->cert))
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_RSAPrivateKey, ERR_R_MALLOC_FAILURE);
		return(0);
		}
	if ((pkey=EVP_PKEY_new()) == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_RSAPrivateKey, ERR_R_EVP_LIB);
		return(0);
		}

	RSA_up_ref(rsa);
	EVP_PKEY_assign_RSA(pkey,rsa);

	ret=ssl_set_pkey(ctx->cert, pkey);
	EVP_PKEY_free(pkey);
	return(ret);
	}

#ifndef OPENSSL_NO_STDIO
int SSL_CTX_use_RSAPrivateKey_file(SSL_CTX *ctx, const char *file, int type)
	{
	int reason_code,ret=0;
	BIO *in;
	RSA *rsa=NULL;

	in=BIO_new(BIO_s_file());
	if (in == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_RSAPrivateKey_file, ERR_R_BUF_LIB);
		goto end;
		}

	if (BIO_read_filename(in,file) <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_RSAPrivateKey_file, ERR_R_SYS_LIB);
		goto end;
		}
	if	(type == SSL_FILETYPE_ASN1)
		{
		reason_code=ERR_R_ASN1_LIB;
		rsa=d2i_RSAPrivateKey_bio(in,NULL);
		}
	else if (type == SSL_FILETYPE_PEM)
		{
		reason_code=ERR_R_PEM_LIB;
		rsa=PEM_read_bio_RSAPrivateKey(in,NULL,
			ctx->default_passwd_callback,ctx->default_passwd_callback_userdata);
		}
	else
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_RSAPrivateKey_file, SSL_R_BAD_SSL_FILETYPE);
		goto end;
		}
	if (rsa == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_RSAPrivateKey_file,reason_code);
		goto end;
		}
	ret=SSL_CTX_use_RSAPrivateKey(ctx,rsa);
	RSA_free(rsa);
end:
	if (in != NULL) BIO_free(in);
	return(ret);
	}
#endif

int SSL_CTX_use_RSAPrivateKey_ASN1(SSL_CTX *ctx, const unsigned char *d, long len)
	{
	int ret;
	const unsigned char *p;
	RSA *rsa;

	p=d;
	if ((rsa=d2i_RSAPrivateKey(NULL,&p,(long)len)) == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_RSAPrivateKey_ASN1, ERR_R_ASN1_LIB);
		return(0);
		}

	ret=SSL_CTX_use_RSAPrivateKey(ctx,rsa);
	RSA_free(rsa);
	return(ret);
	}
#endif /* !OPENSSL_NO_RSA */

int SSL_CTX_use_PrivateKey(SSL_CTX *ctx, EVP_PKEY *pkey)
	{
	if (pkey == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_PrivateKey, ERR_R_PASSED_NULL_PARAMETER);
		return(0);
		}
	if (!ssl_cert_inst(&ctx->cert))
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_PrivateKey, ERR_R_MALLOC_FAILURE);
		return(0);
		}
	return(ssl_set_pkey(ctx->cert,pkey));
	}

#ifndef OPENSSL_NO_STDIO
int SSL_CTX_use_PrivateKey_file(SSL_CTX *ctx, const char *file, int type)
	{
	int reason_code,ret=0;
	BIO *in;
	EVP_PKEY *pkey=NULL;

	in=BIO_new(BIO_s_file());
	if (in == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_PrivateKey_file, ERR_R_BUF_LIB);
		goto end;
		}

	if (BIO_read_filename(in,file) <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_PrivateKey_file, ERR_R_SYS_LIB);
		goto end;
		}
	if (type == SSL_FILETYPE_PEM)
		{
		reason_code=ERR_R_PEM_LIB;
		pkey=PEM_read_bio_PrivateKey(in,NULL,
			ctx->default_passwd_callback,ctx->default_passwd_callback_userdata);
		}
	else if (type == SSL_FILETYPE_ASN1)
		{
		reason_code = ERR_R_ASN1_LIB;
		pkey = d2i_PrivateKey_bio(in,NULL);
		}
	else
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_PrivateKey_file, SSL_R_BAD_SSL_FILETYPE);
		goto end;
		}
	if (pkey == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_PrivateKey_file,reason_code);
		goto end;
		}
	ret=SSL_CTX_use_PrivateKey(ctx,pkey);
	EVP_PKEY_free(pkey);
end:
	if (in != NULL) BIO_free(in);
	return(ret);
	}
#endif

int SSL_CTX_use_PrivateKey_ASN1(int type, SSL_CTX *ctx, const unsigned char *d,
	     long len)
	{
	int ret;
	const unsigned char *p;
	EVP_PKEY *pkey;

	p=d;
	if ((pkey=d2i_PrivateKey(type,NULL,&p,(long)len)) == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_PrivateKey_ASN1, ERR_R_ASN1_LIB);
		return(0);
		}

	ret=SSL_CTX_use_PrivateKey(ctx,pkey);
	EVP_PKEY_free(pkey);
	return(ret);
	}


#ifndef OPENSSL_NO_STDIO
/* Read a file that contains our certificate in "PEM" format,
 * possibly followed by a sequence of CA certificates that should be
 * sent to the peer in the Certificate message.
 */
int SSL_CTX_use_certificate_chain_file(SSL_CTX *ctx, const char *file)
	{
	BIO *in;
	int ret=0;
	X509 *x=NULL;

	ERR_clear_error(); /* clear error stack for SSL_CTX_use_certificate() */

	in = BIO_new(BIO_s_file());
	if (in == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_certificate_chain_file, ERR_R_BUF_LIB);
		goto end;
		}

	if (BIO_read_filename(in,file) <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_certificate_chain_file, ERR_R_SYS_LIB);
		goto end;
		}

	x=PEM_read_bio_X509_AUX(in,NULL,ctx->default_passwd_callback,
				ctx->default_passwd_callback_userdata);
	if (x == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_certificate_chain_file, ERR_R_PEM_LIB);
		goto end;
		}

	ret = SSL_CTX_use_certificate(ctx, x);

	if (ERR_peek_error() != 0)
		ret = 0;  /* Key/certificate mismatch doesn't imply ret==0 ... */
	if (ret)
		{
		/* If we could set up our certificate, now proceed to
		 * the CA certificates.
		 */
		X509 *ca;
		int r;
		unsigned long err;

		SSL_CTX_clear_chain_certs(ctx);
		
		while ((ca = PEM_read_bio_X509(in, NULL,
					ctx->default_passwd_callback,
					ctx->default_passwd_callback_userdata))
			!= NULL)
			{
			r = SSL_CTX_add0_chain_cert(ctx, ca);
			if (!r) 
				{
				X509_free(ca);
				ret = 0;
				goto end;
				}
			/* Note that we must not free r if it was successfully
			 * added to the chain (while we must free the main
			 * certificate, since its reference count is increased
			 * by SSL_CTX_use_certificate). */
			}
		/* When the while loop ends, it's usually just EOF. */
		err = ERR_peek_last_error();
		if (ERR_GET_LIB(err) == ERR_LIB_PEM && ERR_GET_REASON(err) == PEM_R_NO_START_LINE)
			ERR_clear_error();
		else 
			ret = 0; /* some real error */
		}

end:
	if (x != NULL) X509_free(x);
	if (in != NULL) BIO_free(in);
	return(ret);
	}
#endif

#ifndef OPENSSL_NO_TLSEXT
/* authz_validate returns true iff authz is well formed, i.e. that it meets the
 * wire format as documented in the CERT_PKEY structure and that there are no
 * duplicate entries. */
static char authz_validate(const unsigned char *authz, size_t length)
	{
	unsigned char types_seen_bitmap[32];

	if (!authz)
		return 1;

	memset(types_seen_bitmap, 0, sizeof(types_seen_bitmap));

	for (;;)
		{
		unsigned char type, byte, bit;
		unsigned short len;

		if (!length)
			return 1;

		type = *(authz++);
		length--;

		byte = type / 8;
		bit = type & 7;
		if (types_seen_bitmap[byte] & (1 << bit))
			return 0;
		types_seen_bitmap[byte] |= (1 << bit);

		if (length < 2)
			return 0;
		len = ((unsigned short) authz[0]) << 8 |
		      ((unsigned short) authz[1]);
		authz += 2;
		length -= 2;

		if (length < len)
			return 0;

		authz += len;
		length -= len;
		}
	}

static const unsigned char *authz_find_data(const unsigned char *authz,
					    size_t authz_length,
					    unsigned char data_type,
					    size_t *data_length)
	{
	if (authz == NULL) return NULL;
	if (!authz_validate(authz, authz_length))
		{
		OPENSSL_PUT_ERROR(SSL, authz_find_data, SSL_R_INVALID_AUTHZ_DATA);
		return NULL;
		}

	for (;;)
		{
		unsigned char type;
		unsigned short len;
		if (!authz_length)
			return NULL;

		type = *(authz++);
		authz_length--;

		/* We've validated the authz data, so we don't have to
		 * check again that we have enough bytes left. */
		len = ((unsigned short) authz[0]) << 8 |
		      ((unsigned short) authz[1]);
		authz += 2;
		authz_length -= 2;
		if (type == data_type)
			{
			*data_length = len;
			return authz;
			}
		authz += len;
		authz_length -= len;
		}
	/* No match */
	return NULL;
	}

static int ssl_set_authz(CERT *c, unsigned char *authz, size_t authz_length)
	{
	CERT_PKEY *current_key = c->key;
	if (current_key == NULL)
		return 0;
	if (!authz_validate(authz, authz_length))
		{
		OPENSSL_PUT_ERROR(SSL, ssl_set_authz, SSL_R_INVALID_AUTHZ_DATA);
		return(0);
		}
	current_key->authz = OPENSSL_realloc(current_key->authz, authz_length);
	if (current_key->authz == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, ssl_set_authz, ERR_R_MALLOC_FAILURE);
		return 0;
		}
	current_key->authz_length = authz_length;
	memcpy(current_key->authz, authz, authz_length);
	return 1;
	}

int SSL_CTX_use_authz(SSL_CTX *ctx, unsigned char *authz,
		      size_t authz_length)
	{
	if (authz == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_authz, ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (!ssl_cert_inst(&ctx->cert))
		{
		OPENSSL_PUT_ERROR(SSL, SSL_CTX_use_authz, ERR_R_MALLOC_FAILURE);
		return 0;
		}
	return ssl_set_authz(ctx->cert, authz, authz_length);
	}

int SSL_use_authz(SSL *ssl, unsigned char *authz, size_t authz_length)
	{
	if (authz == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_authz, ERR_R_PASSED_NULL_PARAMETER);
		return 0;
		}
	if (!ssl_cert_inst(&ssl->cert))
		{
		OPENSSL_PUT_ERROR(SSL, SSL_use_authz, ERR_R_MALLOC_FAILURE);
		return 0;
		}
	return ssl_set_authz(ssl->cert, authz, authz_length);
	}

const unsigned char *SSL_CTX_get_authz_data(SSL_CTX *ctx, unsigned char type,
					    size_t *data_length)
	{
	CERT_PKEY *current_key;

	if (ctx->cert == NULL)
		return NULL;
	current_key = ctx->cert->key;
	if (current_key->authz == NULL)
		return NULL;
	return authz_find_data(current_key->authz,
		current_key->authz_length, type, data_length);
	}

#ifndef OPENSSL_NO_STDIO
/* read_authz returns a newly allocated buffer with authz data */
static unsigned char *read_authz(const char *file, size_t *authz_length)
	{
	BIO *authz_in = NULL;
	unsigned char *authz = NULL;
	/* Allow authzs up to 64KB. */
	static const size_t authz_limit = 65536;
	size_t read_length;
	unsigned char *ret = NULL;

	authz_in = BIO_new(BIO_s_file());
	if (authz_in == NULL)
		{
		OPENSSL_PUT_ERROR(SSL, read_authz, ERR_R_BUF_LIB);
		goto end;
		}

	if (BIO_read_filename(authz_in,file) <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, read_authz, ERR_R_SYS_LIB);
		goto end;
		}

	authz = OPENSSL_malloc(authz_limit);
	read_length = BIO_read(authz_in, authz, authz_limit);
	if (read_length == authz_limit || read_length <= 0)
		{
		OPENSSL_PUT_ERROR(SSL, read_authz, SSL_R_AUTHZ_DATA_TOO_LARGE);
		OPENSSL_free(authz);
		goto end;
		}
	*authz_length = read_length;
	ret = authz;
end:
	if (authz_in != NULL) BIO_free(authz_in);
	return ret;
	}

int SSL_CTX_use_authz_file(SSL_CTX *ctx, const char *file)
	{
	unsigned char *authz = NULL;
	size_t authz_length = 0;
	int ret;

	authz = read_authz(file, &authz_length);
	if (authz == NULL)
		return 0;

	ret = SSL_CTX_use_authz(ctx, authz, authz_length);
	/* SSL_CTX_use_authz makes a local copy of the authz. */
	OPENSSL_free(authz);
	return ret;
	}

int SSL_use_authz_file(SSL *ssl, const char *file)
	{
	unsigned char *authz = NULL;
	size_t authz_length = 0;
	int ret;

	authz = read_authz(file, &authz_length);
	if (authz == NULL)
		return 0;

	ret = SSL_use_authz(ssl, authz, authz_length);
	/* SSL_use_authz makes a local copy of the authz. */
	OPENSSL_free(authz);
	return ret;
	}
#endif /* OPENSSL_NO_STDIO */
#endif /* OPENSSL_NO_TLSEXT */
