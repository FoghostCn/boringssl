/* Copyright (c) 2020, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

// make_policy_certs.go generates certificates for testing policy handling.
package main

import (
	"crypto/ecdsa"
	"crypto/rand"
	"crypto/x509"
	"crypto/x509/pkix"
	"encoding/asn1"
	"encoding/pem"
	"math/big"
	"os"
	"time"

	"golang.org/x/crypto/cryptobyte"
	cbasn1 "golang.org/x/crypto/cryptobyte/asn1"
)

var (
	// https://davidben.net/oid
	testOID1 = asn1.ObjectIdentifier([]int{1, 2, 840, 113554, 4, 1, 72585, 2, 1})
	testOID2 = asn1.ObjectIdentifier([]int{1, 2, 840, 113554, 4, 1, 72585, 2, 2})
	testOID3 = asn1.ObjectIdentifier([]int{1, 2, 840, 113554, 4, 1, 72585, 2, 3})
	testOID4 = asn1.ObjectIdentifier([]int{1, 2, 840, 113554, 4, 1, 72585, 2, 4})
	testOID5 = asn1.ObjectIdentifier([]int{1, 2, 840, 113554, 4, 1, 72585, 2, 5})

	// https://www.rfc-editor.org/rfc/rfc5280.html#section-4.2.1.4
	certificatePoliciesOID = asn1.ObjectIdentifier([]int{2, 5, 29, 32})
	anyPolicyOID           = asn1.ObjectIdentifier([]int{2, 5, 29, 32, 0})

	// https://www.rfc-editor.org/rfc/rfc5280.html#section-4.2.1.5
	policyMappingsOID = asn1.ObjectIdentifier([]int{2, 5, 29, 33})

	// https://www.rfc-editor.org/rfc/rfc5280.html#section-4.2.1.11
	policyConstraintsOID = asn1.ObjectIdentifier([]int{2, 5, 29, 36})
)

var leafKey, intermediateKey, rootKey *ecdsa.PrivateKey

func init() {
	leafKey = mustParseECDSAKey(leafKeyPEM)
	intermediateKey = mustParseECDSAKey(intermediateKeyPEM)
	rootKey = mustParseECDSAKey(rootKeyPEM)
}

type templateAndKey struct {
	template x509.Certificate
	key      *ecdsa.PrivateKey
}

func mustGenerateCertificate(path string, subject, issuer *templateAndKey) []byte {
	cert, err := x509.CreateCertificate(rand.Reader, &subject.template, &issuer.template, &subject.key.PublicKey, issuer.key)
	if err != nil {
		panic(err)
	}
	file, err := os.Create(path)
	if err != nil {
		panic(err)
	}
	defer file.Close()
	err = pem.Encode(file, &pem.Block{Type: "CERTIFICATE", Bytes: cert})
	if err != nil {
		panic(err)
	}
	return cert
}

func main() {
	notBefore, err := time.Parse(time.RFC3339, "2000-01-01T00:00:00Z")
	if err != nil {
		panic(err)
	}
	notAfter, err := time.Parse(time.RFC3339, "2100-01-01T00:00:00Z")
	if err != nil {
		panic(err)
	}

	root := templateAndKey{
		template: x509.Certificate{
			SerialNumber:          new(big.Int).SetInt64(1),
			Subject:               pkix.Name{CommonName: "Policy Root"},
			NotBefore:             notBefore,
			NotAfter:              notAfter,
			BasicConstraintsValid: true,
			IsCA:                  true,
			ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
			KeyUsage:              x509.KeyUsageCertSign,
			SignatureAlgorithm:    x509.ECDSAWithSHA256,
		},
		key: rootKey,
	}
	intermediate := templateAndKey{
		template: x509.Certificate{
			SerialNumber:          new(big.Int).SetInt64(2),
			Subject:               pkix.Name{CommonName: "Policy Intermediate"},
			NotBefore:             notBefore,
			NotAfter:              notAfter,
			BasicConstraintsValid: true,
			IsCA:                  true,
			ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
			KeyUsage:              x509.KeyUsageCertSign,
			SignatureAlgorithm:    x509.ECDSAWithSHA256,
			PolicyIdentifiers:     []asn1.ObjectIdentifier{testOID1, testOID2},
		},
		key: intermediateKey,
	}
	leaf := templateAndKey{
		template: x509.Certificate{
			SerialNumber:          new(big.Int).SetInt64(3),
			Subject:               pkix.Name{CommonName: "www.example.com"},
			NotBefore:             notBefore,
			NotAfter:              notAfter,
			BasicConstraintsValid: true,
			IsCA:                  false,
			ExtKeyUsage:           []x509.ExtKeyUsage{x509.ExtKeyUsageServerAuth},
			KeyUsage:              x509.KeyUsageCertSign,
			SignatureAlgorithm:    x509.ECDSAWithSHA256,
			DNSNames:              []string{"www.example.com"},
			PolicyIdentifiers:     []asn1.ObjectIdentifier{testOID1, testOID2},
		},
		key: leafKey,
	}

	// Generate a valid certificate chain from the templates.
	mustGenerateCertificate("policy_root.pem", &root, &root)
	mustGenerateCertificate("policy_intermediate.pem", &intermediate, &root)
	mustGenerateCertificate("policy_leaf.pem", &leaf, &intermediate)

	// Introduce syntax errors in the leaf and intermediate.
	leafInvalid := leaf
	leafInvalid.template.PolicyIdentifiers = nil
	leafInvalid.template.ExtraExtensions = []pkix.Extension{{Id: certificatePoliciesOID, Value: []byte("INVALID")}}
	mustGenerateCertificate("policy_leaf_invalid.pem", &leafInvalid, &root)

	intermediateInvalid := intermediate
	intermediateInvalid.template.PolicyIdentifiers = nil
	intermediateInvalid.template.ExtraExtensions = []pkix.Extension{{Id: certificatePoliciesOID, Value: []byte("INVALID")}}
	mustGenerateCertificate("policy_intermediate_invalid.pem", &intermediateInvalid, &root)

	// Duplicates are not allowed in certificatePolicies.
	leafDuplicate := leaf
	leafDuplicate.template.PolicyIdentifiers = []asn1.ObjectIdentifier{testOID1, testOID2, testOID2}
	mustGenerateCertificate("policy_leaf_duplicate.pem", &leafDuplicate, &root)

	intermediateDuplicate := intermediate
	intermediateDuplicate.template.PolicyIdentifiers = []asn1.ObjectIdentifier{testOID1, testOID2, testOID2}
	mustGenerateCertificate("policy_intermediate_duplicate.pem", &intermediateDuplicate, &root)

	// A version of the intermediate that sets requireExplicitPolicy without
	// skipping certificates.
	b := cryptobyte.NewBuilder(nil)
	b.AddASN1(cbasn1.SEQUENCE, func(seq *cryptobyte.Builder) {
		seq.AddASN1Int64WithTag(0, cbasn1.Tag(0).ContextSpecific())
	})
	intermediateRequire := intermediate
	intermediateRequire.template.ExtraExtensions = []pkix.Extension{{Id: policyConstraintsOID, Value: b.BytesOrPanic()}}
	mustGenerateCertificate("policy_intermediate_require.pem", &intermediateRequire, &root)

	// Same as above, but there are no policies on the intermediate.
	intermediateRequire.template.PolicyIdentifiers = nil
	mustGenerateCertificate("policy_intermediate_require_no_policies.pem", &intermediateRequire, &root)

	// Same as above, but the policy list has duplicates.
	intermediateRequire.template.PolicyIdentifiers = []asn1.ObjectIdentifier{testOID1, testOID2, testOID2}
	mustGenerateCertificate("policy_intermediate_require_duplicate.pem", &intermediateRequire, &root)

	// Corresponding certificates that instead assert the anyPolicy OID.
	intermediateAny := intermediate
	intermediateAny.template.PolicyIdentifiers = []asn1.ObjectIdentifier{anyPolicyOID}
	mustGenerateCertificate("policy_intermediate_any.pem", &intermediateAny, &root)

	leafAny := leaf
	leafAny.template.PolicyIdentifiers = []asn1.ObjectIdentifier{anyPolicyOID}
	mustGenerateCertificate("policy_leaf_any.pem", &leafAny, &intermediate)

	// An intermediate which maps OID1 to (OID2, OID3), and which asserts the
	// input OIDs either all at once, or as anyPolicy.
	b = cryptobyte.NewBuilder(nil)
	b.AddASN1(cbasn1.SEQUENCE, func(seq *cryptobyte.Builder) {
		// Map OID3 to (OID1, OID2).
		seq.AddASN1(cbasn1.SEQUENCE, func(mapping *cryptobyte.Builder) {
			mapping.AddASN1ObjectIdentifier(testOID3)
			mapping.AddASN1ObjectIdentifier(testOID1)
		})
		seq.AddASN1(cbasn1.SEQUENCE, func(mapping *cryptobyte.Builder) {
			mapping.AddASN1ObjectIdentifier(testOID3)
			mapping.AddASN1ObjectIdentifier(testOID2)
		})

		// Map all pairs of OID4 and OID5 to each other.
		seq.AddASN1(cbasn1.SEQUENCE, func(mapping *cryptobyte.Builder) {
			mapping.AddASN1ObjectIdentifier(testOID4)
			mapping.AddASN1ObjectIdentifier(testOID4)
		})
		seq.AddASN1(cbasn1.SEQUENCE, func(mapping *cryptobyte.Builder) {
			mapping.AddASN1ObjectIdentifier(testOID4)
			mapping.AddASN1ObjectIdentifier(testOID5)
		})
		seq.AddASN1(cbasn1.SEQUENCE, func(mapping *cryptobyte.Builder) {
			mapping.AddASN1ObjectIdentifier(testOID5)
			mapping.AddASN1ObjectIdentifier(testOID4)
		})
		seq.AddASN1(cbasn1.SEQUENCE, func(mapping *cryptobyte.Builder) {
			mapping.AddASN1ObjectIdentifier(testOID5)
			mapping.AddASN1ObjectIdentifier(testOID5)
		})
	})
	intermediateMapped := intermediate
	intermediateMapped.template.PolicyIdentifiers = []asn1.ObjectIdentifier{testOID1, testOID2, testOID3, testOID4, testOID5}
	intermediateMapped.template.ExtraExtensions = []pkix.Extension{{Id: policyMappingsOID, Value: b.BytesOrPanic()}}
	mustGenerateCertificate("policy_intermediate_mapped.pem", &intermediateMapped, &root)

	intermediateMapped.template.PolicyIdentifiers = []asn1.ObjectIdentifier{anyPolicyOID}
	mustGenerateCertificate("policy_intermediate_mapped_any.pem", &intermediateMapped, &root)

	intermediateMapped.template.PolicyIdentifiers = []asn1.ObjectIdentifier{testOID3}
	mustGenerateCertificate("policy_intermediate_mapped_oid3.pem", &intermediateMapped, &root)

	// Leaves which assert more specific OIDs, to test intermediate_mapped.
	leafSingle := leaf
	leafSingle.template.PolicyIdentifiers = []asn1.ObjectIdentifier{testOID1}
	mustGenerateCertificate("policy_leaf_oid1.pem", &leafSingle, &intermediate)
	leafSingle.template.PolicyIdentifiers = []asn1.ObjectIdentifier{testOID2}
	mustGenerateCertificate("policy_leaf_oid2.pem", &leafSingle, &intermediate)
	leafSingle.template.PolicyIdentifiers = []asn1.ObjectIdentifier{testOID3}
	mustGenerateCertificate("policy_leaf_oid3.pem", &leafSingle, &intermediate)
	leafSingle.template.PolicyIdentifiers = []asn1.ObjectIdentifier{testOID4}
	mustGenerateCertificate("policy_leaf_oid4.pem", &leafSingle, &intermediate)
	leafSingle.template.PolicyIdentifiers = []asn1.ObjectIdentifier{testOID5}
	mustGenerateCertificate("policy_leaf_oid5.pem", &leafSingle, &intermediate)

	// TODO(davidben): Generate more certificates to test policy validation more
	// extensively, including an intermediate with constraints. For now this
	// just tests the basic case.
}

const leafKeyPEM = `-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgoPUXNXuH9mgiS/nk
024SYxryxMa3CyGJldiHymLxSquhRANCAASRKti8VW2Rkma+Kt9jQkMNitlCs0l5
w8u3SSwm7HZREvmcBCJBjVIREacRqI0umhzR2V5NLzBBP9yPD/A+Ch5X
-----END PRIVATE KEY-----`

const intermediateKeyPEM = `-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgWHKCKgY058ahE3t6
vpxVQgzlycgCVMogwjK0y3XMNfWhRANCAATiOnyojN4xS5C8gJ/PHL5cOEsMbsoE
Y6KT9xRQSh8lEL4d1Vb36kqUgkpqedEImo0Og4Owk6VWVVR/m4Lk+yUw
-----END PRIVATE KEY-----`

const rootKeyPEM = `-----BEGIN PRIVATE KEY-----
MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgBwND/eHytW0I417J
Hr+qcPlp5N1jM3ACXys57bPujg+hRANCAAQmdqXYl1GvY7y3jcTTK6MVXIQr44Tq
ChRYI6IeV9tIB6jIsOY+Qol1bk8x/7A5FGOnUWFVLEAPEPSJwPndjolt
-----END PRIVATE KEY-----`

func mustParseECDSAKey(in string) *ecdsa.PrivateKey {
	keyBlock, _ := pem.Decode([]byte(in))
	if keyBlock == nil || keyBlock.Type != "PRIVATE KEY" {
		panic("could not decode private key")
	}
	key, err := x509.ParsePKCS8PrivateKey(keyBlock.Bytes)
	if err != nil {
		panic(err)
	}
	return key.(*ecdsa.PrivateKey)
}
