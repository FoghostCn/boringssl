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

// Package hpke implements Hybrid Public Key Encryption (HPKE).
//
// See https://tools.ietf.org/html/draft-irtf-cfrg-hpke-05.
package hpke

import (
	"crypto/aes"
	"crypto/cipher"
	"encoding/binary"
	"errors"

	"golang.org/x/crypto/chacha20poly1305"
)

// KEM scheme IDs.
const (
	X25519WithHKDFSHA256 uint16 = 0x0020
)

// HPKE AEAD IDs.
const (
	AES128GCM        uint16 = 0x0001
	AES256GCM        uint16 = 0x0002
	ChaCha20Poly1305 uint16 = 0x0003
)

// HPKE KDF IDs.
const (
	HKDFSHA256 uint16 = 0x0001
	HKDFSHA384 uint16 = 0x0002
	HKDFSHA512 uint16 = 0x0003
)

// Internal constants.
const (
	hpkeModeBase uint8 = 0
)

type GenerateKeyPairFunc func() (public []byte, secret []byte, e error)

// Context holds the HPKE state for a sender or a receiver.
type Context struct {
	kemID  uint16
	kdfID  uint16
	aeadID uint16

	kdf  labeledHKDF
	aead cipher.AEAD

	key            []byte
	baseNonce      []byte
	seq            uint64
	exporterSecret []byte
}

// SetupBaseSenderX25519 corresponds to the spec's SetupBaseS(), but only
// supports X25519.
func SetupBaseSenderX25519(kdfID, aeadID uint16, publicKeyR, info []byte, ephemKeygen GenerateKeyPairFunc) (context *Context, enc []byte, err error) {
	kem := DHKEMX25519{}
	sharedSecret, enc, err := kem.Encap(publicKeyR, ephemKeygen)
	if err != nil {
		return nil, nil, err
	}
	context, err = keySchedule(kem.ID(), kdfID, aeadID, sharedSecret, info)
	return
}

// SetupBaseReceiverX25519 corresponds to the spec's SetupBaseR(), but only
// supports X25519.
func SetupBaseReceiverX25519(kdfID, aeadID uint16, enc, secretKeyR, info []byte) (context *Context, err error) {
	kem := DHKEMX25519{}
	sharedSecret, err := kem.Decap(enc, secretKeyR)
	if err != nil {
		return nil, err
	}
	return keySchedule(kem.ID(), kdfID, aeadID, sharedSecret, info)
}

func (c *Context) Seal(additionalData, plaintext []byte) []byte {
	ciphertext := c.aead.Seal(nil, c.computeNonce(), plaintext, additionalData)
	c.incrementSeq()
	return ciphertext
}

func (c *Context) Open(additionalData, ciphertext []byte) ([]byte, error) {
	plaintext, err := c.aead.Open(nil, c.computeNonce(), ciphertext, additionalData)
	if err != nil {
		return nil, err
	}
	c.incrementSeq()
	return plaintext, nil
}

func (c *Context) Export(exporterContext []byte, length int) []byte {
	suiteID := buildSuiteID(c.kemID, c.kdfID, c.aeadID)
	return c.kdf.LabeledExpand(c.exporterSecret, suiteID, []byte("sec"), exporterContext, length)
}

func buildSuiteID(kemID, kdfID, aeadID uint16) []byte {
	ret := make([]byte, 0, 10)
	ret = append(ret, "HPKE"...)
	ret = appendBigEndianUint16(ret, kemID)
	ret = appendBigEndianUint16(ret, kdfID)
	ret = appendBigEndianUint16(ret, aeadID)
	return ret
}

func newAEAD(aeadID uint16, key []byte) (cipher.AEAD, error) {
	if len(key) != expectedKeyLength(aeadID) {
		return nil, errors.New("wrong key length for specified AEAD")
	}
	switch aeadID {
	case AES128GCM, AES256GCM:
		block, err := aes.NewCipher(key)
		if err != nil {
			return nil, err
		}
		aead, err := cipher.NewGCM(block)
		if err != nil {
			return nil, err
		}
		return aead, nil
	case ChaCha20Poly1305:
		aead, err := chacha20poly1305.New(key)
		if err != nil {
			return nil, err
		}
		return aead, nil
	}
	return nil, errors.New("unsupported AEAD")
}

func keySchedule(kemID, kdfID, aeadID uint16, sharedSecret, info []byte) (*Context, error) {
	kdf := NewKDF(kdfID)
	suiteID := buildSuiteID(kemID, kdfID, aeadID)
	pskIDHash := kdf.LabeledExtract(nil, suiteID, []byte("psk_id_hash"), nil)
	infoHash := kdf.LabeledExtract(nil, suiteID, []byte("info_hash"), info)

	keyScheduleContext := make([]byte, 0)
	keyScheduleContext = append(keyScheduleContext, hpkeModeBase)
	keyScheduleContext = append(keyScheduleContext, pskIDHash...)
	keyScheduleContext = append(keyScheduleContext, infoHash...)

	pskHash := kdf.LabeledExtract(nil, suiteID, []byte("psk_hash"), nil)
	secret := kdf.LabeledExtract(pskHash, suiteID, []byte("secret"), sharedSecret)
	key := kdf.LabeledExpand(secret, suiteID, []byte("key"), keyScheduleContext, expectedKeyLength(aeadID))

	aead, err := newAEAD(aeadID, key)
	if err != nil {
		return nil, err
	}

	nonce := kdf.LabeledExpand(secret, suiteID, []byte("nonce"), keyScheduleContext, aead.NonceSize())
	exporterSecret := kdf.LabeledExpand(secret, suiteID, []byte("exp"), keyScheduleContext, kdf.Size())

	return &Context{
		kemID:          kemID,
		kdfID:          kdfID,
		aeadID:         aeadID,
		kdf:            kdf,
		aead:           aead,
		key:            key,
		baseNonce:      nonce,
		seq:            0,
		exporterSecret: exporterSecret,
	}, nil
}

func (c Context) computeNonce() []byte {
	nonce := make([]byte, len(c.baseNonce))
	// Write the big-endian |c.seq| value at the *end* of |baseNonce|.
	binary.BigEndian.PutUint64(nonce[len(nonce)-8:], c.seq)
	// XOR the big-endian |seq| with |c.baseNonce|.
	for i, b := range c.baseNonce {
		nonce[i] ^= b
	}
	return nonce
}

func (c *Context) incrementSeq() {
	c.seq++
	if c.seq == 0 {
		panic("sequence overflow")
	}
}

func expectedKeyLength(aeadID uint16) int {
	switch aeadID {
	case AES128GCM:
		return 128 / 8
	case AES256GCM:
		return 256 / 8
	case ChaCha20Poly1305:
		return chacha20poly1305.KeySize
	}
	panic("unsupported AEAD")
}

func appendBigEndianUint16(b []byte, v uint16) []byte {
	return append(b, byte(v>>8), byte(v))
}
