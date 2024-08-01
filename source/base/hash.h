#ifndef HASH_H
#define HASH_H

#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <utility>

// refactor: deobfuscate rapidhash code, possibly further modernize (std::span)?
// add more std types to hash, completely remove std::hash fallback?

#if defined(_MSC_VER)
  #include <intrin.h>
#if defined(_M_X64) && !defined(_M_ARM64EC)
    #pragma intrinsic(_umul128)
#endif
#endif

namespace bite {
    namespace rapidhash {
        /*
         * hash - Very fast, high quality, platform-independent hashing algorithm.
         * Copyright (C) 2024 Nicolas De Carli
         *
         * Based on 'wyhash', by Wang Yi <godspeed_china@yeah.net>
         *
         * BSD 2-Clause License (https://www.opensource.org/licenses/bsd-license.php)
         *
         * Redistribution and use in source and binary forms, with or without
         * modification, are permitted provided that the following conditions are
         * met:
         *
         *    * Redistributions of source code must retain the above copyright
         *      notice, this list of conditions and the following disclaimer.
         *    * Redistributions in binary form must reproduce the above
         *      copyright notice, this list of conditions and the following disclaimer
         *      in the documentation and/or other materials provided with the
         *      distribution.
         *
         * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
         * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
         * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
         * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
         * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
         * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
         * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
         * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
         * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
         * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
         * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
         *
         * You can contact the author at:
         *   - hash source repository: https://github.com/Nicoshev/hash
         */

        /*
         *  64*64 -> 128bit multiply function.
         *
         *  @param A  Address of 64-bit number.
         *  @param B  Address of 64-bit number.
         *
         *  Calculates 128-bit C = *A * *B.
         *
         *  Overwrites A contents with C's low 64 bits.
         *  Overwrites B contents with C's high 64 bits.
         */
        namespace detail {
            inline void multiply_128(std::uint64_t* A, std::uint64_t* B) noexcept {
                #if defined(__SIZEOF_INT128__)
                __uint128_t r = *A;
                r *= *B;
                *A = static_cast<std::uint64_t>(r);
                *B = static_cast<std::uint64_t>(r >> 64);
                #elif defined(_MSC_VER) && (defined(_WIN64) || defined(_M_HYBRID_CHPE_ARM64))
                #if defined(_M_X64)
                *A = _umul128(*A, *B, B);
                #else
                std::uint64_t c = __umulh(*A, *B);
                *A = *A * *B;
                *B = c;
                #endif
                #else
                std::uint64_t ha = *A >> 32, hb = *B >> 32, la = static_cast<std::uint32_t>(*A), lb = static_cast<std::uint32_t>(*B), hi, lo;
                std::uint64_t rh = ha * hb, rm0 = ha * lb, rm1 = hb * la, rl = la * lb, t = rl + (rm0 << 32), c = t < rl;
                lo = t + (rm1 << 32);
                c += lo < t;
                hi = rh + (rm0 >> 32) + (rm1 >> 32) + c;
                *A = lo;
                *B = hi;
                #endif
            }

            /*
             *  Multiply and xor mix function.
             *
             *  @param A  64-bit number.
             *  @param B  64-bit number.
             *
             *  Calculates 128-bit C = A * B.
             *  Returns 64-bit xor between high and low 64 bits of C.
             */
            inline std::uint64_t mix(std::uint64_t A, std::uint64_t B) noexcept {
                multiply_128(&A, &B);
                return A ^ B;
            }


            inline std::uint64_t read64(const std::uint8_t* p) noexcept {
                std::uint64_t v;
                std::memcpy(&v, p, sizeof(std::uint64_t));
                return v;
            }

            inline uint64_t read32(const uint8_t* p) noexcept {
                std::uint32_t v;
                std::memcpy(&v, p, sizeof(uint32_t));
                return v;
            }

            /*
             *  Reads and combines 3 bytes of input.
             *
             *  @param p  Buffer to read from.
             *  @param k  Length of @p, in bytes.
             *
             *  Always reads and combines 3 bytes from memory.
             *  Guarantees to read each buffer position at least once.
             *
             *  Returns a 64-bit value containing all three bytes read.
             */
            inline std::uint64_t read_small(const std::uint8_t* p, std::size_t k) noexcept {
                return (static_cast<uint64_t>(p[0]) << 56) | (static_cast<uint64_t>(p[k >> 1]) << 32) | p[k - 1];
            }

            /*
             *  hash main function.
             *
             *  @param key     Buffer to be hashed.
             *  @param len     @key length, in bytes.
             *  @param seed    64-bit seed used to alter the hash result predictably.
             *
             *  Returns a 64-bit hash.
             */
            inline std::uint64_t rapidhash_internal(
                const void* key,
                const std::size_t len,
                std::uint64_t seed
            ) noexcept {
                constexpr static std::uint64_t secret[3] = {
                        0x2d358dccaa6c78a5ull,
                        0x8bb84b93962eacc9ull,
                        0x4b33a62ed433d4a3ull
                    };

                const auto* p = static_cast<const uint8_t*>(key);
                seed ^= mix(seed ^ secret[0], secret[1]) ^ len;
                uint64_t a, b;
                if (len <= 16) [[likely]] {
                    if (len >= 4) [[likely]] {
                        const uint8_t* plast = p + len - 4;
                        a = read32(p) << 32 | read32(plast);
                        const uint64_t delta = ((len & 24) >> (len >> 3));
                        b = read32(p + delta) << 32 | read32(plast - delta);
                    } else if (len > 0) [[likely]] {
                        a = read_small(p, len);
                        b = 0;
                    } else
                        a = b = 0;
                } else {
                    size_t i = len;
                    if (i > 48) [[unlikely]] {
                        uint64_t see1 = seed, see2 = seed;
                        while (i >= 96) [[likely]] {
                            seed = mix(read64(p) ^ secret[0], read64(p + 8) ^ seed);
                            see1 = mix(read64(p + 16) ^ secret[1], read64(p + 24) ^ see1);
                            see2 = mix(read64(p + 32) ^ secret[2], read64(p + 40) ^ see2);
                            seed = mix(read64(p + 48) ^ secret[0], read64(p + 56) ^ seed);
                            see1 = mix(read64(p + 64) ^ secret[1], read64(p + 72) ^ see1);
                            see2 = mix(read64(p + 80) ^ secret[2], read64(p + 88) ^ see2);
                            p += 96;
                            i -= 96;
                        }
                        if (i >= 48) [[unlikely]] {
                            seed = mix(read64(p) ^ secret[0], read64(p + 8) ^ seed);
                            see1 = mix(read64(p + 16) ^ secret[1], read64(p + 24) ^ see1);
                            see2 = mix(read64(p + 32) ^ secret[2], read64(p + 40) ^ see2);
                            p += 48;
                            i -= 48;
                        }
                        seed ^= see1 ^ see2;
                    }
                    if (i > 16) {
                        seed = mix(read64(p) ^ secret[2], read64(p + 8) ^ seed ^ secret[1]);
                        if (i > 32)
                            seed = mix(read64(p + 16) ^ secret[2], read64(p + 24) ^ seed);
                    }
                    a = read64(p + i - 16);
                    b = read64(p + i - 8);
                }
                a ^= secret[1];
                b ^= seed;
                multiply_128(&a, &b);
                return mix(a ^ secret[0] ^ len, b ^ secret[1]);
            }
        }

        /*
         *  hash default seeded hash function.
         *
         *  @param key     Buffer to be hashed.
         *  @param len     @key length, in bytes.
         *  @param seed    64-bit seed used to alter the hash result predictably.
         *
         *  Calls rapidhash_internal using provided parameters and default secrets.
         *
         *  Returns a 64-bit hash.
         */
        inline std::uint64_t hash_with_seed(const void* key, const std::size_t len, const std::uint64_t seed) noexcept {
            return detail::rapidhash_internal(key, len, seed);
        }

        /*
         *  hash default hash function.
         *
         *  @param key     Buffer to be hashed.
         *  @param len     @key length, in bytes.
         *
         *  Calls hash_with_seed using provided parameters and the default seed.
         *
         *  Returns a 64-bit hash.
         */
        inline std::uint64_t hash(const void* key, const size_t len) noexcept {
            return hash_with_seed(key, len, 0xbdd89aa982704029ull);
        }

        inline std::uint64_t hash(std::uint64_t x) noexcept {
            return detail::mix(x, 0x9E3779B97F4A7C15ull);
        }
    }  // namespace rapidhash

    // Hash integration from: https://github.com/martinus/unordered_dense/blob/main/include/bite/unordered_dense.h

    template <typename T, typename Enable = void>
    struct hash {
        auto operator()(
            T const& obj
        ) const noexcept(noexcept(std::declval<std::hash<T>>().operator()(std::declval<T const&>()))) -> uint64_t {
            return std::hash<T> {}(obj);
        }
    };

    template <typename CharT>
    struct hash<std::basic_string<CharT>> {
        using is_avalanching = void;

        auto operator()(std::basic_string<CharT> const& str) const noexcept -> uint64_t {
            return rapidhash::hash(str.data(), sizeof(CharT) * str.size());
        }
    };

    template <typename CharT>
    struct hash<std::basic_string_view<CharT>> {
        using is_avalanching = void;

        auto operator()(std::basic_string_view<CharT> const& sv) const noexcept -> uint64_t {
            return rapidhash::hash(sv.data(), sizeof(CharT) * sv.size());
        }
    };

    template <class T>
    struct hash<T*> {
        using is_avalanching = void;

        auto operator()(T* ptr) const noexcept -> uint64_t {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return rapidhash::hash(reinterpret_cast<uintptr_t>(ptr));
        }
    };

    template <class T>
    struct hash<std::unique_ptr<T>> {
        using is_avalanching = void;

        auto operator()(std::unique_ptr<T> const& ptr) const noexcept -> uint64_t {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return rapidhash::hash(reinterpret_cast<uintptr_t>(ptr.get()));
        }
    };

    template <class T>
    struct hash<std::shared_ptr<T>> {
        using is_avalanching = void;

        auto operator()(std::shared_ptr<T> const& ptr) const noexcept -> uint64_t {
            // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
            return rapidhash::hash(reinterpret_cast<uintptr_t>(ptr.get()));
        }
    };

    template <typename Enum>
    struct hash<Enum, std::enable_if_t<std::is_enum_v<Enum>>> {
        using is_avalanching = void;

        auto operator()(Enum e) const noexcept -> uint64_t {
            using underlying = std::underlying_type_t<Enum>;
            return rapidhash::hash(static_cast<underlying>(e));
        }
    };

    template <typename... Args>
    struct tuple_hash_helper {
        // Converts the value into 64bit. If it is an integral type, just cast it. Mixing is doing the rest.
        // If it isn't an integral we need to hash it.
        template <typename Arg>
        [[nodiscard]] constexpr static auto to64(Arg const& arg) -> uint64_t {
            if constexpr (std::is_integral_v<Arg> || std::is_enum_v<Arg>) {
                return static_cast<uint64_t>(arg);
            } else {
                return hash<Arg> {}(arg);
            }
        }

        [[nodiscard]] static auto mix64(uint64_t state, uint64_t v) -> uint64_t {
            return rapidhash::detail::mix(state + v, uint64_t { 0x9ddfea08eb382d69 });
        }

        // Creates a buffer that holds all the data from each element of the tuple. If possible we memcpy the data directly. If
        // not, we hash the object and use this for the array. Size of the array is known at compile time, and memcpy is optimized
        // away, so filling the buffer is highly efficient. Finally, call wyhash with this buffer.
        template <typename T, std::size_t... Idx>
        [[nodiscard]] static auto calc_hash(T const& t, std::index_sequence<Idx...>) noexcept -> uint64_t {
            auto h = uint64_t {};
            ((h = mix64(h, to64(std::get<Idx>(t)))), ...);
            return h;
        }
    };

    template <typename... Args>
    struct hash<std::tuple<Args...>> : tuple_hash_helper<Args...> {
        using is_avalanching = void;

        auto operator()(std::tuple<Args...> const& t) const noexcept -> uint64_t {
            return tuple_hash_helper<Args...>::calc_hash(t, std::index_sequence_for<Args...> {});
        }
    };

    template <typename A, typename B>
    struct hash<std::pair<A, B>> : tuple_hash_helper<A, B> {
        using is_avalanching = void;

        auto operator()(std::pair<A, B> const& t) const noexcept -> uint64_t {
            return tuple_hash_helper<A, B>::calc_hash(t, std::index_sequence_for<A, B> {});
        }
    };

    // NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
    #    define BITE_HASH_STATICCAST(T)                    \
        template <>                                                      \
        struct hash<T> {                                                 \
            using is_avalanching = void;                                 \
            auto operator()(T const& obj) const noexcept -> uint64_t {   \
                return rapidhash::hash(static_cast<uint64_t>(obj)); \
            }                                                            \
        }

    #    if defined(__GNUC__) && !defined(__clang__)
    #        pragma GCC diagnostic push
    #        pragma GCC diagnostic ignored "-Wuseless-cast"
    #    endif
    // see https://en.cppreference.com/w/cpp/utility/hash
    BITE_HASH_STATICCAST(bool);

    BITE_HASH_STATICCAST(char);

    BITE_HASH_STATICCAST(signed char);

    BITE_HASH_STATICCAST(unsigned char);

    BITE_HASH_STATICCAST(char8_t);

    BITE_HASH_STATICCAST(char16_t);

    BITE_HASH_STATICCAST(char32_t);

    BITE_HASH_STATICCAST(wchar_t);

    BITE_HASH_STATICCAST(short);

    BITE_HASH_STATICCAST(unsigned short);

    BITE_HASH_STATICCAST(int);

    BITE_HASH_STATICCAST(unsigned int);

    BITE_HASH_STATICCAST(long);

    BITE_HASH_STATICCAST(long long);

    BITE_HASH_STATICCAST(unsigned long);

    BITE_HASH_STATICCAST(unsigned long long);

    #    if defined(__GNUC__) && !defined(__clang__)
    #        pragma GCC diagnostic pop
    #    endif
} // namespace bite


#endif
