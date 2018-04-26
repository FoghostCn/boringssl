/* Copyright (c) 2018, Google Inc.
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

// convert_wycheproof.go converts Wycheproof test vectors into a format more
// easily consumed by BoringSSL.
package main

import (
	"encoding/json"
	"fmt"
	"io"
	"io/ioutil"
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

func isSupportedCurve(curve string) bool {
	switch curve {
	case "brainpoolP224r1", "brainpoolP224t1", "brainpoolP256r1", "brainpoolP256t1", "brainpoolP320r1", "brainpoolP320t1", "brainpoolP384r1", "brainpoolP384t1", "brainpoolP512r1", "brainpoolP512t1", "secp256k1":
		return false
	case "edwards25519", "curve25519", "secp224r1", "secp256r1", "secp384r1", "secp521r1":
		return true
	default:
		panic("Unknown curve: " + curve)
	}
}

func convertWycheproof(jsonPath, txtPath string) error {
	jsonData, err := ioutil.ReadFile(jsonPath)
	if err != nil {
		return err
	}

	var w wycheproofTest
	if err := json.Unmarshal(jsonData, &w); err != nil {
		return err
	}

	f, err := os.OpenFile(txtPath, os.O_CREATE|os.O_TRUNC|os.O_WRONLY, 0666)
	if err != nil {
		return err
	}
	defer f.Close()
	if _, err := fmt.Fprintf(f, `# Imported from Wycheproof's %s.
# This file is generated by convert_wycheproof.go. Do not edit by hand.
#
# Algorithm: %s
# Generator version: %s

`, jsonPath, w.Algorithm, w.GeneratorVersion); err != nil {
		return err
	}

	for _, group := range w.TestGroups {
		// Skip tests with unsupported curves. We filter these out at
		// conversion time to avoid unnecessarily inflating
		// crypto_test_data.cc.
		if curve, ok := group["curve"]; ok && !isSupportedCurve(curve.(string)) {
			continue
		}
		if keyI, ok := group["key"]; ok {
			if key, ok := keyI.(map[string]interface{}); ok {
				if curve, ok := key["curve"]; ok && !isSupportedCurve(curve.(string)) {
					continue
				}
			}
		}

		for _, k := range sortedKeys(group) {
			// Wycheproof files always include both keyPem and
			// keyDer. Skip keyPem as they contain newlines. We
			// process keyDer more easily.
			if k == "type" || k == "tests" || k == "keyPem" {
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
			// Skip tests with unsupported curves.
			if curve, ok := test["curve"]; ok && !isSupportedCurve(curve.(string)) {
				continue
			}
			if comment, ok := test["comment"]; ok {
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
			if flags, ok := test["flags"]; ok {
				for _, flag := range flags.([]interface{}) {
					if note, ok := w.Notes[flag.(string)]; ok {
						if err := printComment(f, note); err != nil {
							return err
						}
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

func main() {
	jsonPaths := []string{
		"x25519_test.json",

		// TODO(davidben): The following tests still need test drivers.
		// "aes_cbc_pkcs5_test.json",
		// "aes_gcm_siv_test.json",
		// "aes_gcm_test.json",
		// "chacha20_poly1305_test.json",
		// "dsa_test.json",
		// "ecdh_test.json",
		// "ecdsa_secp224r1_sha224_test.json",
		// "ecdsa_secp224r1_sha256_test.json",
		// "ecdsa_secp256r1_sha256_test.json",
		// "ecdsa_secp384r1_sha384_test.json",
		// "ecdsa_secp384r1_sha512_test.json",
		// "ecdsa_secp521r1_sha512_test.json",
		// "eddsa_test.json",
		// "rsa_signature_test.json",
	}
	for _, jsonPath := range jsonPaths {
		if !strings.HasSuffix(jsonPath, ".json") {
			panic(jsonPath)
		}
		txtPath := jsonPath[:len(jsonPath)-len(".json")] + ".txt"
		if err := convertWycheproof(jsonPath, txtPath); err != nil {
			fmt.Fprintf(os.Stderr, "Error converting %s: %s\n", jsonPath, err)
			os.Exit(1)
		}
	}
}
