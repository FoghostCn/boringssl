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

package hpke

import (
	"crypto"
	"crypto/rand"
	"errors"

	"golang.org/x/crypto/curve25519"
	"golang.org/x/crypto/hkdf"
)

const (
	rfcLabel string = "RFCXXXX "
)

type labeledKdf interface {
	Nh() uint16
	LabeledExtract(salt, suiteID, label, ikm []byte) []byte
	LabeledExpand(prk, suiteID, label, info []byte, length uint16) ([]byte, error)
}

func newKdf(kdfID uint16) labeledKdf {
	switch kdfID {
	case HpkeHkdfSha256:
		return labeledHkdf{h: crypto.SHA256}
	case HpkeHkdfSha384:
		return labeledHkdf{h: crypto.SHA384}
	case HpkeHkdfSha512:
		return labeledHkdf{h: crypto.SHA512}
	}
	panic("Unknown kdfID")
}

// labeledHkdf implements the LabeledKdf interface for hash-based KDFs.
type labeledHkdf struct {
	h crypto.Hash
}

func (h labeledHkdf) Nh() uint16 {
	return uint16(h.h.Size())
}
func (h labeledHkdf) LabeledExtract(salt, suiteID, label, ikm []byte) []byte {
	labeledIKM := concat([]byte(rfcLabel), suiteID, label, ikm)
	return hkdf.Extract(h.h.New, labeledIKM, salt)
}
func (h labeledHkdf) LabeledExpand(prk, suiteID, label, info []byte, length uint16) ([]byte, error) {
	labeledInfo := concat(encodeBigEndianUint16(length), []byte(rfcLabel), suiteID, label, info)
	reader := hkdf.Expand(h.h.New, prk, labeledInfo)
	key := make([]uint8, length)
	_, err := reader.Read(key)
	if err != nil {
		return nil, err
	}
	return key, nil
}

// dhkemX25519 implements the Kem interface.
type dhkemX25519 struct{}

func (d dhkemX25519) ID() uint16 {
	return HpkeDhkemX25519HkdfSha256
}
func (d dhkemX25519) GenerateKeyPair() (publicKey, secretKeyOut []byte, err error) {
	secretKey, err := newX25519PrivateKey()
	if err != nil {
		return
	}
	publicKeyTmp := secretKey.publicKey()
	return publicKeyTmp[:], secretKey.scalar[:], nil
}
func (d dhkemX25519) Encap(publicKeyR []byte, keygen GenerateKeyPairFun) ([]byte, []byte, error) {
	if keygen == nil {
		keygen = d.GenerateKeyPair
	}
	publicKeyEphem, secretKeyEphem, err := keygen()
	if err != nil {
		return nil, nil, err
	}
	dh, err := curve25519.X25519(secretKeyEphem, publicKeyR)
	if err != nil {
		return nil, nil, err
	}
	kemContext := concat(publicKeyEphem, publicKeyR)
	zz, err := d.extractAndExpand(dh, kemContext)
	if err != nil {
		return nil, nil, err
	}
	return zz, publicKeyEphem, nil
}
func (d dhkemX25519) Decap(enc, secretKeyR []byte) ([]byte, error) {
	publicKeyEphem := enc
	dh, err := curve25519.X25519(secretKeyR, publicKeyEphem)
	if err != nil {
		return nil, err
	}
	wrappedSecretKeyR, err := newX25519PrivateKeyFromBytes(secretKeyR)
	if err != nil {
		return nil, err
	}
	publicKeyR := wrappedSecretKeyR.publicKey()
	kemContext := concat(enc, publicKeyR[:])
	zz, err := d.extractAndExpand(dh, kemContext)
	if err != nil {
		return nil, err
	}
	return zz, nil
}
func (d dhkemX25519) Nenc() uint16 {
	return 32
}
func (d dhkemX25519) Nzz() uint16 {
	return 32
}
func (d dhkemX25519) extractAndExpand(dh []byte, kemContext []byte) ([]byte, error) {
	kdf := newKdf(HpkeHkdfSha256)
	suiteID := concat([]byte("KEM"), encodeBigEndianUint16(d.ID()))

	prk := kdf.LabeledExtract(nil, suiteID, []byte("eae_prk"), dh)
	key, err := kdf.LabeledExpand(prk, suiteID, []byte("zz"), kemContext, d.Nzz())
	if err != nil {
		return nil, err
	}
	return key, nil
}

// x25519PrivateKey is a wrapper around curve25519.
type x25519PrivateKey struct {
	scalar [curve25519.ScalarSize]byte
}

// newX25519PrivateKeyFromBytes generates a new, random key.
func newX25519PrivateKey() (*x25519PrivateKey, error) {
	key := &x25519PrivateKey{}
	_, err := rand.Read(key.scalar[:])
	return key, err
}

// newX25519PrivateKeyFromBytes builds a new x25519PrivateKey from the private
// key |scalar|.
func newX25519PrivateKeyFromBytes(scalar []byte) (*x25519PrivateKey, error) {
	key := &x25519PrivateKey{}
	if len(scalar) != len(key.scalar) {
		return nil, errors.New("secretKeyR has wrong length")
	}
	copy(key.scalar[:], scalar)
	return key, nil
}

// publicKey recovers the public key corresponding to this private key.
func (x x25519PrivateKey) publicKey() [32]byte {
	var publicKey [32]byte
	curve25519.ScalarBaseMult(&publicKey, &x.scalar)
	return publicKey
}
