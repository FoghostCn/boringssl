// Copyright (c) 2018, Google Inc.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

//go:build ignore

// convert_wycheproof converts Wycheproof test vectors into a format more easily
// consumed by BoringSSL.
package main

import (
	"encoding/json"
	"fmt"
	"io"
	"os"
	"sort"
	"strings"
)

type wycheproofTest struct {
	Algorithm        string            `json:"algorithm"`
	GeneratorVersion string            `json:"generatorVersion"`
	NumberOfTests    int               `json:"numberOfTests"`
	Notes            map[string]string `json:"notes"`
	Header           []string          `json:"header"`
	// encoding/json does not support collecting unused keys, so we leave
	// everything past this point as generic.
	TestGroups []map[string]interface{} `json:"testGroups"`
}

func sortedKeys(m map[string]interface{}) []string {
	keys := make([]string, 0, len(m))
	for k, _ := range m {
		keys = append(keys, k)
	}
	sort.Strings(keys)
	return keys
}

func printAttribute(w io.Writer, key string, valueI interface{}, isInstruction bool) error {
	switch value := valueI.(type) {
	case float64:
		if float64(int(value)) != value {
			panic(key + "was not an integer.")
		}
		if isInstruction {
			if _, err := fmt.Fprintf(w, "[%s = %d]\n", key, int(value)); err != nil {
				return err
			}
		} else {
			if _, err := fmt.Fprintf(w, "%s = %d\n", key, int(value)); err != nil {
				return err
			}
		}
	case string:
		if strings.Contains(value, "\n") {
			panic(key + " contained a newline.")
		}
		if isInstruction {
			if _, err := fmt.Fprintf(w, "[%s = %s]\n", key, value); err != nil {
				return err
			}
		} else {
			if _, err := fmt.Fprintf(w, "%s = %s\n", key, value); err != nil {
				return err
			}
		}
	case map[string]interface{}:
		for _, k := range sortedKeys(value) {
			if err := printAttribute(w, key+"."+k, value[k], isInstruction); err != nil {
				return err
			}
		}
	default:
		panic(fmt.Sprintf("Unknown type for %q: %T", key, valueI))
	}
	return nil
}

func printComment(w io.Writer, in string) error {
	const width = 80 - 2
	lines := strings.Split(in, "\n")
	for _, line := range lines {
		for {
			if len(line) <= width {
				if _, err := fmt.Fprintf(w, "# %s\n", line); err != nil {
					return err
				}
				break
			}

			// Find the last space we can break at.
			n := strings.LastIndexByte(line[:width+1], ' ')
			if n < 0 {
				// The next word is too long. Wrap as soon as that word ends.
				n = strings.IndexByte(line[width+1:], ' ')
				if n < 0 {
					// This was the last word.
					if _, err := fmt.Fprintf(w, "# %s\n", line); err != nil {
						return nil
					}
					break
				}
				n += width + 1
			}
			if _, err := fmt.Fprintf(w, "# %s\n", line[:n]); err != nil {
				return err
			}
			line = line[n+1:] // Ignore the space.
		}
	}
	return nil
}

func convertWycheproof(f io.Writer, jsonPath string) error {
	jsonData, err := os.ReadFile(jsonPath)
	if err != nil {
		return err
	}

	var w wycheproofTest
	if err := json.Unmarshal(jsonData, &w); err != nil {
		return err
	}

	if _, err := fmt.Fprintf(f, `# Imported from Wycheproof's %s.
# This file is generated by convert_wycheproof.go. Do not edit by hand.
#
# Algorithm: %s
# Generator version: %s

`, jsonPath, w.Algorithm, w.GeneratorVersion); err != nil {
		return err
	}

	for _, group := range w.TestGroups {
		for _, k := range sortedKeys(group) {
			// Wycheproof files include keys in multiple formats. Skip PEM and
			// JWK formats. We process DER more easily. PEM has newlines and
			// JWK is a JSON object.
			if k == "type" || k == "tests" || strings.HasSuffix(k, "Pem") || strings.HasSuffix(k, "Jwk") || k == "jwk" {
				continue
			}
			if err := printAttribute(f, k, group[k], true); err != nil {
				return err
			}
		}
		fmt.Fprintf(f, "\n")
		tests := group["tests"].([]interface{})
		for _, testI := range tests {
			test := testI.(map[string]interface{})
			if _, err := fmt.Fprintf(f, "# tcId = %d\n", int(test["tcId"].(float64))); err != nil {
				return err
			}
			if comment, ok := test["comment"]; ok && len(comment.(string)) != 0 {
				if err := printComment(f, comment.(string)); err != nil {
					return err
				}
			}
			for _, k := range sortedKeys(test) {
				if k == "comment" || k == "flags" || k == "tcId" {
					continue
				}
				if err := printAttribute(f, k, test[k], false); err != nil {
					return err
				}
			}
			if flagsI, ok := test["flags"]; ok {
				var flags []string
				for _, flagI := range flagsI.([]interface{}) {
					flag := flagI.(string)
					flags = append(flags, flag)
				}
				if len(flags) != 0 {
					if err := printAttribute(f, "flags", strings.Join(flags, ","), false); err != nil {
						return err
					}
				}
			}
			if _, err := fmt.Fprintf(f, "\n"); err != nil {
				return err
			}
		}
	}
	return nil
}

var defaultInputs = []string{
	"aes_cbc_pkcs5_test.json",
	"aes_cmac_test.json",
	"aes_gcm_siv_test.json",
	"aes_gcm_test.json",
	"chacha20_poly1305_test.json",
	"dsa_test.json",
	"ecdh_secp224r1_test.json",
	"ecdh_secp256r1_test.json",
	"ecdh_secp384r1_test.json",
	"ecdh_secp521r1_test.json",
	"ecdsa_secp224r1_sha224_test.json",
	"ecdsa_secp224r1_sha256_test.json",
	"ecdsa_secp224r1_sha512_test.json",
	"ecdsa_secp256r1_sha256_test.json",
	"ecdsa_secp256r1_sha512_test.json",
	"ecdsa_secp384r1_sha384_test.json",
	"ecdsa_secp384r1_sha512_test.json",
	"ecdsa_secp521r1_sha512_test.json",
	"eddsa_test.json",
	"hkdf_sha1_test.json",
	"hkdf_sha256_test.json",
	"hkdf_sha384_test.json",
	"hkdf_sha512_test.json",
	"hmac_sha1_test.json",
	"hmac_sha224_test.json",
	"hmac_sha256_test.json",
	"hmac_sha384_test.json",
	"hmac_sha512_test.json",
	"kw_test.json",
	"kwp_test.json",
	"primality_test.json",
	"rsa_oaep_2048_sha1_mgf1sha1_test.json",
	"rsa_oaep_2048_sha224_mgf1sha1_test.json",
	"rsa_oaep_2048_sha224_mgf1sha224_test.json",
	"rsa_oaep_2048_sha256_mgf1sha1_test.json",
	"rsa_oaep_2048_sha256_mgf1sha256_test.json",
	"rsa_oaep_2048_sha384_mgf1sha1_test.json",
	"rsa_oaep_2048_sha384_mgf1sha384_test.json",
	"rsa_oaep_2048_sha512_mgf1sha1_test.json",
	"rsa_oaep_2048_sha512_mgf1sha512_test.json",
	"rsa_oaep_3072_sha256_mgf1sha1_test.json",
	"rsa_oaep_3072_sha256_mgf1sha256_test.json",
	"rsa_oaep_3072_sha512_mgf1sha1_test.json",
	"rsa_oaep_3072_sha512_mgf1sha512_test.json",
	"rsa_oaep_4096_sha256_mgf1sha1_test.json",
	"rsa_oaep_4096_sha256_mgf1sha256_test.json",
	"rsa_oaep_4096_sha512_mgf1sha1_test.json",
	"rsa_oaep_4096_sha512_mgf1sha512_test.json",
	"rsa_oaep_misc_test.json",
	"rsa_pkcs1_2048_test.json",
	"rsa_pkcs1_3072_test.json",
	"rsa_pkcs1_4096_test.json",
	"rsa_pss_2048_sha1_mgf1_20_test.json",
	"rsa_pss_2048_sha256_mgf1_0_test.json",
	"rsa_pss_2048_sha256_mgf1_32_test.json",
	"rsa_pss_3072_sha256_mgf1_32_test.json",
	"rsa_pss_4096_sha256_mgf1_32_test.json",
	"rsa_pss_4096_sha512_mgf1_32_test.json",
	"rsa_pss_misc_test.json",
	"rsa_sig_gen_misc_test.json",
	"rsa_signature_2048_sha224_test.json",
	"rsa_signature_2048_sha256_test.json",
	"rsa_signature_2048_sha384_test.json",
	"rsa_signature_2048_sha512_test.json",
	"rsa_signature_3072_sha256_test.json",
	"rsa_signature_3072_sha384_test.json",
	"rsa_signature_3072_sha512_test.json",
	"rsa_signature_4096_sha384_test.json",
	"rsa_signature_4096_sha512_test.json",
	"rsa_signature_test.json",
	"x25519_test.json",
	"xchacha20_poly1305_test.json",
}

func main() {
	switch len(os.Args) {
	case 1:
		for _, jsonPath := range defaultInputs {
			if !strings.HasSuffix(jsonPath, ".json") {
				panic(jsonPath)
			}

			txtPath := jsonPath[:len(jsonPath)-len(".json")] + ".txt"
			out, err := os.Create(txtPath)
			if err != nil {
				fmt.Fprintf(os.Stderr, "Error opening output %s: %s\n", txtPath, err)
				os.Exit(1)
			}
			defer out.Close()

			if err := convertWycheproof(out, jsonPath); err != nil {
				fmt.Fprintf(os.Stderr, "Error converting %s: %s\n", jsonPath, err)
				os.Exit(1)
			}
		}

	case 2:
		if err := convertWycheproof(os.Stdout, os.Args[1]); err != nil {
			fmt.Fprintf(os.Stderr, "Error converting %s: %s\n", os.Args[1], err)
			os.Exit(1)
		}

	default:
		fmt.Fprintf(os.Stderr, "Usage: %s [input JSON]\n", os.Args[0])
		os.Exit(1)
	}
}
