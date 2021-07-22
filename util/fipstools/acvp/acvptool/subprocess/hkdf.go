// Copyright (c) 2021, Google Inc.
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

package subprocess

import (
	"bytes"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"strings"
)

// The following structures reflect the JSON of ACVP KAS KDF tests. See
// https://pages.nist.gov/ACVP/draft-celi-acvp-kbkdf.html

type hkdfTestVectorSet struct {
	Groups []hkdfTestGroup `json:"testGroups"`
}

type hkdfTestGroup struct {
	ID    uint64     `json:"tgId"`
	Type  string     `json:"testType"` // AFT or VAL
	Tests []hkdfTest `json:"tests"`
}

type hkdfTest struct {
	ID          uint64         `json:"tcId"`
	Params      hkdfParameters `json:"kdfParameter"`
	PartyU      hkdfPartyInfo  `json:"fixedInfoPartyU"`
	PartyV      hkdfPartyInfo  `json:"fixedInfoPartyV"`
	ExpectedHex string         `json:"dkm"`
}

type hkdfParameters struct {
	Type               string `json:"kdfType"`
	AdditionalNonce    bool   `json:"requiresAdditionalNoncePair"`
	SaltHex            string `json:"salt"`
	KeyHex             string `json:"z"`
	OutputBits         uint32 `json:"l"`
	FixedInfoPattern   string `json:"fixedInfoPattern"`
	FixedInputEncoding string `json:"fixedInputEncoding"`
	KDFMode            string `json:"kdfMode"`
	MACMode            string `json:"macMode"`
	CounterLocation    string `json:"counterLocation"`
	CounterBits        uint   `json:"counterLen"`
}

func (p *hkdfParameters) extract() (key, salt []byte, outBytes uint32, hashName string, err error) {
	if p.Type != "twoStep" ||
		p.AdditionalNonce ||
		p.FixedInfoPattern != "uPartyInfo||vPartyInfo" ||
		p.FixedInputEncoding != "concatenation" ||
		p.KDFMode != "feedback" ||
		p.CounterLocation != "after fixed data" ||
		p.CounterBits != 8 ||
		p.OutputBits%8 != 0 {
		err = fmt.Errorf("KAS-KDF not configured for HKDF: %#v", p)
		return
	}

	salt, err = hex.DecodeString(p.SaltHex)
	if err != nil {
		return
	}

	key, err = hex.DecodeString(p.KeyHex)
	if err != nil {
		return
	}

	outBytes = p.OutputBits / 8

	if !strings.HasPrefix(p.MACMode, "HMAC-") {
		err = fmt.Errorf("MAC mode %q does't start with 'HMAC-'", p.MACMode)
	}
	hashName = p.MACMode[5:]
	return
}

type hkdfPartyInfo struct {
	IDHex    string `json:"partyId"`
	ExtraHex string `json:"ephemeralData"`
}

func (p *hkdfPartyInfo) data() ([]byte, error) {
	ret, err := hex.DecodeString(p.IDHex)
	if err != nil {
		return nil, err
	}

	if len(p.ExtraHex) > 0 {
		extra, err := hex.DecodeString(p.ExtraHex)
		if err != nil {
			return nil, err
		}
		ret = append(ret, extra...)
	}

	return ret, nil
}

type hkdfTestGroupResponse struct {
	ID    uint64             `json:"tgId"`
	Tests []hkdfTestResponse `json:"tests"`
}

type hkdfTestResponse struct {
	ID     uint64 `json:"tcId"`
	KeyOut string `json:"dkm,omitempty"`
	Passed *bool  `json:"testPassed,omitempty"`
}

type hkdf struct{}

func (k *hkdf) Process(vectorSet []byte, m Transactable) (interface{}, error) {
	var parsed hkdfTestVectorSet
	if err := json.Unmarshal(vectorSet, &parsed); err != nil {
		return nil, err
	}

	var respGroups []hkdfTestGroupResponse
	for _, group := range parsed.Groups {
		groupResp := hkdfTestGroupResponse{ID: group.ID}

		var isValidationTest bool
		switch group.Type {
		case "VAL":
			isValidationTest = true
		case "AFT":
			isValidationTest = false
		default:
			return nil, fmt.Errorf("unknown test type %q", group.Type)
		}

		for _, test := range group.Tests {
			testResp := hkdfTestResponse{ID: test.ID}

			key, salt, outBytes, hashName, err := test.Params.extract()
			if err != nil {
				return nil, err
			}
			uData, err := test.PartyU.data()
			if err != nil {
				return nil, err
			}
			vData, err := test.PartyV.data()
			if err != nil {
				return nil, err
			}

			var expected []byte
			if isValidationTest {
				expected, err = hex.DecodeString(test.ExpectedHex)
				if err != nil {
					return nil, err
				}
			}

			info := make([]byte, 0, len(uData)+len(vData))
			info = append(info, uData...)
			info = append(info, vData...)

			resp, err := m.Transact("HKDF/"+hashName, 1, key, salt, info, uint32le(outBytes))
			if err != nil {
				return nil, fmt.Errorf("HKDF operation failed: %s", err)
			}

			if isValidationTest {
				passed := bytes.Equal(expected, resp[0])
				testResp.Passed = &passed
			} else {
				testResp.KeyOut = hex.EncodeToString(resp[0])
			}

			groupResp.Tests = append(groupResp.Tests, testResp)
		}
		respGroups = append(respGroups, groupResp)
	}

	return respGroups, nil
}
