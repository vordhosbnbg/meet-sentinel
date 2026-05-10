#include "google/pkce.h"

#include <array>
#include <cstdint>
#include <random>
#include <span>
#include <string>

namespace meet_sentinel::google {
namespace {

using Sha256Digest = std::array<std::uint8_t, 32>;

constexpr std::array<std::uint32_t, 64> kSha256{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

std::uint32_t rotate_right(std::uint32_t value, int bits) {
    return (value >> bits) | (value << (32 - bits));
}

std::uint8_t byte_at(std::string_view bytes, std::size_t offset) {
    return static_cast<std::uint8_t>(bytes[offset]);
}

std::uint32_t read_big_endian_word(std::string_view bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(byte_at(bytes, offset)) << 24U) |
           (static_cast<std::uint32_t>(byte_at(bytes, offset + 1)) << 16U) |
           (static_cast<std::uint32_t>(byte_at(bytes, offset + 2)) << 8U) |
           static_cast<std::uint32_t>(byte_at(bytes, offset + 3));
}

void append_big_endian_u64(std::string& bytes, std::uint64_t value) {
    for(int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<char>((value >> shift) & 0xFFU));
    }
}

Sha256Digest sha256(std::string_view input) {
    std::array<std::uint32_t, 8> state{
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
    };

    std::string message{input};
    const std::uint64_t bit_length = static_cast<std::uint64_t>(message.size()) * 8U;
    message.push_back(static_cast<char>(0x80));
    while((message.size() % 64U) != 56U) {
        message.push_back('\0');
    }
    append_big_endian_u64(message, bit_length);

    for(std::size_t chunk = 0; chunk < message.size(); chunk += 64U) {
        std::array<std::uint32_t, 64> words{};
        const std::string_view bytes{message.data() + chunk, 64};
        for(std::size_t i = 0; i < 16; ++i) {
            words[i] = read_big_endian_word(bytes, i * 4U);
        }
        for(std::size_t i = 16; i < 64; ++i) {
            const std::uint32_t s0 =
                rotate_right(words[i - 15], 7) ^ rotate_right(words[i - 15], 18) ^ (words[i - 15] >> 3U);
            const std::uint32_t s1 =
                rotate_right(words[i - 2], 17) ^ rotate_right(words[i - 2], 19) ^ (words[i - 2] >> 10U);
            words[i] = words[i - 16] + s0 + words[i - 7] + s1;
        }

        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        std::uint32_t f = state[5];
        std::uint32_t g = state[6];
        std::uint32_t h = state[7];

        for(std::size_t i = 0; i < 64; ++i) {
            const std::uint32_t s1 = rotate_right(e, 6) ^ rotate_right(e, 11) ^ rotate_right(e, 25);
            const std::uint32_t choose = (e & f) ^ ((~e) & g);
            const std::uint32_t temp1 = h + s1 + choose + kSha256[i] + words[i];
            const std::uint32_t s0 = rotate_right(a, 2) ^ rotate_right(a, 13) ^ rotate_right(a, 22);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temp2 = s0 + majority;

            h = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    Sha256Digest digest{};
    for(std::size_t i = 0; i < state.size(); ++i) {
        digest[i * 4U] = static_cast<std::uint8_t>((state[i] >> 24U) & 0xFFU);
        digest[i * 4U + 1U] = static_cast<std::uint8_t>((state[i] >> 16U) & 0xFFU);
        digest[i * 4U + 2U] = static_cast<std::uint8_t>((state[i] >> 8U) & 0xFFU);
        digest[i * 4U + 3U] = static_cast<std::uint8_t>(state[i] & 0xFFU);
    }

    return digest;
}

std::string base64url_encode(std::span<const std::uint8_t> bytes) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string encoded;
    std::uint32_t buffer = 0;
    int bit_count = 0;
    for(const std::uint8_t byte : bytes) {
        buffer = (buffer << 8U) | byte;
        bit_count += 8;
        while(bit_count >= 6) {
            bit_count -= 6;
            encoded.push_back(alphabet[(buffer >> bit_count) & 0x3FU]);
        }
    }

    if(bit_count > 0) {
        encoded.push_back(alphabet[(buffer << (6 - bit_count)) & 0x3FU]);
    }

    return encoded;
}

std::array<std::uint8_t, 32> random_bytes() {
    std::array<std::uint8_t, 32> bytes{};
    std::random_device random;
    std::uniform_int_distribution<int> byte_distribution{0, 255};
    for(auto& byte : bytes) {
        byte = static_cast<std::uint8_t>(byte_distribution(random));
    }
    return bytes;
}

} // namespace

PkceChallenge generate_pkce_challenge() {
    const auto verifier_bytes = random_bytes();
    PkceChallenge challenge;
    challenge.verifier = base64url_encode(verifier_bytes);
    challenge.challenge = pkce_challenge_for_verifier(challenge.verifier);
    return challenge;
}

std::string pkce_challenge_for_verifier(std::string_view verifier) {
    const auto digest = sha256(verifier);
    return base64url_encode(digest);
}

} // namespace meet_sentinel::google
