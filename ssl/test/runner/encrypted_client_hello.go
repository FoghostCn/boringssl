// Copyright (c) 2020, Google Inc.
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

package runner

import (
	"crypto"
	"errors"

	"golang.org/x/crypto/hkdf"

	"boringssl.googlesource.com/boringssl/ssl/test/runner/hpke"
)

type EchConfig struct {
	version           uint16
	echConfigContents echConfigContents

	// secretKey is only used by servers. It is not part of serialization or
	// equality.
	secretKey []byte
}

func (e *EchConfig) Marshal() []byte {
	bb := newByteBuilder()
	bb.addU16(e.version)
	bb.addU16LengthPrefixed().addBytes(e.echConfigContents.marshal())
	return bb.finish()
}

func (e *EchConfig) unmarshal(reader *byteReader) bool {
	var contentsBytes []byte
	if !reader.readU16(&e.version) ||
		!reader.readU16LengthPrefixedBytes(&contentsBytes) ||
		!e.echConfigContents.unmarshal(contentsBytes) {
		return false
	}
	return true
}

func (e *EchConfig) hash(h crypto.Hash) []byte {
	return h.New().Sum(e.Marshal())
}

type echConfigContents struct {
	publicName    []byte
	publicKey     []byte
	kem           uint16
	cipherSuites  []hpkeCipherSuite
	maxNameLength uint16
	extensions    []byte
}

func (e *echConfigContents) marshal() []byte {
	bb := newByteBuilder()
	bb.addU16LengthPrefixed().addBytes(e.publicName)
	bb.addU16LengthPrefixed().addBytes(e.publicKey)
	bb.addU16(e.kem)

	bbCipherSuites := bb.addU16LengthPrefixed()
	for _, cipherSuite := range e.cipherSuites {
		bbCipherSuites.addBytes(cipherSuite.marshal())
	}

	bb.addU16(e.maxNameLength)
	bb.addU16LengthPrefixed().addBytes(e.extensions)
	return bb.finish()
}

func (e *echConfigContents) unmarshal(data []byte) bool {
	reader := byteReader(data)
	if !reader.readU16LengthPrefixedBytes(&e.publicName) ||
		!reader.readU16LengthPrefixedBytes(&e.publicKey) ||
		!reader.readU16(&e.kem) {
		return false
	}

	var cipherSuitesReader byteReader
	if !reader.readU16LengthPrefixed(&cipherSuitesReader) {
		return false
	}
	for len(cipherSuitesReader) > 0 {
		var cipherSuite hpkeCipherSuite
		if !cipherSuite.unmarshal(&cipherSuitesReader) {
			return false
		}
		e.cipherSuites = append(e.cipherSuites, cipherSuite)
	}

	if !reader.readU16(&e.maxNameLength) ||
		!reader.readU16LengthPrefixedBytes(&e.extensions) {
		return false
	}

	return true
}

// GenerateECHConfigWithSecretKey constructs a valid ECHConfig and corresponding
// private key for the server.
func GenerateECHConfigWithSecretKey(publicName string) (*EchConfig, error) {
	publicKeyR, secretKeyR, err := hpke.DHKEMX25519{}.GenerateKeyPair()
	if err != nil {
		return nil, err
	}
	result := EchConfig{
		version: echVersion,
		echConfigContents: echConfigContents{
			publicName: []byte(publicName),
			publicKey:  publicKeyR,
			kem:        hpke.X25519WithHKDFSHA256,
			cipherSuites: []hpkeCipherSuite{
				{
					KDF:  hpke.HKDFSHA256,
					AEAD: hpke.AES256GCM,
				},
			},
			// For real-life purposes, the maxNameLength should be
			// based on the set of domain names that the server
			// represents.
			maxNameLength: 16,
			extensions:    nil,
		},
		secretKey: secretKeyR,
	}
	return &result, nil
}

func echServerHelloRandom(kdf uint16, clientRandom, serverRandomExtracted []byte) ([]byte, error) {
	if len(serverRandomExtracted) != 24 {
		panic("serverRandomExtracted must be 24 bytes")
	}
	h, err := hpke.GetHash(kdf)
	if err != nil {
		return nil, err
	}
	var secret []byte
	secret = append(secret, clientRandom...)
	secret = append(secret, serverRandomExtracted...)
	shared_secret := hkdf.Extract(h.New, secret, nil)[0:8]
	return shared_secret, nil
}

func echElideOuterExtensions(clientHello []byte, outerExtensions *echOuterExtensions) ([]byte, bool) {
	panic("not implemented")
}

func echIncorporateOuterExtensions(chInnerBytes, chOuterBytes []byte) ([]byte, error) {
	// TODO(dmcardle): check the hash before returning.

	// Get the outer ClientHello's extensions.
	var chOuter clientHelloMsg
	_, chOuterExtensionsBytes, ok := chOuter.unmarshalUntilExtensions(chOuterBytes)
	if !ok {
		return nil, errors.New("failed to parse outer CH")
	}
	chOuterExtensions := byteReader(chOuterExtensionsBytes)

	// Get the inner ClientHello's extensions.
	var chInner clientHelloMsg
	chInnerBeforeExtensionsBytes, chInnerExtensionsBytes, ok := chInner.unmarshalUntilExtensions(chInnerBytes)
	if !ok {
		panic("failed to parse inner CH")
	}
	chInnerExtensions := byteReader(chInnerExtensionsBytes)

	// Reconstruct the inner CH with extensions from the outer CH. Start by
	// copying everything before the extensions.
	chInnerFull := newByteBuilder()
	chInnerFull.addBytes(chInnerBeforeExtensionsBytes)
	chInnerFullExtensions := chInnerFull.addU16LengthPrefixed()

	// Construct a map from extension types to extension bodies for the
	// outer CH.
	outerExtMap := make(map[uint16][]byte)
	for len(chOuterExtensions) > 0 {
		var extension uint16
		var body []byte
		if !chOuterExtensions.readU16(&extension) ||
			!chOuterExtensions.readU16LengthPrefixedBytes(&body) {
			return nil, errors.New("failed to unmarshal outer CH")
		}
		outerExtMap[extension] = body
	}

	// Walk through the inner CH's extensions.
	for len(chInnerExtensions) > 0 {
		var extension uint16
		var body []byte
		if !chInnerExtensions.readU16(&extension) ||
			!chInnerExtensions.readU16LengthPrefixedBytes(&body) {
			return nil, errors.New("failed to unmarshal inner CH")
		}
		// If it's not the "outer_extensions" extension, preserve it.
		if extension != extensionEchOuterExtensions {
			chInnerFullExtensions.addU16(extension)
			chInnerFullExtensions.addU16LengthPrefixed().addBytes(body)
			continue
		}
		// Parse the "outer_extensions" extension. For each requested
		// extension in chOuterExtensions, copy the extension from the
		// outer CH to chInnerFull.
		var outerExts echOuterExtensions
		if !outerExts.unmarshal(body) {
			return nil, errors.New("failed to unmarshal extensionEchOuterExtensions")
		}
		for _, outerExt := range outerExts.outerExtensions {
			extBody, ok := outerExtMap[outerExt]
			if !ok {
				return nil, errors.New("cannot find required extension in outer CH")
			}
			chInnerFullExtensions.addU16(outerExt)
			chInnerFullExtensions.addU16LengthPrefixed().addBytes(extBody)
		}
	}
	return chInnerFull.finish(), nil
}
