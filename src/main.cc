#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

// constant used while processing each chunk
constexpr std::array<uint32_t, 80> K = []() -> std::array<uint32_t, 80> {
        std::array<uint32_t, 80> k{};

        for (uint32_t i = 0; i < 80; i++) {
                if (i < 20) k[i] = 0x5A827999;
                else if (i < 40) k[i] = 0x6ED9EBA1;
                else if (i < 60) k[i] = 0x8F1BBCDC;
                else k[i] = 0xCA62C1D6;
        }

        return k;
}();

constexpr int CHUNK_SIZE_BYTES = 64;  // DO NOT CHANGE: SHA-1 operates on 64-byte (512-bit) chunks
constexpr int BUF_SIZE_BYTES   = 32'768;  // file read buffer size, must be at least 64

using BUF = std::array<uint8_t, BUF_SIZE_BYTES>;

uint64_t FILE_SIZE_BYTES{};

std::array<uint32_t, 5> H = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};  // state

void process_chunk(const BUF& buf, const int offset) {
        std::array<uint32_t, 80> M{};

#pragma GCC unroll 16
        for (uint32_t j = 0; j < 16; j++) {
                uint32_t i = 4 * j + offset;
                M[j]       = (static_cast<uint32_t>(buf[i]) << 24) |
                             (static_cast<uint32_t>(buf[i + 1]) << 16) |
                             (static_cast<uint32_t>(buf[i + 2]) << 8) |
                             (static_cast<uint32_t>(buf[i + 3]));
        }

#pragma GCC unroll 64
        for (uint32_t j = 16; j < 80; j++)
                M[j] = std::rotl(M[j - 3] ^ M[j - 8] ^ M[j - 14] ^ M[j - 16], 1);

        uint32_t A = H[0];
        uint32_t B = H[1];
        uint32_t C = H[2];
        uint32_t D = H[3];
        uint32_t E = H[4];

#pragma GCC unroll 80
        for (int i = 0; i < 80; i++) {
                uint32_t F{};

                if (i < 20) F = D ^ (B & (C ^ D));
                else if (i < 40) F = B ^ C ^ D;
                else if (i < 60) F = (B & C) | ((B | C) & D);
                else F = B ^ C ^ D;

                uint32_t t = std::rotl(A, 5) + F + E + K[i] + M[i];
                E          = D;
                D          = C;
                C          = std::rotl(B, 30);
                B          = A;
                A          = t;
        }

        H[0] += A;
        H[1] += B;
        H[2] += C;
        H[3] += D;
        H[4] += E;
}

auto process_input(const std::string& file_name) -> int {
        std::ifstream file(file_name, std::ios::binary);

        if (!file) {
                std::cerr << "Error: Unable to open file \"" << file_name << "\".\n";

                return 1;
        }

        BUF buf{};

        int pad_start_idx{};
        int offset{};

        while (true) {
                file.read(reinterpret_cast<char*>(buf.data()), BUF_SIZE_BYTES);
                auto bytes_read  = static_cast<int>(file.gcount());
                FILE_SIZE_BYTES += bytes_read;

                offset = 0;

                // process all except final chunk
                for (; offset < bytes_read - CHUNK_SIZE_BYTES; offset += CHUNK_SIZE_BYTES)
                        process_chunk(buf, offset);

                // this is not the final buf, process final chunk as usual
                if (bytes_read == BUF_SIZE_BYTES) {
                        process_chunk(buf, offset);
                        continue;
                }

                // this is the final buf

                pad_start_idx = bytes_read - offset;

                // we have a full chunk to process before padding
                if (pad_start_idx == CHUNK_SIZE_BYTES) {
                        process_chunk(buf, offset);
                        pad_start_idx = 0;
                }

                break;
        }

        // move bytes remaining to front of buffer for simpler processing
        for (int i = 0; i < pad_start_idx; i++) buf[i] = buf[i + offset];

        buf[pad_start_idx++] = 0x80;
        int pad_offset       = 0;

        while (pad_start_idx != CHUNK_SIZE_BYTES - 8) {
                if (pad_start_idx == CHUNK_SIZE_BYTES) {
                        process_chunk(buf, pad_offset);
                        pad_offset    += CHUNK_SIZE_BYTES;
                        pad_start_idx  = 0;
                }

                buf[pad_start_idx + pad_offset] = 0x00;
                pad_start_idx++;
        }

        uint64_t file_size_bits = FILE_SIZE_BYTES * 8;

        for (int i = 0, s = 56; i < 8; i++, s -= 8)
                buf[pad_start_idx + pad_offset + i] = static_cast<uint8_t>(file_size_bits >> s);

        process_chunk(buf, pad_offset);

        file.close();

        return 0;
}

auto get_output() -> std::string {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');

        for (const auto& word : H) oss << std::setw(8) << word;

        return oss.str();
}

auto main(int argc, char** argv) -> int {
        if (argc <= 1) {
                std::cerr << "Input file must be specified.\n";

                return 1;
        }

        std::string file_name = argv[1];
        int         retval    = process_input(file_name);

        if (retval == 0) std::cout << get_output() << "\n";

        return retval;
}
