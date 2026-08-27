#pragma once

// -----------------------------------------------------------------------------
// Self-contained crypto for the sealed Photon AppID envelope.
//
// The backend serves Photon credentials as an AES-256-GCM envelope whose
// additional authenticated data is "<player id>|<mode>", so an envelope issued
// for one account cannot be replayed by another. This header implements exactly
// what is needed to open one: SHA-256 (key derivation from a passphrase),
// AES-256 encryption (the GCM keystream and the hash key), GHASH, and Base64
// decoding.
//
// Why not a library: this .so must stay a single artifact with no new
// dependencies, and the NDK ships no crypto library that is stable across the
// API levels this build targets. Every primitive below is the plain textbook
// construction with no allocations in the hot path.
//
// Tag handling follows Go's crypto/cipher: the tag is the last 16 bytes of the
// ciphertext. Verification is constant-time and fail-closed.
// -----------------------------------------------------------------------------

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace opg3d_crypto {
namespace detail {

// ------------------------------------------------------------------- SHA-256

inline constexpr uint32_t kSha256K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u,
};

inline uint32_t rotr32(uint32_t value, unsigned bits) {
    return (value >> bits) | (value << (32u - bits));
}

inline void sha256_block(const uint8_t* block, uint32_t* state) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = (static_cast<uint32_t>(block[i * 4 + 0]) << 24) |
               (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) |
               static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; ++i) {
        const uint32_t s0 =
            rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const uint32_t s1 =
            rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; ++i) {
        const uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t t1 = h + s1 + ch + kSha256K[i] + w[i];
        const uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t t2 = s0 + maj;
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

// ----------------------------------------------------------------- AES-256

inline constexpr uint8_t kSbox[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16,
};

inline uint8_t xtime(uint8_t value) {
    return static_cast<uint8_t>((value << 1) ^ ((value & 0x80u) ? 0x1bu : 0x00u));
}

// AES-256: 14 rounds, 60 words of round key.
struct Aes256 {
    uint8_t round_key[240];

    void expand(const uint8_t key[32]) {
        std::memcpy(round_key, key, 32u);
        uint8_t rcon = 1u;
        for (size_t i = 32u; i < 240u; i += 4u) {
            uint8_t t[4];
            std::memcpy(t, round_key + i - 4u, 4u);
            if ((i % 32u) == 0u) {
                const uint8_t rotated = t[0];
                t[0] = static_cast<uint8_t>(kSbox[t[1]] ^ rcon);
                t[1] = kSbox[t[2]];
                t[2] = kSbox[t[3]];
                t[3] = kSbox[rotated];
                rcon = xtime(rcon);
            } else if ((i % 32u) == 16u) {
                for (int j = 0; j < 4; ++j) t[j] = kSbox[t[j]];
            }
            for (int j = 0; j < 4; ++j) {
                round_key[i + static_cast<size_t>(j)] =
                    static_cast<uint8_t>(round_key[i - 32u + static_cast<size_t>(j)] ^ t[j]);
            }
        }
    }

    void encrypt_block(const uint8_t in[16], uint8_t out[16]) const {
        uint8_t s[16];
        for (int i = 0; i < 16; ++i) s[i] = static_cast<uint8_t>(in[i] ^ round_key[i]);

        for (int round = 1; round <= 14; ++round) {
            for (int i = 0; i < 16; ++i) s[i] = kSbox[s[i]];

            // ShiftRows on the column-major AES state.
            uint8_t t;
            t = s[1]; s[1] = s[5]; s[5] = s[9]; s[9] = s[13]; s[13] = t;
            t = s[2]; s[2] = s[10]; s[10] = t;
            t = s[6]; s[6] = s[14]; s[14] = t;
            t = s[15]; s[15] = s[11]; s[11] = s[7]; s[7] = s[3]; s[3] = t;

            if (round != 14) {
                for (int c = 0; c < 4; ++c) {
                    uint8_t* col = s + c * 4;
                    const uint8_t a0 = col[0], a1 = col[1], a2 = col[2], a3 = col[3];
                    const uint8_t all = static_cast<uint8_t>(a0 ^ a1 ^ a2 ^ a3);
                    col[0] = static_cast<uint8_t>(a0 ^ all ^ xtime(static_cast<uint8_t>(a0 ^ a1)));
                    col[1] = static_cast<uint8_t>(a1 ^ all ^ xtime(static_cast<uint8_t>(a1 ^ a2)));
                    col[2] = static_cast<uint8_t>(a2 ^ all ^ xtime(static_cast<uint8_t>(a2 ^ a3)));
                    col[3] = static_cast<uint8_t>(a3 ^ all ^ xtime(static_cast<uint8_t>(a3 ^ a0)));
                }
            }

            const uint8_t* rk = round_key + round * 16;
            for (int i = 0; i < 16; ++i) s[i] = static_cast<uint8_t>(s[i] ^ rk[i]);
        }
        std::memcpy(out, s, 16u);
    }
};

// -------------------------------------------------------------------- GHASH

struct Ghash {
    uint64_t h_high = 0u;
    uint64_t h_low = 0u;
    uint64_t y_high = 0u;
    uint64_t y_low = 0u;

    static uint64_t load_be64(const uint8_t* p) {
        uint64_t v = 0u;
        for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
        return v;
    }

    static void store_be64(uint64_t v, uint8_t* p) {
        for (int i = 7; i >= 0; --i) {
            p[i] = static_cast<uint8_t>(v & 0xffu);
            v >>= 8;
        }
    }

    void init(const uint8_t h[16]) {
        h_high = load_be64(h);
        h_low = load_be64(h + 8);
        y_high = 0u;
        y_low = 0u;
    }

    // Carry-less multiply in GF(2^128) with the GCM reduction polynomial,
    // most-significant bit first. Plain shift-and-xor: correctness over speed,
    // and only a handful of blocks are ever processed.
    void multiply() {
        uint64_t z_high = 0u, z_low = 0u;
        uint64_t v_high = h_high, v_low = h_low;
        for (int i = 0; i < 128; ++i) {
            const uint64_t bit = (i < 64) ? ((y_high >> (63 - i)) & 1u)
                                          : ((y_low >> (127 - i)) & 1u);
            if (bit != 0u) {
                z_high ^= v_high;
                z_low ^= v_low;
            }
            const bool lsb = (v_low & 1u) != 0u;
            v_low = (v_low >> 1) | (v_high << 63);
            v_high >>= 1;
            if (lsb) v_high ^= 0xe100000000000000ull;
        }
        y_high = z_high;
        y_low = z_low;
    }

    void update(const uint8_t* data, size_t size) {
        size_t offset = 0u;
        while (offset < size) {
            uint8_t block[16] = {0};
            const size_t chunk = (size - offset) < 16u ? (size - offset) : 16u;
            std::memcpy(block, data + offset, chunk);
            y_high ^= load_be64(block);
            y_low ^= load_be64(block + 8);
            multiply();
            offset += chunk;
        }
    }

    void finish(uint8_t out[16]) const {
        store_be64(y_high, out);
        store_be64(y_low, out + 8);
    }
};

inline int base64_value(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}

inline int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

} // namespace detail

inline void sha256(const void* data, size_t size, uint8_t out[32]) {
    uint32_t state[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                         0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    size_t offset = 0u;
    while (size - offset >= 64u) {
        detail::sha256_block(bytes + offset, state);
        offset += 64u;
    }

    uint8_t tail[128] = {0};
    const size_t remainder = size - offset;
    std::memcpy(tail, bytes + offset, remainder);
    tail[remainder] = 0x80u;
    const size_t total = (remainder + 9u > 64u) ? 128u : 64u;
    const uint64_t bits = static_cast<uint64_t>(size) * 8u;
    for (int i = 0; i < 8; ++i) {
        tail[total - 1u - static_cast<size_t>(i)] =
            static_cast<uint8_t>((bits >> (8 * i)) & 0xffu);
    }
    for (size_t i = 0u; i < total; i += 64u) detail::sha256_block(tail + i, state);

    for (int i = 0; i < 8; ++i) {
        out[i * 4 + 0] = static_cast<uint8_t>(state[i] >> 24);
        out[i * 4 + 1] = static_cast<uint8_t>(state[i] >> 16);
        out[i * 4 + 2] = static_cast<uint8_t>(state[i] >> 8);
        out[i * 4 + 3] = static_cast<uint8_t>(state[i]);
    }
}

// Mirrors the server's key derivation: exactly 64 hex characters are the raw
// key, anything else is a passphrase hashed with SHA-256.
inline bool derive_key(const std::string& material, uint8_t out[32]) {
    if (material.empty()) return false;
    if (material.size() == 64u) {
        bool all_hex = true;
        for (size_t i = 0u; i < 64u && all_hex; ++i) {
            all_hex = detail::hex_value(material[i]) >= 0;
        }
        if (all_hex) {
            for (size_t i = 0u; i < 32u; ++i) {
                out[i] = static_cast<uint8_t>(
                    (detail::hex_value(material[i * 2u]) << 4) |
                    detail::hex_value(material[i * 2u + 1u]));
            }
            return true;
        }
    }
    sha256(material.data(), material.size(), out);
    return true;
}

// Standard and URL-safe alphabets, with or without padding.
inline bool base64_decode(const std::string& text, std::string* out) {
    if (out == nullptr) return false;
    out->clear();
    out->reserve(text.size() * 3u / 4u + 3u);
    uint32_t accumulator = 0u;
    int bits = 0;
    for (const char c : text) {
        if (c == '=' || c == '\n' || c == '\r' || c == ' ' || c == '\t') continue;
        const int value = detail::base64_value(c);
        if (value < 0) return false;
        accumulator = (accumulator << 6) | static_cast<uint32_t>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out->push_back(static_cast<char>((accumulator >> bits) & 0xffu));
        }
    }
    return true;
}

// Opens an AES-256-GCM envelope. `ciphertext` carries the 16-byte tag at the
// end, exactly as Go's crypto/cipher Seal produces it. Returns false on any
// length problem or tag mismatch, and never yields unverified plaintext.
inline bool aes256gcm_open(const uint8_t key[32], const std::string& nonce,
                          const std::string& ciphertext, const std::string& aad,
                          std::string* plaintext) {
    constexpr size_t kTagSize = 16u;
    constexpr size_t kNonceSize = 12u;
    if (plaintext == nullptr) return false;
    if (nonce.size() != kNonceSize) return false;
    if (ciphertext.size() < kTagSize) return false;

    const size_t body = ciphertext.size() - kTagSize;
    const uint8_t* ct = reinterpret_cast<const uint8_t*>(ciphertext.data());
    const uint8_t* tag = ct + body;

    detail::Aes256 aes;
    aes.expand(key);

    uint8_t zero[16] = {0};
    uint8_t hash_key[16];
    aes.encrypt_block(zero, hash_key);

    // J0 for a 96-bit nonce is nonce || 0x00000001.
    uint8_t counter[16] = {0};
    std::memcpy(counter, nonce.data(), kNonceSize);
    counter[15] = 1u;

    uint8_t tag_mask[16];
    aes.encrypt_block(counter, tag_mask);

    detail::Ghash ghash;
    ghash.init(hash_key);
    ghash.update(reinterpret_cast<const uint8_t*>(aad.data()), aad.size());
    ghash.update(ct, body);

    uint8_t lengths[16] = {0};
    detail::Ghash::store_be64(static_cast<uint64_t>(aad.size()) * 8u, lengths);
    detail::Ghash::store_be64(static_cast<uint64_t>(body) * 8u, lengths + 8);
    ghash.update(lengths, sizeof(lengths));

    uint8_t computed[16];
    ghash.finish(computed);
    for (size_t i = 0u; i < kTagSize; ++i) computed[i] ^= tag_mask[i];

    uint8_t difference = 0u;
    for (size_t i = 0u; i < kTagSize; ++i) {
        difference = static_cast<uint8_t>(difference | (computed[i] ^ tag[i]));
    }
    if (difference != 0u) return false;

    plaintext->clear();
    plaintext->reserve(body);
    uint8_t keystream[16];
    for (size_t offset = 0u; offset < body; offset += 16u) {
        // Increment the 32-bit big-endian block counter.
        for (int i = 15; i >= 12; --i) {
            if (++counter[i] != 0u) break;
        }
        aes.encrypt_block(counter, keystream);
        const size_t chunk = (body - offset) < 16u ? (body - offset) : 16u;
        for (size_t i = 0u; i < chunk; ++i) {
            plaintext->push_back(static_cast<char>(ct[offset + i] ^ keystream[i]));
        }
    }
    return true;
}

} // namespace opg3d_crypto
