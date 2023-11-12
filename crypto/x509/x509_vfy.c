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

#include <ctype.h>
#include <string.h>
#include <time.h>

#include <openssl/asn1.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/mem.h>
#include <openssl/obj.h>
#include <openssl/thread.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include "../internal.h"
#include "../x509v3/internal.h"
#include "internal.h"

static CRYPTO_EX_DATA_CLASS g_ex_data_class =
    CRYPTO_EX_DATA_CLASS_INIT_WITH_APP_DATA;

// CRL score values

// No unhandled critical extensions
#define CRL_SCORE_NOCRITICAL 0x100

// certificate is within CRL scope
#define CRL_SCORE_SCOPE 0x080

// CRL times valid
#define CRL_SCORE_TIME 0x040

// Issuer name matches certificate
#define CRL_SCORE_ISSUER_NAME 0x020

// If this score or above CRL is probably valid
#define CRL_SCORE_VALID \
  (CRL_SCORE_NOCRITICAL | CRL_SCORE_TIME | CRL_SCORE_SCOPE)

// CRL issuer is certificate issuer
#define CRL_SCORE_ISSUER_CERT 0x018

// CRL issuer is on certificate path
#define CRL_SCORE_SAME_PATH 0x008

// CRL issuer matches CRL AKID
#define CRL_SCORE_AKID 0x004

static int null_callback(int ok, X509_STORE_CTX *e);
static int check_issued(X509_STORE_CTX *ctx, X509 *x, X509 *issuer);
static X509 *find_issuer(X509_STORE_CTX *ctx, STACK_OF(X509) *sk, X509 *x);
static int check_chain_extensions(X509_STORE_CTX *ctx);
static int check_name_constraints(X509_STORE_CTX *ctx);
static int check_id(X509_STORE_CTX *ctx);
static int check_trust(X509_STORE_CTX *ctx);
static int check_revocation(X509_STORE_CTX *ctx);
static int check_cert(X509_STORE_CTX *ctx);
static int check_policy(X509_STORE_CTX *ctx);

static int get_crl_score(X509_STORE_CTX *ctx, X509 **pissuer, X509_CRL *crl,
                         X509 *x);
static int get_crl(X509_STORE_CTX *ctx, X509_CRL **pcrl, X509 *x);
static int crl_akid_check(X509_STORE_CTX *ctx, X509_CRL *crl, X509 **pissuer,
                          int *pcrl_score);
static int crl_crldp_check(X509 *x, X509_CRL *crl, int crl_score);

static int internal_verify(X509_STORE_CTX *ctx);

static int null_callback(int ok, X509_STORE_CTX *e) { return ok; }

// cert_self_signed checks if |x| is self-signed. If |x| is valid, it returns
// one and sets |*out_is_self_signed| to the result. If |x| is invalid, it
// returns zero.
static int cert_self_signed(X509 *x, int *out_is_self_signed) {
  if (!x509v3_cache_extensions(x)) {
    return 0;
  }
  *out_is_self_signed = (x->ex_flags & EXFLAG_SS) != 0;
  return 1;
}

// Given a certificate try and find an exact match in the store

static X509 *lookup_cert_match(X509_STORE_CTX *ctx, X509 *x) {
  STACK_OF(X509) *certs;
  X509 *xtmp = NULL;
  size_t i;
  // Lookup all certs with matching subject name
  certs = ctx->lookup_certs(ctx, X509_get_subject_name(x));
  if (certs == NULL) {
    return NULL;
  }
  // Look for exact match
  for (i = 0; i < sk_X509_num(certs); i++) {
    xtmp = sk_X509_value(certs, i);
    if (!X509_cmp(xtmp, x)) {
      break;
    }
  }
  if (i < sk_X509_num(certs)) {
    X509_up_ref(xtmp);
  } else {
    xtmp = NULL;
  }
  sk_X509_pop_free(certs, X509_free);
  return xtmp;
}

int X509_verify_cert(X509_STORE_CTX *ctx) {
  X509 *x, *xtmp, *xtmp2, *chain_ss = NULL;
  int bad_chain = 0;
  X509_VERIFY_PARAM *param = ctx->param;
  int depth, i, ok = 0;
  int num, j, retry, trust;
  STACK_OF(X509) *sktmp = NULL;

  if (ctx->cert == NULL) {
    OPENSSL_PUT_ERROR(X509, X509_R_NO_CERT_SET_FOR_US_TO_VERIFY);
    ctx->error = X509_V_ERR_INVALID_CALL;
    return -1;
  }

  if (ctx->chain != NULL) {
    // This X509_STORE_CTX has already been used to verify a cert. We
    // cannot do another one.
    OPENSSL_PUT_ERROR(X509, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    ctx->error = X509_V_ERR_INVALID_CALL;
    return -1;
  }

  if (ctx->param->flags &
      (X509_V_FLAG_EXTENDED_CRL_SUPPORT | X509_V_FLAG_USE_DELTAS)) {
    // We do not support indirect or delta CRLs. The flags still exist for
    // compatibility with bindings libraries, but to ensure we do not
    // inadvertently skip a CRL check that the caller expects, fail closed.
    OPENSSL_PUT_ERROR(X509, ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED);
    ctx->error = X509_V_ERR_INVALID_CALL;
    return -1;
  }

  // first we make sure the chain we are going to build is present and that
  // the first entry is in place
  ctx->chain = sk_X509_new_null();
  if (ctx->chain == NULL || !sk_X509_push(ctx->chain, ctx->cert)) {
    ctx->error = X509_V_ERR_OUT_OF_MEM;
    goto end;
  }
  X509_up_ref(ctx->cert);
  ctx->last_untrusted = 1;

  // We use a temporary STACK so we can chop and hack at it.
  if (ctx->untrusted != NULL && (sktmp = sk_X509_dup(ctx->untrusted)) == NULL) {
    ctx->error = X509_V_ERR_OUT_OF_MEM;
    goto end;
  }

  num = (int)sk_X509_num(ctx->chain);
  x = sk_X509_value(ctx->chain, num - 1);
  depth = param->depth;

  for (;;) {
    // If we have enough, we break
    if (depth < num) {
      break;  // FIXME: If this happens, we should take
              // note of it and, if appropriate, use the
              // X509_V_ERR_CERT_CHAIN_TOO_LONG error code
              // later.
    }

    int is_self_signed;
    if (!cert_self_signed(x, &is_self_signed)) {
      ctx->error = X509_V_ERR_INVALID_EXTENSION;
      goto end;
    }

    // If we are self signed, we break
    if (is_self_signed) {
      break;
    }
    // If asked see if we can find issuer in trusted store first
    if (ctx->param->flags & X509_V_FLAG_TRUSTED_FIRST) {
      ok = ctx->get_issuer(&xtmp, ctx, x);
      if (ok < 0) {
        ctx->error = X509_V_ERR_STORE_LOOKUP;
        goto end;
      }
      // If successful for now free up cert so it will be picked up
      // again later.
      if (ok > 0) {
        X509_free(xtmp);
        break;
      }
    }

    // If we were passed a cert chain, use it first
    if (sktmp != NULL) {
      xtmp = find_issuer(ctx, sktmp, x);
      if (xtmp != NULL) {
        if (!sk_X509_push(ctx->chain, xtmp)) {
          ctx->error = X509_V_ERR_OUT_OF_MEM;
          ok = 0;
          goto end;
        }
        X509_up_ref(xtmp);
        (void)sk_X509_delete_ptr(sktmp, xtmp);
        ctx->last_untrusted++;
        x = xtmp;
        num++;
        // reparse the full chain for the next one
        continue;
      }
    }
    break;
  }

  // Remember how many untrusted certs we have
  j = num;
  // at this point, chain should contain a list of untrusted certificates.
  // We now need to add at least one trusted one, if possible, otherwise we
  // complain.

  do {
    // Examine last certificate in chain and see if it is self signed.
    i = (int)sk_X509_num(ctx->chain);
    x = sk_X509_value(ctx->chain, i - 1);

    int is_self_signed;
    if (!cert_self_signed(x, &is_self_signed)) {
      ctx->error = X509_V_ERR_INVALID_EXTENSION;
      goto end;
    }

    if (is_self_signed) {
      // we have a self signed certificate
      if (sk_X509_num(ctx->chain) == 1) {
        // We have a single self signed certificate: see if we can
        // find it in the store. We must have an exact match to avoid
        // possible impersonation.
        ok = ctx->get_issuer(&xtmp, ctx, x);
        if ((ok <= 0) || X509_cmp(x, xtmp)) {
          ctx->error = X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT;
          ctx->current_cert = x;
          ctx->error_depth = i - 1;
          if (ok == 1) {
            X509_free(xtmp);
          }
          bad_chain = 1;
          ok = ctx->verify_cb(0, ctx);
          if (!ok) {
            goto end;
          }
        } else {
          // We have a match: replace certificate with store
          // version so we get any trust settings.
          X509_free(x);
          x = xtmp;
          (void)sk_X509_set(ctx->chain, i - 1, x);
          ctx->last_untrusted = 0;
        }
      } else {
        // extract and save self signed certificate for later use
        chain_ss = sk_X509_pop(ctx->chain);
        ctx->last_untrusted--;
        num--;
        j--;
        x = sk_X509_value(ctx->chain, num - 1);
      }
    }
    // We now lookup certs from the certificate store
    for (;;) {
      // If we have enough, we break
      if (depth < num) {
        break;
      }
      if (!cert_self_signed(x, &is_self_signed)) {
        ctx->error = X509_V_ERR_INVALID_EXTENSION;
        goto end;
      }
      // If we are self signed, we break
      if (is_self_signed) {
        break;
      }
      ok = ctx->get_issuer(&xtmp, ctx, x);

      if (ok < 0) {
        ctx->error = X509_V_ERR_STORE_LOOKUP;
        goto end;
      }
      if (ok == 0) {
        break;
      }
      x = xtmp;
      if (!sk_X509_push(ctx->chain, x)) {
        X509_free(xtmp);
        ctx->error = X509_V_ERR_OUT_OF_MEM;
        ok = 0;
        goto end;
      }
      num++;
    }

    // we now have our chain, lets check it...
    trust = check_trust(ctx);

    // If explicitly rejected error
    if (trust == X509_TRUST_REJECTED) {
      ok = 0;
      goto end;
    }
    // If it's not explicitly trusted then check if there is an alternative
    // chain that could be used. We only do this if we haven't already
    // checked via TRUSTED_FIRST and the user hasn't switched off alternate
    // chain checking
    retry = 0;
    if (trust != X509_TRUST_TRUSTED &&
        !(ctx->param->flags & X509_V_FLAG_TRUSTED_FIRST) &&
        !(ctx->param->flags & X509_V_FLAG_NO_ALT_CHAINS)) {
      while (j-- > 1) {
        xtmp2 = sk_X509_value(ctx->chain, j - 1);
        ok = ctx->get_issuer(&xtmp, ctx, xtmp2);
        if (ok < 0) {
          goto end;
        }
        // Check if we found an alternate chain
        if (ok > 0) {
          // Free up the found cert we'll add it again later
          X509_free(xtmp);

          // Dump all the certs above this point - we've found an
          // alternate chain
          while (num > j) {
            xtmp = sk_X509_pop(ctx->chain);
            X509_free(xtmp);
            num--;
          }
          ctx->last_untrusted = (int)sk_X509_num(ctx->chain);
          retry = 1;
          break;
        }
      }
    }
  } while (retry);

  // If not explicitly trusted then indicate error unless it's a single
  // self signed certificate in which case we've indicated an error already
  // and set bad_chain == 1
  if (trust != X509_TRUST_TRUSTED && !bad_chain) {
    if ((chain_ss == NULL) || !ctx->check_issued(ctx, x, chain_ss)) {
      if (ctx->last_untrusted >= num) {
        ctx->error = X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY;
      } else {
        ctx->error = X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT;
      }
      ctx->current_cert = x;
    } else {
      sk_X509_push(ctx->chain, chain_ss);
      num++;
      ctx->last_untrusted = num;
      ctx->current_cert = chain_ss;
      ctx->error = X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN;
      chain_ss = NULL;
    }

    ctx->error_depth = num - 1;
    bad_chain = 1;
    ok = ctx->verify_cb(0, ctx);
    if (!ok) {
      goto end;
    }
  }

  // We have the chain complete: now we need to check its purpose
  ok = check_chain_extensions(ctx);

  if (!ok) {
    goto end;
  }

  ok = check_id(ctx);

  if (!ok) {
    goto end;
  }

  // Check revocation status: we do this after copying parameters because
  // they may be needed for CRL signature verification.
  ok = ctx->check_revocation(ctx);
  if (!ok) {
    goto end;
  }

  // At this point, we have a chain and need to verify it
  if (ctx->verify != NULL) {
    ok = ctx->verify(ctx);
  } else {
    ok = internal_verify(ctx);
  }
  if (!ok) {
    goto end;
  }

  // Check name constraints
  ok = check_name_constraints(ctx);
  if (!ok) {
    goto end;
  }

  // If we get this far, evaluate policies.
  if (!bad_chain) {
    ok = ctx->check_policy(ctx);
  }

end:
  if (sktmp != NULL) {
    sk_X509_free(sktmp);
  }
  if (chain_ss != NULL) {
    X509_free(chain_ss);
  }

  // Safety net, error returns must set ctx->error
  if (ok <= 0 && ctx->error == X509_V_OK) {
    ctx->error = X509_V_ERR_UNSPECIFIED;
  }
  return ok;
}

// Given a STACK_OF(X509) find the issuer of cert (if any)

static X509 *find_issuer(X509_STORE_CTX *ctx, STACK_OF(X509) *sk, X509 *x) {
  size_t i;
  X509 *issuer;
  for (i = 0; i < sk_X509_num(sk); i++) {
    issuer = sk_X509_value(sk, i);
    if (ctx->check_issued(ctx, x, issuer)) {
      return issuer;
    }
  }
  return NULL;
}

// Given a possible certificate and issuer check them

static int check_issued(X509_STORE_CTX *ctx, X509 *x, X509 *issuer) {
  int ret;
  ret = X509_check_issued(issuer, x);
  if (ret == X509_V_OK) {
    return 1;
  }
  // If we haven't asked for issuer errors don't set ctx
  if (!(ctx->param->flags & X509_V_FLAG_CB_ISSUER_CHECK)) {
    return 0;
  }

  ctx->error = ret;
  ctx->current_cert = x;
  ctx->current_issuer = issuer;
  return ctx->verify_cb(0, ctx);
}

// Alternative lookup method: look from a STACK stored in other_ctx

static int get_issuer_sk(X509 **issuer, X509_STORE_CTX *ctx, X509 *x) {
  *issuer = find_issuer(ctx, ctx->other_ctx, x);
  if (*issuer) {
    X509_up_ref(*issuer);
    return 1;
  } else {
    return 0;
  }
}

// Check a certificate chains extensions for consistency with the supplied
// purpose

static int check_chain_extensions(X509_STORE_CTX *ctx) {
  int ok = 0, plen = 0;
  int purpose = ctx->param->purpose;

  // Check all untrusted certificates
  for (int i = 0; i < ctx->last_untrusted; i++) {
    X509 *x = sk_X509_value(ctx->chain, i);
    if (!(ctx->param->flags & X509_V_FLAG_IGNORE_CRITICAL) &&
        (x->ex_flags & EXFLAG_CRITICAL)) {
      ctx->error = X509_V_ERR_UNHANDLED_CRITICAL_EXTENSION;
      ctx->error_depth = i;
      ctx->current_cert = x;
      ok = ctx->verify_cb(0, ctx);
      if (!ok) {
        goto end;
      }
    }

    int must_be_ca = i > 0;
    if (must_be_ca && !X509_check_ca(x)) {
      ctx->error = X509_V_ERR_INVALID_CA;
      ctx->error_depth = i;
      ctx->current_cert = x;
      ok = ctx->verify_cb(0, ctx);
      if (!ok) {
        goto end;
      }
    }
    if (ctx->param->purpose > 0 &&
        X509_check_purpose(x, purpose, must_be_ca) != 1) {
      ctx->error = X509_V_ERR_INVALID_PURPOSE;
      ctx->error_depth = i;
      ctx->current_cert = x;
      ok = ctx->verify_cb(0, ctx);
      if (!ok) {
        goto end;
      }
    }
    // Check pathlen if not self issued
    if (i > 1 && !(x->ex_flags & EXFLAG_SI) && x->ex_pathlen != -1 &&
        plen > x->ex_pathlen + 1) {
      ctx->error = X509_V_ERR_PATH_LENGTH_EXCEEDED;
      ctx->error_depth = i;
      ctx->current_cert = x;
      ok = ctx->verify_cb(0, ctx);
      if (!ok) {
        goto end;
      }
    }
    // Increment path length if not self issued
    if (!(x->ex_flags & EXFLAG_SI)) {
      plen++;
    }
  }
  ok = 1;
end:
  return ok;
}

static int reject_dns_name_in_common_name(X509 *x509) {
  const X509_NAME *name = X509_get_subject_name(x509);
  int i = -1;
  for (;;) {
    i = X509_NAME_get_index_by_NID(name, NID_commonName, i);
    if (i == -1) {
      return X509_V_OK;
    }

    const X509_NAME_ENTRY *entry = X509_NAME_get_entry(name, i);
    const ASN1_STRING *common_name = X509_NAME_ENTRY_get_data(entry);
    unsigned char *idval;
    int idlen = ASN1_STRING_to_UTF8(&idval, common_name);
    if (idlen < 0) {
      return X509_V_ERR_OUT_OF_MEM;
    }
    // Only process attributes that look like host names. Note it is
    // important that this check be mirrored in |X509_check_host|.
    int looks_like_dns = x509v3_looks_like_dns_name(idval, (size_t)idlen);
    OPENSSL_free(idval);
    if (looks_like_dns) {
      return X509_V_ERR_NAME_CONSTRAINTS_WITHOUT_SANS;
    }
  }
}

static int check_name_constraints(X509_STORE_CTX *ctx) {
  int i, j, rv;
  int has_name_constraints = 0;
  // Check name constraints for all certificates
  for (i = (int)sk_X509_num(ctx->chain) - 1; i >= 0; i--) {
    X509 *x = sk_X509_value(ctx->chain, i);
    // Ignore self issued certs unless last in chain
    if (i && (x->ex_flags & EXFLAG_SI)) {
      continue;
    }
    // Check against constraints for all certificates higher in chain
    // including trust anchor. Trust anchor not strictly speaking needed
    // but if it includes constraints it is to be assumed it expects them
    // to be obeyed.
    for (j = (int)sk_X509_num(ctx->chain) - 1; j > i; j--) {
      NAME_CONSTRAINTS *nc = sk_X509_value(ctx->chain, j)->nc;
      if (nc) {
        has_name_constraints = 1;
        rv = NAME_CONSTRAINTS_check(x, nc);
        switch (rv) {
          case X509_V_OK:
            continue;
          case X509_V_ERR_OUT_OF_MEM:
            ctx->error = rv;
            return 0;
          default:
            ctx->error = rv;
            ctx->error_depth = i;
            ctx->current_cert = x;
            if (!ctx->verify_cb(0, ctx)) {
              return 0;
            }
            break;
        }
      }
    }
  }

  // Name constraints do not match against the common name, but
  // |X509_check_host| still implements the legacy behavior where, on
  // certificates lacking a SAN list, DNS-like names in the common name are
  // checked instead.
  //
  // While we could apply the name constraints to the common name, name
  // constraints are rare enough that can hold such certificates to a higher
  // standard. Note this does not make "DNS-like" heuristic failures any
  // worse. A decorative common-name misidentified as a DNS name would fail
  // the name constraint anyway.
  X509 *leaf = sk_X509_value(ctx->chain, 0);
  if (has_name_constraints && leaf->altname == NULL) {
    rv = reject_dns_name_in_common_name(leaf);
    switch (rv) {
      case X509_V_OK:
        break;
      case X509_V_ERR_OUT_OF_MEM:
        ctx->error = rv;
        return 0;
      default:
        ctx->error = rv;
        ctx->error_depth = i;
        ctx->current_cert = leaf;
        if (!ctx->verify_cb(0, ctx)) {
          return 0;
        }
        break;
    }
  }

  return 1;
}

static int check_id_error(X509_STORE_CTX *ctx, int errcode) {
  ctx->error = errcode;
  ctx->current_cert = ctx->cert;
  ctx->error_depth = 0;
  return ctx->verify_cb(0, ctx);
}

static int check_hosts(X509 *x, X509_VERIFY_PARAM *param) {
  size_t i;
  size_t n = sk_OPENSSL_STRING_num(param->hosts);
  char *name;

  if (param->peername != NULL) {
    OPENSSL_free(param->peername);
    param->peername = NULL;
  }
  for (i = 0; i < n; ++i) {
    name = sk_OPENSSL_STRING_value(param->hosts, i);
    if (X509_check_host(x, name, strlen(name), param->hostflags,
                        &param->peername) > 0) {
      return 1;
    }
  }
  return n == 0;
}

static int check_id(X509_STORE_CTX *ctx) {
  X509_VERIFY_PARAM *vpm = ctx->param;
  X509 *x = ctx->cert;
  if (vpm->poison) {
    if (!check_id_error(ctx, X509_V_ERR_INVALID_CALL)) {
      return 0;
    }
  }
  if (vpm->hosts && check_hosts(x, vpm) <= 0) {
    if (!check_id_error(ctx, X509_V_ERR_HOSTNAME_MISMATCH)) {
      return 0;
    }
  }
  if (vpm->email && X509_check_email(x, vpm->email, vpm->emaillen, 0) <= 0) {
    if (!check_id_error(ctx, X509_V_ERR_EMAIL_MISMATCH)) {
      return 0;
    }
  }
  if (vpm->ip && X509_check_ip(x, vpm->ip, vpm->iplen, 0) <= 0) {
    if (!check_id_error(ctx, X509_V_ERR_IP_ADDRESS_MISMATCH)) {
      return 0;
    }
  }
  return 1;
}

static int check_trust(X509_STORE_CTX *ctx) {
  int ok;
  X509 *x = NULL;
  // Check all trusted certificates in chain
  for (size_t i = ctx->last_untrusted; i < sk_X509_num(ctx->chain); i++) {
    x = sk_X509_value(ctx->chain, i);
    ok = X509_check_trust(x, ctx->param->trust, 0);
    // If explicitly trusted return trusted
    if (ok == X509_TRUST_TRUSTED) {
      return X509_TRUST_TRUSTED;
    }
    // If explicitly rejected notify callback and reject if not
    // overridden.
    if (ok == X509_TRUST_REJECTED) {
      ctx->error_depth = (int)i;
      ctx->current_cert = x;
      ctx->error = X509_V_ERR_CERT_REJECTED;
      ok = ctx->verify_cb(0, ctx);
      if (!ok) {
        return X509_TRUST_REJECTED;
      }
    }
  }
  // If we accept partial chains and have at least one trusted certificate
  // return success.
  if (ctx->param->flags & X509_V_FLAG_PARTIAL_CHAIN) {
    X509 *mx;
    if (ctx->last_untrusted < (int)sk_X509_num(ctx->chain)) {
      return X509_TRUST_TRUSTED;
    }
    x = sk_X509_value(ctx->chain, 0);
    mx = lookup_cert_match(ctx, x);
    if (mx) {
      (void)sk_X509_set(ctx->chain, 0, mx);
      X509_free(x);
      ctx->last_untrusted = 0;
      return X509_TRUST_TRUSTED;
    }
  }

  // If no trusted certs in chain at all return untrusted and allow
  // standard (no issuer cert) etc errors to be indicated.
  return X509_TRUST_UNTRUSTED;
}

static int check_revocation(X509_STORE_CTX *ctx) {
  if (!(ctx->param->flags & X509_V_FLAG_CRL_CHECK)) {
    return 1;
  }
  int last;
  if (ctx->param->flags & X509_V_FLAG_CRL_CHECK_ALL) {
    last = (int)sk_X509_num(ctx->chain) - 1;
  } else {
    last = 0;
  }
  for (int i = 0; i <= last; i++) {
    ctx->error_depth = i;
    int ok = check_cert(ctx);
    if (!ok) {
      return ok;
    }
  }
  return 1;
}

static int check_cert(X509_STORE_CTX *ctx) {
  X509_CRL *crl = NULL;
  int ok = 0, cnum = ctx->error_depth;
  X509 *x = sk_X509_value(ctx->chain, cnum);
  ctx->current_cert = x;
  ctx->current_issuer = NULL;
  ctx->current_crl_score = 0;

  // Try to retrieve relevant CRL
  if (ctx->get_crl) {
    ok = ctx->get_crl(ctx, &crl, x);
  } else {
    ok = get_crl(ctx, &crl, x);
  }
  // If error looking up CRL, nothing we can do except notify callback
  if (!ok) {
    ctx->error = X509_V_ERR_UNABLE_TO_GET_CRL;
    ok = ctx->verify_cb(0, ctx);
    goto err;
  }
  ctx->current_crl = crl;
  ok = ctx->check_crl(ctx, crl);
  if (!ok) {
    goto err;
  }

  ok = ctx->cert_crl(ctx, crl, x);
  if (!ok) {
    goto err;
  }

err:
  X509_CRL_free(crl);
  ctx->current_crl = NULL;
  return ok;
}

// Check CRL times against values in X509_STORE_CTX
static int check_crl_time(X509_STORE_CTX *ctx, X509_CRL *crl, int notify) {
  if (ctx->param->flags & X509_V_FLAG_NO_CHECK_TIME) {
    return 1;
  }

  if (notify) {
    ctx->current_crl = crl;
  }
  int64_t ptime;
  if (ctx->param->flags & X509_V_FLAG_USE_CHECK_TIME) {
    ptime = ctx->param->check_time;
  } else {
    ptime = time(NULL);
  }

  int i = X509_cmp_time_posix(X509_CRL_get0_lastUpdate(crl), ptime);
  if (i == 0) {
    if (!notify) {
      return 0;
    }
    ctx->error = X509_V_ERR_ERROR_IN_CRL_LAST_UPDATE_FIELD;
    if (!ctx->verify_cb(0, ctx)) {
      return 0;
    }
  }

  if (i > 0) {
    if (!notify) {
      return 0;
    }
    ctx->error = X509_V_ERR_CRL_NOT_YET_VALID;
    if (!ctx->verify_cb(0, ctx)) {
      return 0;
    }
  }

  if (X509_CRL_get0_nextUpdate(crl)) {
    i = X509_cmp_time_posix(X509_CRL_get0_nextUpdate(crl), ptime);

    if (i == 0) {
      if (!notify) {
        return 0;
      }
      ctx->error = X509_V_ERR_ERROR_IN_CRL_NEXT_UPDATE_FIELD;
      if (!ctx->verify_cb(0, ctx)) {
        return 0;
      }
    }
    if (i < 0) {
      if (!notify) {
        return 0;
      }
      ctx->error = X509_V_ERR_CRL_HAS_EXPIRED;
      if (!ctx->verify_cb(0, ctx)) {
        return 0;
      }
    }
  }

  if (notify) {
    ctx->current_crl = NULL;
  }

  return 1;
}

static int get_crl_sk(X509_STORE_CTX *ctx, X509_CRL **pcrl, X509 **pissuer,
                      int *pscore, STACK_OF(X509_CRL) *crls) {
  int crl_score, best_score = *pscore;
  X509 *x = ctx->current_cert;
  X509_CRL *best_crl = NULL;
  X509 *crl_issuer = NULL, *best_crl_issuer = NULL;

  for (size_t i = 0; i < sk_X509_CRL_num(crls); i++) {
    X509_CRL *crl = sk_X509_CRL_value(crls, i);
    crl_score = get_crl_score(ctx, &crl_issuer, crl, x);
    if (crl_score < best_score || crl_score == 0) {
      continue;
    }
    // If current CRL is equivalent use it if it is newer
    if (crl_score == best_score && best_crl != NULL) {
      int day, sec;
      if (ASN1_TIME_diff(&day, &sec, X509_CRL_get0_lastUpdate(best_crl),
                         X509_CRL_get0_lastUpdate(crl)) == 0) {
        continue;
      }
      // ASN1_TIME_diff never returns inconsistent signs for |day|
      // and |sec|.
      if (day <= 0 && sec <= 0) {
        continue;
      }
    }
    best_crl = crl;
    best_crl_issuer = crl_issuer;
    best_score = crl_score;
  }

  if (best_crl) {
    if (*pcrl) {
      X509_CRL_free(*pcrl);
    }
    *pcrl = best_crl;
    *pissuer = best_crl_issuer;
    *pscore = best_score;
    X509_CRL_up_ref(best_crl);
  }

  if (best_score >= CRL_SCORE_VALID) {
    return 1;
  }

  return 0;
}

// For a given CRL return how suitable it is for the supplied certificate
// 'x'. The return value is a mask of several criteria. If the issuer is not
// the certificate issuer this is returned in *pissuer.
static int get_crl_score(X509_STORE_CTX *ctx, X509 **pissuer, X509_CRL *crl,
                         X509 *x) {
  int crl_score = 0;

  // First see if we can reject CRL straight away

  // Invalid IDP cannot be processed
  if (crl->idp_flags & IDP_INVALID) {
    return 0;
  }
  // Reason codes and indirect CRLs are not supported.
  if (crl->idp_flags & (IDP_INDIRECT | IDP_REASONS)) {
    return 0;
  }
  // We do not support indirect CRLs, so the issuer names must match.
  if (X509_NAME_cmp(X509_get_issuer_name(x), X509_CRL_get_issuer(crl))) {
    return 0;
  }
  crl_score |= CRL_SCORE_ISSUER_NAME;

  if (!(crl->flags & EXFLAG_CRITICAL)) {
    crl_score |= CRL_SCORE_NOCRITICAL;
  }

  // Check expiry
  if (check_crl_time(ctx, crl, 0)) {
    crl_score |= CRL_SCORE_TIME;
  }

  // Check authority key ID and locate certificate issuer
  if (!crl_akid_check(ctx, crl, pissuer, &crl_score)) {
    // If we can't locate certificate issuer at this point forget it
    return 0;
  }

  // Check cert for matching CRL distribution points
  if (crl_crldp_check(x, crl, crl_score)) {
    crl_score |= CRL_SCORE_SCOPE;
  }

  return crl_score;
}

static int crl_akid_check(X509_STORE_CTX *ctx, X509_CRL *crl, X509 **pissuer,
                          int *pcrl_score) {
  X509 *crl_issuer = NULL;
  X509_NAME *cnm = X509_CRL_get_issuer(crl);
  int cidx = ctx->error_depth;

  if ((size_t)cidx != sk_X509_num(ctx->chain) - 1) {
    cidx++;
  }

  crl_issuer = sk_X509_value(ctx->chain, cidx);

  if (X509_check_akid(crl_issuer, crl->akid) == X509_V_OK) {
    *pcrl_score |= CRL_SCORE_AKID | CRL_SCORE_ISSUER_CERT;
    *pissuer = crl_issuer;
    return 1;
  }

  for (cidx++; cidx < (int)sk_X509_num(ctx->chain); cidx++) {
    crl_issuer = sk_X509_value(ctx->chain, cidx);
    if (X509_NAME_cmp(X509_get_subject_name(crl_issuer), cnm)) {
      continue;
    }
    if (X509_check_akid(crl_issuer, crl->akid) == X509_V_OK) {
      *pcrl_score |= CRL_SCORE_AKID | CRL_SCORE_SAME_PATH;
      *pissuer = crl_issuer;
      return 1;
    }
  }

  return 0;
}

// Check for match between two dist point names: three separate cases. 1.
// Both are relative names and compare X509_NAME types. 2. One full, one
// relative. Compare X509_NAME to GENERAL_NAMES. 3. Both are full names and
// compare two GENERAL_NAMES. 4. One is NULL: automatic match.
static int idp_check_dp(DIST_POINT_NAME *a, DIST_POINT_NAME *b) {
  X509_NAME *nm = NULL;
  GENERAL_NAMES *gens = NULL;
  GENERAL_NAME *gena, *genb;
  size_t i, j;
  if (!a || !b) {
    return 1;
  }
  if (a->type == 1) {
    if (!a->dpname) {
      return 0;
    }
    // Case 1: two X509_NAME
    if (b->type == 1) {
      if (!b->dpname) {
        return 0;
      }
      if (!X509_NAME_cmp(a->dpname, b->dpname)) {
        return 1;
      } else {
        return 0;
      }
    }
    // Case 2: set name and GENERAL_NAMES appropriately
    nm = a->dpname;
    gens = b->name.fullname;
  } else if (b->type == 1) {
    if (!b->dpname) {
      return 0;
    }
    // Case 2: set name and GENERAL_NAMES appropriately
    gens = a->name.fullname;
    nm = b->dpname;
  }

  // Handle case 2 with one GENERAL_NAMES and one X509_NAME
  if (nm) {
    for (i = 0; i < sk_GENERAL_NAME_num(gens); i++) {
      gena = sk_GENERAL_NAME_value(gens, i);
      if (gena->type != GEN_DIRNAME) {
        continue;
      }
      if (!X509_NAME_cmp(nm, gena->d.directoryName)) {
        return 1;
      }
    }
    return 0;
  }

  // Else case 3: two GENERAL_NAMES

  for (i = 0; i < sk_GENERAL_NAME_num(a->name.fullname); i++) {
    gena = sk_GENERAL_NAME_value(a->name.fullname, i);
    for (j = 0; j < sk_GENERAL_NAME_num(b->name.fullname); j++) {
      genb = sk_GENERAL_NAME_value(b->name.fullname, j);
      if (!GENERAL_NAME_cmp(gena, genb)) {
        return 1;
      }
    }
  }

  return 0;
}

// Check CRLDP and IDP
static int crl_crldp_check(X509 *x, X509_CRL *crl, int crl_score) {
  if (crl->idp_flags & IDP_ONLYATTR) {
    return 0;
  }
  if (x->ex_flags & EXFLAG_CA) {
    if (crl->idp_flags & IDP_ONLYUSER) {
      return 0;
    }
  } else {
    if (crl->idp_flags & IDP_ONLYCA) {
      return 0;
    }
  }
  for (size_t i = 0; i < sk_DIST_POINT_num(x->crldp); i++) {
    DIST_POINT *dp = sk_DIST_POINT_value(x->crldp, i);
    // Skip distribution points with a reasons field or a CRL issuer:
    //
    // We do not support CRLs partitioned by reason code. RFC 5280 requires CAs
    // include at least one DistributionPoint that covers all reasons.
    //
    // We also do not support indirect CRLs, and a CRL issuer can only match
    // indirect CRLs (RFC 5280, section 6.3.3, step b.1).
    // support.
    if (dp->reasons != NULL && dp->CRLissuer != NULL &&
        (!crl->idp || idp_check_dp(dp->distpoint, crl->idp->distpoint))) {
      return 1;
    }
  }

  // If the CRL does not specify an issuing distribution point, allow it to
  // match anything.
  //
  // TODO(davidben): Does this match RFC 5280? It's hard to follow because RFC
  // 5280 starts from distribution points, while this starts from CRLs.
  return !crl->idp || !crl->idp->distpoint;
}

// Retrieve CRL corresponding to current certificate.
static int get_crl(X509_STORE_CTX *ctx, X509_CRL **pcrl, X509 *x) {
  int ok;
  X509 *issuer = NULL;
  int crl_score = 0;
  X509_CRL *crl = NULL;
  STACK_OF(X509_CRL) *skcrl;
  X509_NAME *nm = X509_get_issuer_name(x);
  ok = get_crl_sk(ctx, &crl, &issuer, &crl_score, ctx->crls);
  if (ok) {
    goto done;
  }

  // Lookup CRLs from store
  skcrl = ctx->lookup_crls(ctx, nm);

  // If no CRLs found and a near match from get_crl_sk use that
  if (!skcrl && crl) {
    goto done;
  }

  get_crl_sk(ctx, &crl, &issuer, &crl_score, skcrl);

  sk_X509_CRL_pop_free(skcrl, X509_CRL_free);

done:

  // If we got any kind of CRL use it and return success
  if (crl) {
    ctx->current_issuer = issuer;
    ctx->current_crl_score = crl_score;
    *pcrl = crl;
    return 1;
  }

  return 0;
}

// Check CRL validity
static int check_crl(X509_STORE_CTX *ctx, X509_CRL *crl) {
  X509 *issuer = NULL;
  EVP_PKEY *ikey = NULL;
  int ok = 0;
  int cnum = ctx->error_depth;
  int chnum = (int)sk_X509_num(ctx->chain) - 1;
  // if we have an alternative CRL issuer cert use that
  if (ctx->current_issuer) {
    issuer = ctx->current_issuer;
  }

  // Else find CRL issuer: if not last certificate then issuer is next
  // certificate in chain.
  else if (cnum < chnum) {
    issuer = sk_X509_value(ctx->chain, cnum + 1);
  } else {
    issuer = sk_X509_value(ctx->chain, chnum);
    // If not self signed, can't check signature
    if (!ctx->check_issued(ctx, issuer, issuer)) {
      ctx->error = X509_V_ERR_UNABLE_TO_GET_CRL_ISSUER;
      ok = ctx->verify_cb(0, ctx);
      if (!ok) {
        goto err;
      }
    }
  }

  if (issuer) {
    // Check for cRLSign bit if keyUsage present
    if ((issuer->ex_flags & EXFLAG_KUSAGE) &&
        !(issuer->ex_kusage & KU_CRL_SIGN)) {
      ctx->error = X509_V_ERR_KEYUSAGE_NO_CRL_SIGN;
      ok = ctx->verify_cb(0, ctx);
      if (!ok) {
        goto err;
      }
    }

    if (!(ctx->current_crl_score & CRL_SCORE_SCOPE)) {
      ctx->error = X509_V_ERR_DIFFERENT_CRL_SCOPE;
      ok = ctx->verify_cb(0, ctx);
      if (!ok) {
        goto err;
      }
    }

    if (crl->idp_flags & IDP_INVALID) {
      ctx->error = X509_V_ERR_INVALID_EXTENSION;
      ok = ctx->verify_cb(0, ctx);
      if (!ok) {
        goto err;
      }
    }

    if (!(ctx->current_crl_score & CRL_SCORE_TIME)) {
      ok = check_crl_time(ctx, crl, 1);
      if (!ok) {
        goto err;
      }
    }

    // Attempt to get issuer certificate public key
    ikey = X509_get_pubkey(issuer);

    if (!ikey) {
      ctx->error = X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY;
      ok = ctx->verify_cb(0, ctx);
      if (!ok) {
        goto err;
      }
    } else {
      // Verify CRL signature
      if (X509_CRL_verify(crl, ikey) <= 0) {
        ctx->error = X509_V_ERR_CRL_SIGNATURE_FAILURE;
        ok = ctx->verify_cb(0, ctx);
        if (!ok) {
          goto err;
        }
      }
    }
  }

  ok = 1;

err:
  EVP_PKEY_free(ikey);
  return ok;
}

// Check certificate against CRL
static int cert_crl(X509_STORE_CTX *ctx, X509_CRL *crl, X509 *x) {
  int ok;
  X509_REVOKED *rev;
  // The rules changed for this... previously if a CRL contained unhandled
  // critical extensions it could still be used to indicate a certificate
  // was revoked. This has since been changed since critical extension can
  // change the meaning of CRL entries.
  if (!(ctx->param->flags & X509_V_FLAG_IGNORE_CRITICAL) &&
      (crl->flags & EXFLAG_CRITICAL)) {
    ctx->error = X509_V_ERR_UNHANDLED_CRITICAL_CRL_EXTENSION;
    ok = ctx->verify_cb(0, ctx);
    if (!ok) {
      return 0;
    }
  }
  // Look for serial number of certificate in CRL.
  if (X509_CRL_get0_by_cert(crl, &rev, x)) {
    ctx->error = X509_V_ERR_CERT_REVOKED;
    ok = ctx->verify_cb(0, ctx);
    if (!ok) {
      return 0;
    }
  }

  return 1;
}

static int check_policy(X509_STORE_CTX *ctx) {
  X509 *current_cert = NULL;
  int ret = X509_policy_check(ctx->chain, ctx->param->policies,
                              ctx->param->flags, &current_cert);
  if (ret != X509_V_OK) {
    ctx->current_cert = current_cert;
    ctx->error = ret;
    if (ret == X509_V_ERR_OUT_OF_MEM) {
      return 0;
    }
    return ctx->verify_cb(0, ctx);
  }

  if (ctx->param->flags & X509_V_FLAG_NOTIFY_POLICY) {
    ctx->current_cert = NULL;
    // Verification errors need to be "sticky", a callback may have allowed
    // an SSL handshake to continue despite an error, and we must then
    // remain in an error state.  Therefore, we MUST NOT clear earlier
    // verification errors by setting the error to X509_V_OK.
    if (!ctx->verify_cb(2, ctx)) {
      return 0;
    }
  }

  return 1;
}

static int check_cert_time(X509_STORE_CTX *ctx, X509 *x) {
  if (ctx->param->flags & X509_V_FLAG_NO_CHECK_TIME) {
    return 1;
  }

  int64_t ptime;
  if (ctx->param->flags & X509_V_FLAG_USE_CHECK_TIME) {
    ptime = ctx->param->check_time;
  } else {
    ptime = time(NULL);
  }

  int i = X509_cmp_time_posix(X509_get_notBefore(x), ptime);
  if (i == 0) {
    ctx->error = X509_V_ERR_ERROR_IN_CERT_NOT_BEFORE_FIELD;
    ctx->current_cert = x;
    if (!ctx->verify_cb(0, ctx)) {
      return 0;
    }
  }

  if (i > 0) {
    ctx->error = X509_V_ERR_CERT_NOT_YET_VALID;
    ctx->current_cert = x;
    if (!ctx->verify_cb(0, ctx)) {
      return 0;
    }
  }

  i = X509_cmp_time_posix(X509_get_notAfter(x), ptime);
  if (i == 0) {
    ctx->error = X509_V_ERR_ERROR_IN_CERT_NOT_AFTER_FIELD;
    ctx->current_cert = x;
    if (!ctx->verify_cb(0, ctx)) {
      return 0;
    }
  }

  if (i < 0) {
    ctx->error = X509_V_ERR_CERT_HAS_EXPIRED;
    ctx->current_cert = x;
    if (!ctx->verify_cb(0, ctx)) {
      return 0;
    }
  }

  return 1;
}

static int internal_verify(X509_STORE_CTX *ctx) {
  int ok = 0;
  X509 *xs, *xi;
  EVP_PKEY *pkey = NULL;

  int n = (int)sk_X509_num(ctx->chain);
  ctx->error_depth = n - 1;
  n--;
  xi = sk_X509_value(ctx->chain, n);

  if (ctx->check_issued(ctx, xi, xi)) {
    xs = xi;
  } else {
    if (ctx->param->flags & X509_V_FLAG_PARTIAL_CHAIN) {
      xs = xi;
      goto check_cert;
    }
    if (n <= 0) {
      ctx->error = X509_V_ERR_UNABLE_TO_VERIFY_LEAF_SIGNATURE;
      ctx->current_cert = xi;
      ok = ctx->verify_cb(0, ctx);
      goto end;
    } else {
      n--;
      ctx->error_depth = n;
      xs = sk_X509_value(ctx->chain, n);
    }
  }

  //      ctx->error=0;  not needed
  while (n >= 0) {
    ctx->error_depth = n;

    // Skip signature check for self signed certificates unless
    // explicitly asked for. It doesn't add any security and just wastes
    // time.
    if (xs != xi || (ctx->param->flags & X509_V_FLAG_CHECK_SS_SIGNATURE)) {
      if ((pkey = X509_get_pubkey(xi)) == NULL) {
        ctx->error = X509_V_ERR_UNABLE_TO_DECODE_ISSUER_PUBLIC_KEY;
        ctx->current_cert = xi;
        ok = ctx->verify_cb(0, ctx);
        if (!ok) {
          goto end;
        }
      } else if (X509_verify(xs, pkey) <= 0) {
        ctx->error = X509_V_ERR_CERT_SIGNATURE_FAILURE;
        ctx->current_cert = xs;
        ok = ctx->verify_cb(0, ctx);
        if (!ok) {
          EVP_PKEY_free(pkey);
          goto end;
        }
      }
      EVP_PKEY_free(pkey);
      pkey = NULL;
    }

  check_cert:
    ok = check_cert_time(ctx, xs);
    if (!ok) {
      goto end;
    }

    // The last error (if any) is still in the error value
    ctx->current_issuer = xi;
    ctx->current_cert = xs;
    ok = ctx->verify_cb(1, ctx);
    if (!ok) {
      goto end;
    }

    n--;
    if (n >= 0) {
      xi = xs;
      xs = sk_X509_value(ctx->chain, n);
    }
  }
  ok = 1;
end:
  return ok;
}

int X509_cmp_current_time(const ASN1_TIME *ctm) {
  return X509_cmp_time_posix(ctm, time(NULL));
}

int X509_cmp_time(const ASN1_TIME *ctm, const time_t *cmp_time) {
  int64_t compare_time = (cmp_time == NULL) ? time(NULL) : *cmp_time;
  return X509_cmp_time_posix(ctm, compare_time);
}

int X509_cmp_time_posix(const ASN1_TIME *ctm, int64_t cmp_time) {
  int64_t ctm_time;
  if (!ASN1_TIME_to_posix(ctm, &ctm_time)) {
    return 0;
  }
  // The return value 0 is reserved for errors.
  return (ctm_time - cmp_time <= 0) ? -1 : 1;
}

ASN1_TIME *X509_gmtime_adj(ASN1_TIME *s, long offset_sec) {
  return X509_time_adj(s, offset_sec, NULL);
}

ASN1_TIME *X509_time_adj(ASN1_TIME *s, long offset_sec, const time_t *in_tm) {
  return X509_time_adj_ex(s, 0, offset_sec, in_tm);
}

ASN1_TIME *X509_time_adj_ex(ASN1_TIME *s, int offset_day, long offset_sec,
                            const time_t *in_tm) {
  int64_t t = 0;

  if (in_tm) {
    t = *in_tm;
  } else {
    t = time(NULL);
  }

  return ASN1_TIME_adj(s, t, offset_day, offset_sec);
}

int X509_STORE_CTX_get_ex_new_index(long argl, void *argp,
                                    CRYPTO_EX_unused *unused,
                                    CRYPTO_EX_dup *dup_unused,
                                    CRYPTO_EX_free *free_func) {
  // This function is (usually) called only once, by
  // SSL_get_ex_data_X509_STORE_CTX_idx (ssl/ssl_cert.c).
  int index;
  if (!CRYPTO_get_ex_new_index(&g_ex_data_class, &index, argl, argp,
                               free_func)) {
    return -1;
  }
  return index;
}

int X509_STORE_CTX_set_ex_data(X509_STORE_CTX *ctx, int idx, void *data) {
  return CRYPTO_set_ex_data(&ctx->ex_data, idx, data);
}

void *X509_STORE_CTX_get_ex_data(X509_STORE_CTX *ctx, int idx) {
  return CRYPTO_get_ex_data(&ctx->ex_data, idx);
}

int X509_STORE_CTX_get_error(X509_STORE_CTX *ctx) { return ctx->error; }

void X509_STORE_CTX_set_error(X509_STORE_CTX *ctx, int err) {
  ctx->error = err;
}

int X509_STORE_CTX_get_error_depth(X509_STORE_CTX *ctx) {
  return ctx->error_depth;
}

X509 *X509_STORE_CTX_get_current_cert(X509_STORE_CTX *ctx) {
  return ctx->current_cert;
}

STACK_OF(X509) *X509_STORE_CTX_get_chain(X509_STORE_CTX *ctx) {
  return ctx->chain;
}

STACK_OF(X509) *X509_STORE_CTX_get0_chain(X509_STORE_CTX *ctx) {
  return ctx->chain;
}

STACK_OF(X509) *X509_STORE_CTX_get1_chain(X509_STORE_CTX *ctx) {
  if (!ctx->chain) {
    return NULL;
  }
  return X509_chain_up_ref(ctx->chain);
}

X509 *X509_STORE_CTX_get0_current_issuer(X509_STORE_CTX *ctx) {
  return ctx->current_issuer;
}

X509_CRL *X509_STORE_CTX_get0_current_crl(X509_STORE_CTX *ctx) {
  return ctx->current_crl;
}

X509_STORE_CTX *X509_STORE_CTX_get0_parent_ctx(X509_STORE_CTX *ctx) {
  // In OpenSSL, an |X509_STORE_CTX| sometimes has a parent context during CRL
  // path validation for indirect CRLs. We require the CRL to be issued
  // somewhere along the certificate path, so this is always NULL.
  return NULL;
}

void X509_STORE_CTX_set_cert(X509_STORE_CTX *ctx, X509 *x) { ctx->cert = x; }

void X509_STORE_CTX_set_chain(X509_STORE_CTX *ctx, STACK_OF(X509) *sk) {
  ctx->untrusted = sk;
}

STACK_OF(X509) *X509_STORE_CTX_get0_untrusted(X509_STORE_CTX *ctx) {
  return ctx->untrusted;
}

void X509_STORE_CTX_set0_crls(X509_STORE_CTX *ctx, STACK_OF(X509_CRL) *sk) {
  ctx->crls = sk;
}

int X509_STORE_CTX_set_purpose(X509_STORE_CTX *ctx, int purpose) {
  return X509_STORE_CTX_purpose_inherit(ctx, 0, purpose, 0);
}

int X509_STORE_CTX_set_trust(X509_STORE_CTX *ctx, int trust) {
  return X509_STORE_CTX_purpose_inherit(ctx, 0, 0, trust);
}

// This function is used to set the X509_STORE_CTX purpose and trust values.
// This is intended to be used when another structure has its own trust and
// purpose values which (if set) will be inherited by the ctx. If they aren't
// set then we will usually have a default purpose in mind which should then
// be used to set the trust value. An example of this is SSL use: an SSL
// structure will have its own purpose and trust settings which the
// application can set: if they aren't set then we use the default of SSL
// client/server.

int X509_STORE_CTX_purpose_inherit(X509_STORE_CTX *ctx, int def_purpose,
                                   int purpose, int trust) {
  int idx;
  // If purpose not set use default
  if (!purpose) {
    purpose = def_purpose;
  }
  // If we have a purpose then check it is valid
  if (purpose) {
    X509_PURPOSE *ptmp;
    idx = X509_PURPOSE_get_by_id(purpose);
    if (idx == -1) {
      OPENSSL_PUT_ERROR(X509, X509_R_UNKNOWN_PURPOSE_ID);
      return 0;
    }
    ptmp = X509_PURPOSE_get0(idx);
    if (ptmp->trust == X509_TRUST_DEFAULT) {
      idx = X509_PURPOSE_get_by_id(def_purpose);
      if (idx == -1) {
        OPENSSL_PUT_ERROR(X509, X509_R_UNKNOWN_PURPOSE_ID);
        return 0;
      }
      ptmp = X509_PURPOSE_get0(idx);
    }
    // If trust not set then get from purpose default
    if (!trust) {
      trust = ptmp->trust;
    }
  }
  if (trust) {
    idx = X509_TRUST_get_by_id(trust);
    if (idx == -1) {
      OPENSSL_PUT_ERROR(X509, X509_R_UNKNOWN_TRUST_ID);
      return 0;
    }
  }

  if (purpose && !ctx->param->purpose) {
    ctx->param->purpose = purpose;
  }
  if (trust && !ctx->param->trust) {
    ctx->param->trust = trust;
  }
  return 1;
}

X509_STORE_CTX *X509_STORE_CTX_new(void) {
  X509_STORE_CTX *ctx;
  ctx = (X509_STORE_CTX *)OPENSSL_malloc(sizeof(X509_STORE_CTX));
  if (!ctx) {
    return NULL;
  }
  X509_STORE_CTX_zero(ctx);
  return ctx;
}

void X509_STORE_CTX_zero(X509_STORE_CTX *ctx) {
  OPENSSL_memset(ctx, 0, sizeof(X509_STORE_CTX));
}

void X509_STORE_CTX_free(X509_STORE_CTX *ctx) {
  if (ctx == NULL) {
    return;
  }
  X509_STORE_CTX_cleanup(ctx);
  OPENSSL_free(ctx);
}

int X509_STORE_CTX_init(X509_STORE_CTX *ctx, X509_STORE *store, X509 *x509,
                        STACK_OF(X509) *chain) {
  X509_STORE_CTX_zero(ctx);
  ctx->ctx = store;
  ctx->cert = x509;
  ctx->untrusted = chain;

  CRYPTO_new_ex_data(&ctx->ex_data);

  if (store == NULL) {
    OPENSSL_PUT_ERROR(X509, ERR_R_PASSED_NULL_PARAMETER);
    goto err;
  }

  ctx->param = X509_VERIFY_PARAM_new();
  if (!ctx->param) {
    goto err;
  }

  // Inherit callbacks and flags from X509_STORE.

  ctx->verify_cb = store->verify_cb;
  ctx->cleanup = store->cleanup;

  if (!X509_VERIFY_PARAM_inherit(ctx->param, store->param) ||
      !X509_VERIFY_PARAM_inherit(ctx->param,
                                 X509_VERIFY_PARAM_lookup("default"))) {
    goto err;
  }

  if (store->check_issued) {
    ctx->check_issued = store->check_issued;
  } else {
    ctx->check_issued = check_issued;
  }

  if (store->get_issuer) {
    ctx->get_issuer = store->get_issuer;
  } else {
    ctx->get_issuer = X509_STORE_CTX_get1_issuer;
  }

  if (store->verify_cb) {
    ctx->verify_cb = store->verify_cb;
  } else {
    ctx->verify_cb = null_callback;
  }

  if (store->verify) {
    ctx->verify = store->verify;
  } else {
    ctx->verify = internal_verify;
  }

  if (store->check_revocation) {
    ctx->check_revocation = store->check_revocation;
  } else {
    ctx->check_revocation = check_revocation;
  }

  if (store->get_crl) {
    ctx->get_crl = store->get_crl;
  } else {
    ctx->get_crl = NULL;
  }

  if (store->check_crl) {
    ctx->check_crl = store->check_crl;
  } else {
    ctx->check_crl = check_crl;
  }

  if (store->cert_crl) {
    ctx->cert_crl = store->cert_crl;
  } else {
    ctx->cert_crl = cert_crl;
  }

  if (store->lookup_certs) {
    ctx->lookup_certs = store->lookup_certs;
  } else {
    ctx->lookup_certs = X509_STORE_get1_certs;
  }

  if (store->lookup_crls) {
    ctx->lookup_crls = store->lookup_crls;
  } else {
    ctx->lookup_crls = X509_STORE_get1_crls;
  }

  ctx->check_policy = check_policy;

  return 1;

err:
  CRYPTO_free_ex_data(&g_ex_data_class, ctx, &ctx->ex_data);
  if (ctx->param != NULL) {
    X509_VERIFY_PARAM_free(ctx->param);
  }

  OPENSSL_memset(ctx, 0, sizeof(X509_STORE_CTX));
  return 0;
}

// Set alternative lookup method: just a STACK of trusted certificates. This
// avoids X509_STORE nastiness where it isn't needed.

void X509_STORE_CTX_set0_trusted_stack(X509_STORE_CTX *ctx,
                                       STACK_OF(X509) *sk) {
  ctx->other_ctx = sk;
  ctx->get_issuer = get_issuer_sk;
}

void X509_STORE_CTX_trusted_stack(X509_STORE_CTX *ctx, STACK_OF(X509) *sk) {
  X509_STORE_CTX_set0_trusted_stack(ctx, sk);
}

void X509_STORE_CTX_cleanup(X509_STORE_CTX *ctx) {
  // We need to be idempotent because, unfortunately, |X509_STORE_CTX_free|
  // also calls this function.
  if (ctx->cleanup != NULL) {
    ctx->cleanup(ctx);
    ctx->cleanup = NULL;
  }
  X509_VERIFY_PARAM_free(ctx->param);
  ctx->param = NULL;
  sk_X509_pop_free(ctx->chain, X509_free);
  ctx->chain = NULL;
  CRYPTO_free_ex_data(&g_ex_data_class, ctx, &(ctx->ex_data));
  OPENSSL_memset(&ctx->ex_data, 0, sizeof(CRYPTO_EX_DATA));
}

void X509_STORE_CTX_set_depth(X509_STORE_CTX *ctx, int depth) {
  X509_VERIFY_PARAM_set_depth(ctx->param, depth);
}

void X509_STORE_CTX_set_flags(X509_STORE_CTX *ctx, unsigned long flags) {
  X509_VERIFY_PARAM_set_flags(ctx->param, flags);
}

void X509_STORE_CTX_set_time_posix(X509_STORE_CTX *ctx, unsigned long flags,
                             int64_t t) {
  X509_VERIFY_PARAM_set_time_posix(ctx->param, t);
}

void X509_STORE_CTX_set_time(X509_STORE_CTX *ctx, unsigned long flags,
                             time_t t) {
  X509_STORE_CTX_set_time_posix(ctx, flags, t);
}

X509 *X509_STORE_CTX_get0_cert(X509_STORE_CTX *ctx) {
  return ctx->cert;
}

void X509_STORE_CTX_set_verify_cb(X509_STORE_CTX *ctx,
                                  int (*verify_cb)(int, X509_STORE_CTX *)) {
  ctx->verify_cb = verify_cb;
}

int X509_STORE_CTX_set_default(X509_STORE_CTX *ctx, const char *name) {
  const X509_VERIFY_PARAM *param;
  param = X509_VERIFY_PARAM_lookup(name);
  if (!param) {
    return 0;
  }
  return X509_VERIFY_PARAM_inherit(ctx->param, param);
}

X509_VERIFY_PARAM *X509_STORE_CTX_get0_param(X509_STORE_CTX *ctx) {
  return ctx->param;
}

void X509_STORE_CTX_set0_param(X509_STORE_CTX *ctx, X509_VERIFY_PARAM *param) {
  if (ctx->param) {
    X509_VERIFY_PARAM_free(ctx->param);
  }
  ctx->param = param;
}
