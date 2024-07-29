#ifndef UNORDERED_DENSE_H
#define UNORDERED_DENSE_H

// Modified: https://github.com/martinus/unordered_dense/blob/main/include/bite/unordered_dense.h
// Removed useless macros, modernized, integrated into existing bite macros and minor reformatted.
// Replaced hash
// refactor: investigate useless includes

#include <array>            // for array
#include <cstdint>          // for uint64_t, uint32_t, uint8_t, UINT64_C
#include <cstring>          // for size_t, memcpy, memset
#include <functional>       // for equal_to, hash
#include <initializer_list> // for initializer_list
#include <iterator>         // for pair, distance
#include <limits>           // for numeric_limits
#include <memory>           // for allocator, allocator_traits, shared_ptr
#include <optional>         // for optional
#include <stdexcept>        // for out_of_range
#include <string>           // for basic_string
#include <string_view>      // for basic_string_view, hash
#include <tuple>            // for forward_as_tuple
#include <type_traits>      // for enable_if_t, declval, conditional_t, ena...
#include <utility>          // for forward, exchange, pair, as_const, piece...
#include <vector>           // for vector
#include <memory_resource>  // for polymorphic_allocator

#include "common.h"
#include "hash.h"

namespace bite::unordered_dense {
    namespace detail {
        // make sure this is not inlined as it is slow and dramatically enlarges code, thus making other
        // inlinings more difficult. Throws are also generally the slow path.
        [[noreturn]] BITE_NOINLINE inline void on_error_key_not_found() {
            throw std::out_of_range("bite::unordered_dense::map::at(): key not found");
        }

        [[noreturn]] BITE_NOINLINE inline void on_error_bucket_overflow() {
            throw std::overflow_error("bite::unordered_dense: reached max bucket size, cannot increase size");
        }

        [[noreturn]] BITE_NOINLINE inline void on_error_too_many_elements() {
            throw std::out_of_range("bite::unordered_dense::map::replace(): too many elements");
        }
    } // namespace detail

    // bucket_type //////////////////////////////////////////////////////////

    namespace bucket_type {
        struct standard {
            static constexpr uint32_t dist_inc = 1U << 8U; // skip 1 byte fingerprint
            static constexpr uint32_t fingerprint_mask = dist_inc - 1; // mask for 1 byte of fingerprint

            uint32_t m_dist_and_fingerprint;
            // upper 3 byte: distance to original bucket. lower byte: fingerprint from hash
            uint32_t m_value_idx; // index into the m_values vector.
        };

        BITE_DENSE_PACK(
            struct big { static constexpr uint32_t dist_inc = 1U << 8U; // skip 1 byte fingerprint
            static constexpr uint32_t fingerprint_mask = dist_inc - 1; // mask for 1 byte of fingerprint

            uint32_t m_dist_and_fingerprint;
            // upper 3 byte: distance to original bucket. lower byte: fingerprint from hash
            size_t m_value_idx; // index into the m_values vector.
            }
        );
    } // namespace bucket_type

    namespace detail {
        struct nonesuch {};

        template <class Default, class AlwaysVoid, template <class...> class Op, class... Args>
        struct detector {
            using value_t = std::false_type;
            using type = Default;
        };

        template <class Default, template <class...> class Op, class... Args>
        struct detector<Default, std::void_t<Op<Args...>>, Op, Args...> {
            using value_t = std::true_type;
            using type = Op<Args...>;
        };

        template <template <class...> class Op, class... Args>
        using is_detected = typename detector<nonesuch, void, Op, Args...>::value_t;

        template <template <class...> class Op, class... Args>
        constexpr bool is_detected_v = is_detected<Op, Args...>::value;

        template <typename T>
        using detect_avalanching = typename T::is_avalanching;

        template <typename T>
        using detect_is_transparent = typename T::is_transparent;

        template <typename T>
        using detect_iterator = typename T::iterator;

        template <typename T>
        using detect_reserve = decltype(std::declval<T&>().reserve(size_t {}));

        // enable_if helpers

        template <typename Mapped>
        constexpr bool is_map_v = !std::is_void_v<Mapped>;

        // clang-format off
        template <typename Hash, typename KeyEqual>
        constexpr bool is_transparent_v = is_detected_v<detect_is_transparent, Hash> && is_detected_v<
            detect_is_transparent, KeyEqual>;
        // clang-format on

        template <typename From, typename To1, typename To2>
        constexpr bool is_neither_convertible_v = !std::is_convertible_v<From, To1> && !std::is_convertible_v<
            From, To2>;

        template <typename T>
        constexpr bool has_reserve = is_detected_v<detect_reserve, T>;

        // base type for map has mapped_type
        template <class T>
        struct base_table_type_map {
            using mapped_type = T;
        };

        // base type for set doesn't have mapped_type
        struct base_table_type_set {};
    } // namespace detail

    // Very much like std::deque, but faster for indexing (in most cases). As of now this doesn't implement the full std::vector
    // API, but merely what's necessary to work as an underlying container for bite::unordered_dense::{map, set}.
    // It allocates blocks of equal size and puts them into the m_blocks vector. That means it can grow simply by adding a new
    // block to the back of m_blocks, and doesn't double its size like an std::vector. The disadvantage is that memory is not
    // linear and thus there is one more indirection necessary for indexing.
    template <typename T, typename Allocator = std::allocator<T>, size_t MaxSegmentSizeBytes = 4096>
    class segmented_vector {
        template <bool IsConst>
        class iter_t;

    public:
        using allocator_type = Allocator;
        using pointer = typename std::allocator_traits<allocator_type>::pointer;
        using const_pointer = typename std::allocator_traits<allocator_type>::const_pointer;
        using difference_type = typename std::allocator_traits<allocator_type>::difference_type;
        using value_type = T;
        using size_type = std::size_t;
        using reference = T&;
        using const_reference = T const&;
        using iterator = iter_t<false>;
        using const_iterator = iter_t<true>;

    private:
        using vec_alloc = typename std::allocator_traits<Allocator>::template rebind_alloc<pointer>;
        std::vector<pointer, vec_alloc> m_blocks {};
        size_t m_size {};

        // Calculates the maximum number for x in  (s << x) <= max_val
        static constexpr auto num_bits_closest(size_t max_val, size_t s) -> size_t {
            auto f = size_t { 0 };
            while (s << (f + 1) <= max_val) {
                ++f;
            }
            return f;
        }

        using self_t = segmented_vector<T, Allocator, MaxSegmentSizeBytes>;
        static constexpr auto num_bits = num_bits_closest(MaxSegmentSizeBytes, sizeof(T));
        static constexpr auto num_elements_in_block = 1U << num_bits;
        static constexpr auto mask = num_elements_in_block - 1U;

        /**
         * Iterator class doubles as const_iterator and iterator
         */
        template <bool IsConst>
        class iter_t {
            using ptr_t = std::conditional_t<IsConst, const_pointer const*, pointer*>;
            ptr_t m_data {};
            size_t m_idx {};

            template <bool B>
            friend class iter_t;

        public:
            using difference_type = difference_type;
            using value_type = T;
            using reference = std::conditional_t<IsConst, value_type const&, value_type&>;
            using pointer = std::conditional_t<IsConst, const_pointer, pointer>;
            using iterator_category = std::forward_iterator_tag;

            iter_t() noexcept = default;

            template <bool OtherIsConst, typename = std::enable_if_t<IsConst && !OtherIsConst>>
            // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
            constexpr iter_t(iter_t<OtherIsConst> const& other) noexcept : m_data(other.m_data),
                                                                           m_idx(other.m_idx) {}

            constexpr iter_t(ptr_t data, size_t idx) noexcept : m_data(data),
                                                                m_idx(idx) {}

            template <bool OtherIsConst, typename = std::enable_if_t<IsConst && !OtherIsConst>>
            constexpr auto operator =(iter_t<OtherIsConst> const& other) noexcept -> iter_t& {
                m_data = other.m_data;
                m_idx = other.m_idx;
                return *this;
            }

            constexpr auto operator++() noexcept -> iter_t& {
                ++m_idx;
                return *this;
            }

            constexpr auto operator+(difference_type diff) noexcept -> iter_t {
                return { m_data, static_cast<size_t>(static_cast<difference_type>(m_idx) + diff) };
            }

            template <bool OtherIsConst>
            constexpr auto operator-(iter_t<OtherIsConst> const& other) noexcept -> difference_type {
                return static_cast<difference_type>(m_idx) - static_cast<difference_type>(other.m_idx);
            }

            constexpr auto operator*() const noexcept -> reference {
                return m_data[m_idx >> num_bits][m_idx & mask];
            }

            constexpr auto operator->() const noexcept -> pointer {
                return &m_data[m_idx >> num_bits][m_idx & mask];
            }

            template <bool O>
            constexpr auto operator==(iter_t<O> const& o) const noexcept -> bool {
                return m_idx == o.m_idx;
            }

            template <bool O>
            constexpr auto operator!=(iter_t<O> const& o) const noexcept -> bool {
                return !(*this == o);
            }
        };

        // slow path: need to allocate a new segment every once in a while
        void increase_capacity() {
            auto ba = Allocator(m_blocks.get_allocator());
            pointer block = std::allocator_traits<Allocator>::allocate(ba, num_elements_in_block);
            m_blocks.push_back(block);
        }

        // Moves everything from other
        void append_everything_from(segmented_vector&& other) {
            reserve(size() + other.size());
            for (auto&& o : other) {
                emplace_back(std::move(o));
            }
        }

        // Copies everything from other
        void append_everything_from(segmented_vector const& other) {
            reserve(size() + other.size());
            for (auto const& o : other) {
                emplace_back(o);
            }
        }

        void dealloc() {
            auto ba = Allocator(m_blocks.get_allocator());
            for (auto ptr : m_blocks) {
                std::allocator_traits<Allocator>::deallocate(ba, ptr, num_elements_in_block);
            }
        }

        [[nodiscard]] static constexpr auto calc_num_blocks_for_capacity(size_t capacity) {
            return (capacity + num_elements_in_block - 1U) / num_elements_in_block;
        }

    public:
        segmented_vector() = default;

        // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
        segmented_vector(Allocator alloc) : m_blocks(vec_alloc(alloc)) {}

        segmented_vector(segmented_vector&& other, Allocator alloc) : segmented_vector(alloc) {
            *this = std::move(other);
        }

        segmented_vector(segmented_vector const& other, Allocator alloc) : m_blocks(vec_alloc(alloc)) {
            append_everything_from(other);
        }

        segmented_vector(segmented_vector&& other) noexcept : segmented_vector(std::move(other), get_allocator()) {}

        segmented_vector(segmented_vector const& other) {
            append_everything_from(other);
        }

        auto operator=(segmented_vector const& other) -> segmented_vector& {
            if (this == &other) {
                return *this;
            }
            clear();
            append_everything_from(other);
            return *this;
        }

        auto operator=(segmented_vector&& other) noexcept -> segmented_vector& {
            clear();
            dealloc();
            if (other.get_allocator() == get_allocator()) {
                m_blocks = std::move(other.m_blocks);
                m_size = std::exchange(other.m_size, {});
            } else {
                // make sure to construct with other's allocator!
                m_blocks = std::vector<pointer, vec_alloc>(vec_alloc(other.get_allocator()));
                append_everything_from(std::move(other));
            }
            return *this;
        }

        ~segmented_vector() {
            clear();
            dealloc();
        }

        [[nodiscard]] constexpr auto size() const -> size_t {
            return m_size;
        }

        [[nodiscard]] constexpr auto capacity() const -> size_t {
            return m_blocks.size() * num_elements_in_block;
        }

        // Indexing is highly performance critical
        [[nodiscard]] constexpr auto operator[](size_t i) const noexcept -> T const& {
            return m_blocks[i >> num_bits][i & mask];
        }

        [[nodiscard]] constexpr auto operator[](size_t i) noexcept -> T& {
            return m_blocks[i >> num_bits][i & mask];
        }

        [[nodiscard]] constexpr auto begin() -> iterator {
            return { m_blocks.data(), 0U };
        }

        [[nodiscard]] constexpr auto begin() const -> const_iterator {
            return { m_blocks.data(), 0U };
        }

        [[nodiscard]] constexpr auto cbegin() const -> const_iterator {
            return { m_blocks.data(), 0U };
        }

        [[nodiscard]] constexpr auto end() -> iterator {
            return { m_blocks.data(), m_size };
        }

        [[nodiscard]] constexpr auto end() const -> const_iterator {
            return { m_blocks.data(), m_size };
        }

        [[nodiscard]] constexpr auto cend() const -> const_iterator {
            return { m_blocks.data(), m_size };
        }

        [[nodiscard]] constexpr auto back() -> reference {
            return operator[](m_size - 1);
        }

        [[nodiscard]] constexpr auto back() const -> const_reference {
            return operator[](m_size - 1);
        }

        void pop_back() {
            back().~T();
            --m_size;
        }

        [[nodiscard]] auto empty() const {
            return 0 == m_size;
        }

        void reserve(const size_t new_capacity) {
            m_blocks.reserve(calc_num_blocks_for_capacity(new_capacity));
            while (new_capacity > capacity()) {
                increase_capacity();
            }
        }

        [[nodiscard]] auto get_allocator() const -> allocator_type {
            return allocator_type { m_blocks.get_allocator() };
        }

        template <class... Args>
        auto emplace_back(Args&&... args) -> reference {
            if (m_size == capacity()) {
                increase_capacity();
            }
            auto* ptr = static_cast<void*>(&operator[](m_size));
            auto& ref = *new(ptr) T(std::forward<Args>(args)...);
            ++m_size;
            return ref;
        }

        void clear() {
            if constexpr (!std::is_trivially_destructible_v<T>) {
                for (size_t i = 0, s = size(); i < s; ++i) {
                    operator[](i).~T();
                }
            }
            m_size = 0;
        }

        void shrink_to_fit() {
            auto ba = Allocator(m_blocks.get_allocator());
            auto num_blocks_required = calc_num_blocks_for_capacity(m_size);
            while (m_blocks.size() > num_blocks_required) {
                std::allocator_traits<Allocator>::deallocate(ba, m_blocks.back(), num_elements_in_block);
                m_blocks.pop_back();
            }
            m_blocks.shrink_to_fit();
        }
    };

    namespace detail {
        // This is it, the table. Doubles as map and set, and uses `void` for T when its used as a set.
        template <class Key, class T, // when void, treat it as a set.
                  class Hash, class KeyEqual, class AllocatorOrContainer, class Bucket, bool IsSegmented>
        class table : public std::conditional_t<is_map_v<T>, base_table_type_map<T>, base_table_type_set> {
            using underlying_value_type = std::conditional_t<is_map_v<T>, std::pair<Key, T>, Key>;
            using underlying_container_type = std::conditional_t<IsSegmented,
                                                                 segmented_vector<
                                                                     underlying_value_type, AllocatorOrContainer>,
                                                                 std::vector<
                                                                     underlying_value_type, AllocatorOrContainer>>;

        public:
            using value_container_type = std::conditional_t<is_detected_v<detect_iterator, AllocatorOrContainer>,
                                                            AllocatorOrContainer, underlying_container_type>;

        private:
            using bucket_alloc = typename std::allocator_traits<typename value_container_type::allocator_type>::template
            rebind_alloc<Bucket>;
            using bucket_alloc_traits = std::allocator_traits<bucket_alloc>;

            static constexpr uint8_t initial_shifts = 64 - 2; // 2^(64-m_shift) number of buckets
            static constexpr float default_max_load_factor = 0.8F;

        public:
            using key_type = Key;
            using value_type = typename value_container_type::value_type;
            using size_type = typename value_container_type::size_type;
            using difference_type = typename value_container_type::difference_type;
            using hasher = Hash;
            using key_equal = KeyEqual;
            using allocator_type = typename value_container_type::allocator_type;
            using reference = typename value_container_type::reference;
            using const_reference = typename value_container_type::const_reference;
            using pointer = typename value_container_type::pointer;
            using const_pointer = typename value_container_type::const_pointer;
            using const_iterator = typename value_container_type::const_iterator;
            using iterator = std::conditional_t<is_map_v<T>, typename value_container_type::iterator, const_iterator>;
            using bucket_type = Bucket;

        private:
            using value_idx_type = decltype(Bucket::m_value_idx);
            using dist_and_fingerprint_type = decltype(Bucket::m_dist_and_fingerprint);

            static_assert(
                std::is_trivially_destructible_v<Bucket>,
                "assert there's no need to call destructor / std::destroy"
            );
            static_assert(std::is_trivially_copyable_v<Bucket>, "assert we can just memset / memcpy");

            value_container_type m_values {};
            // Contains all the key-value pairs in one densely stored container. No holes.
            using bucket_pointer = typename std::allocator_traits<bucket_alloc>::pointer;
            bucket_pointer m_buckets {};
            size_t m_num_buckets = 0;
            size_t m_max_bucket_capacity = 0;
            float m_max_load_factor = default_max_load_factor;
            Hash m_hash {};
            KeyEqual m_equal {};
            uint8_t m_shifts = initial_shifts;

            [[nodiscard]] auto next(value_idx_type bucket_idx) const -> value_idx_type {
                // add unlikely
                return (bucket_idx + 1U == m_num_buckets) ? 0 : static_cast<value_idx_type>(bucket_idx + 1U);
            }

            // Helper to access bucket through pointer types
            [[nodiscard]] static constexpr auto at(bucket_pointer bucket_ptr, size_t offset) -> Bucket& {
                return *(bucket_ptr + static_cast<typename std::allocator_traits<bucket_alloc>::difference_type>(
                    offset));
            }

            // use the dist_inc and dist_dec functions so that uint16_t types work without warning
            [[nodiscard]] static constexpr auto dist_inc(dist_and_fingerprint_type x) -> dist_and_fingerprint_type {
                return static_cast<dist_and_fingerprint_type>(x + Bucket::dist_inc);
            }

            [[nodiscard]] static constexpr auto dist_dec(dist_and_fingerprint_type x) -> dist_and_fingerprint_type {
                return static_cast<dist_and_fingerprint_type>(x - Bucket::dist_inc);
            }

            // The goal of mixed_hash is to always produce a high quality 64bit hash.
            template <typename K>
            [[nodiscard]] constexpr auto mixed_hash(K const& key) const -> uint64_t {
                if constexpr (is_detected_v<detect_avalanching, Hash>) {
                    // we know that the hash is good because is_avalanching.
                    if constexpr (sizeof(decltype(m_hash(key))) < sizeof(uint64_t)) {
                        // 32bit hash and is_avalanching => multiply with a constant to avalanche bits upwards
                        return m_hash(key) * UINT64_C(0x9ddfea08eb382d69);
                    } else {
                        // 64bit and is_avalanching => only use the hash itself.
                        return m_hash(key);
                    }
                } else {
                    // not is_avalanching => apply wyhash
                    return rapidhash::hash(m_hash(key));
                }
            }

            [[nodiscard]] constexpr auto dist_and_fingerprint_from_hash(
                uint64_t hash
            ) const -> dist_and_fingerprint_type {
                return Bucket::dist_inc | (static_cast<dist_and_fingerprint_type>(hash) & Bucket::fingerprint_mask);
            }

            [[nodiscard]] constexpr auto bucket_idx_from_hash(uint64_t hash) const -> value_idx_type {
                return static_cast<value_idx_type>(hash >> m_shifts);
            }

            [[nodiscard]] static constexpr auto get_key(value_type const& vt) -> key_type const& {
                if constexpr (is_map_v<T>) {
                    return vt.first;
                } else {
                    return vt;
                }
            }

            template <typename K>
            [[nodiscard]] auto next_while_less(K const& key) const -> Bucket {
                auto hash = mixed_hash(key);
                auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
                auto bucket_idx = bucket_idx_from_hash(hash);

                while (dist_and_fingerprint < at(m_buckets, bucket_idx).m_dist_and_fingerprint) {
                    dist_and_fingerprint = dist_inc(dist_and_fingerprint);
                    bucket_idx = next(bucket_idx);
                }
                return { dist_and_fingerprint, bucket_idx };
            }

            void place_and_shift_up(Bucket bucket, value_idx_type place) {
                while (0 != at(m_buckets, place).m_dist_and_fingerprint) {
                    bucket = std::exchange(at(m_buckets, place), bucket);
                    bucket.m_dist_and_fingerprint = dist_inc(bucket.m_dist_and_fingerprint);
                    place = next(place);
                }
                at(m_buckets, place) = bucket;
            }

            [[nodiscard]] static constexpr auto calc_num_buckets(uint8_t shifts) -> size_t {
                return (std::min)(max_bucket_count(), size_t { 1 } << (64U - shifts));
            }

            [[nodiscard]] constexpr auto calc_shifts_for_size(size_t s) const -> uint8_t {
                auto shifts = initial_shifts;
                while (shifts > 0 && static_cast<size_t>(static_cast<float>(calc_num_buckets(shifts)) *
                    max_load_factor()) < s) {
                    --shifts;
                }
                return shifts;
            }

            // assumes m_values has data, m_buckets=m_buckets_end=nullptr, m_shifts is INITIAL_SHIFTS
            void copy_buckets(table const& other) {
                // assumes m_values has already the correct data copied over.
                if (empty()) {
                    // when empty, at least allocate an initial buckets and clear them.
                    allocate_buckets_from_shift();
                    clear_buckets();
                } else {
                    m_shifts = other.m_shifts;
                    allocate_buckets_from_shift();
                    std::memcpy(m_buckets, other.m_buckets, sizeof(Bucket) * bucket_count());
                }
            }

            /**
             * True when no element can be added any more without increasing the size
             */
            [[nodiscard]] auto is_full() const -> bool {
                return size() > m_max_bucket_capacity;
            }

            void deallocate_buckets() {
                auto ba = bucket_alloc(m_values.get_allocator());
                if (nullptr != m_buckets) {
                    bucket_alloc_traits::deallocate(ba, m_buckets, bucket_count());
                    m_buckets = nullptr;
                }
                m_num_buckets = 0;
                m_max_bucket_capacity = 0;
            }

            void allocate_buckets_from_shift() {
                auto ba = bucket_alloc(m_values.get_allocator());
                m_num_buckets = calc_num_buckets(m_shifts);
                m_buckets = bucket_alloc_traits::allocate(ba, m_num_buckets);
                if (m_num_buckets == max_bucket_count()) {
                    // reached the maximum, make sure we can use each bucket
                    m_max_bucket_capacity = max_bucket_count();
                } else {
                    m_max_bucket_capacity = static_cast<value_idx_type>(static_cast<float>(m_num_buckets) *
                        max_load_factor());
                }
            }

            void clear_buckets() {
                if (m_buckets != nullptr) {
                    std::memset(&*m_buckets, 0, sizeof(Bucket) * bucket_count());
                }
            }

            void clear_and_fill_buckets_from_values() {
                clear_buckets();
                for (value_idx_type value_idx = 0, end_idx = static_cast<value_idx_type>(m_values.size()); value_idx <
                     end_idx; ++value_idx) {
                    auto const& key = get_key(m_values[value_idx]);
                    auto [dist_and_fingerprint, bucket] = next_while_less(key);

                    // we know for certain that key has not yet been inserted, so no need to check it.
                    place_and_shift_up({ dist_and_fingerprint, value_idx }, bucket);
                }
            }

            void increase_size() {
                if (m_max_bucket_capacity == max_bucket_count()) {
                    // remove the value again, we can't add it!
                    m_values.pop_back();
                    on_error_bucket_overflow();
                }
                --m_shifts;
                deallocate_buckets();
                allocate_buckets_from_shift();
                clear_and_fill_buckets_from_values();
            }

            template <typename Op>
            void do_erase(value_idx_type bucket_idx, Op handle_erased_value) {
                auto const value_idx_to_remove = at(m_buckets, bucket_idx).m_value_idx;

                // shift down until either empty or an element with correct spot is found
                auto next_bucket_idx = next(bucket_idx);
                while (at(m_buckets, next_bucket_idx).m_dist_and_fingerprint >= Bucket::dist_inc * 2) {
                    at(m_buckets, bucket_idx) = {
                            dist_dec(at(m_buckets, next_bucket_idx).m_dist_and_fingerprint),
                            at(m_buckets, next_bucket_idx).m_value_idx
                        };
                    bucket_idx = std::exchange(next_bucket_idx, next(next_bucket_idx));
                }
                at(m_buckets, bucket_idx) = {};
                handle_erased_value(std::move(m_values[value_idx_to_remove]));

                // update m_values
                if (value_idx_to_remove != m_values.size() - 1) {
                    // no luck, we'll have to replace the value with the last one and update the index accordingly
                    auto& val = m_values[value_idx_to_remove];
                    val = std::move(m_values.back());

                    // update the values_idx of the moved entry. No need to play the info game, just look until we find the values_idx
                    auto mh = mixed_hash(get_key(val));
                    bucket_idx = bucket_idx_from_hash(mh);

                    auto const values_idx_back = static_cast<value_idx_type>(m_values.size() - 1);
                    while (values_idx_back != at(m_buckets, bucket_idx).m_value_idx) {
                        bucket_idx = next(bucket_idx);
                    }
                    at(m_buckets, bucket_idx).m_value_idx = value_idx_to_remove;
                }
                m_values.pop_back();
            }

            template <typename K, typename Op>
            auto do_erase_key(K&& key, Op handle_erased_value) -> size_t {
                if (empty()) {
                    return 0;
                }

                auto [dist_and_fingerprint, bucket_idx] = next_while_less(key);

                while (dist_and_fingerprint == at(m_buckets, bucket_idx).m_dist_and_fingerprint && !m_equal(
                    key,
                    get_key(m_values[at(m_buckets, bucket_idx).m_value_idx])
                )) {
                    dist_and_fingerprint = dist_inc(dist_and_fingerprint);
                    bucket_idx = next(bucket_idx);
                }

                if (dist_and_fingerprint != at(m_buckets, bucket_idx).m_dist_and_fingerprint) {
                    return 0;
                }
                do_erase(bucket_idx, handle_erased_value);
                return 1;
            }

            template <class K, class M>
            auto do_insert_or_assign(K&& key, M&& mapped) -> std::pair<iterator, bool> {
                auto it_isinserted = try_emplace(std::forward<K>(key), std::forward<M>(mapped));
                if (!it_isinserted.second) {
                    it_isinserted.first->second = std::forward<M>(mapped);
                }
                return it_isinserted;
            }

            template <typename... Args>
            auto do_place_element(
                dist_and_fingerprint_type dist_and_fingerprint,
                value_idx_type bucket_idx,
                Args&&... args
            ) -> std::pair<iterator, bool> {
                // emplace the new value. If that throws an exception, no harm done; index is still in a valid state
                m_values.emplace_back(std::forward<Args>(args)...);

                auto value_idx = static_cast<value_idx_type>(m_values.size() - 1);
                if (is_full()) [[unlikely]] {
                    increase_size();
                } else {
                    place_and_shift_up({ dist_and_fingerprint, value_idx }, bucket_idx);
                }

                // place element and shift up until we find an empty spot
                return { begin() + static_cast<difference_type>(value_idx), true };
            }

            template <typename K, typename... Args>
            auto do_try_emplace(K&& key, Args&&... args) -> std::pair<iterator, bool> {
                auto hash = mixed_hash(key);
                auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
                auto bucket_idx = bucket_idx_from_hash(hash);

                while (true) {
                    auto* bucket = &at(m_buckets, bucket_idx);
                    if (dist_and_fingerprint == bucket->m_dist_and_fingerprint) {
                        if (m_equal(key, get_key(m_values[bucket->m_value_idx]))) {
                            return { begin() + static_cast<difference_type>(bucket->m_value_idx), false };
                        }
                    } else if (dist_and_fingerprint > bucket->m_dist_and_fingerprint) {
                        return do_place_element(
                            dist_and_fingerprint,
                            bucket_idx,
                            std::piecewise_construct,
                            std::forward_as_tuple(std::forward<K>(key)),
                            std::forward_as_tuple(std::forward<Args>(args)...)
                        );
                    }
                    dist_and_fingerprint = dist_inc(dist_and_fingerprint);
                    bucket_idx = next(bucket_idx);
                }
            }

            template <typename K>
            auto do_find(K const& key) -> iterator {
                if (empty()) [[unlikely]] {
                    return end();
                }

                auto mh = mixed_hash(key);
                auto dist_and_fingerprint = dist_and_fingerprint_from_hash(mh);
                auto bucket_idx = bucket_idx_from_hash(mh);
                auto* bucket = &at(m_buckets, bucket_idx);

                // unrolled loop. *Always* check a few directly, then enter the loop. This is faster.
                if (dist_and_fingerprint == bucket->m_dist_and_fingerprint && m_equal(
                    key,
                    get_key(m_values[bucket->m_value_idx])
                )) {
                    return begin() + static_cast<difference_type>(bucket->m_value_idx);
                }
                dist_and_fingerprint = dist_inc(dist_and_fingerprint);
                bucket_idx = next(bucket_idx);
                bucket = &at(m_buckets, bucket_idx);

                if (dist_and_fingerprint == bucket->m_dist_and_fingerprint && m_equal(
                    key,
                    get_key(m_values[bucket->m_value_idx])
                )) {
                    return begin() + static_cast<difference_type>(bucket->m_value_idx);
                }
                dist_and_fingerprint = dist_inc(dist_and_fingerprint);
                bucket_idx = next(bucket_idx);
                bucket = &at(m_buckets, bucket_idx);

                while (true) {
                    if (dist_and_fingerprint == bucket->m_dist_and_fingerprint) {
                        if (m_equal(key, get_key(m_values[bucket->m_value_idx]))) {
                            return begin() + static_cast<difference_type>(bucket->m_value_idx);
                        }
                    } else if (dist_and_fingerprint > bucket->m_dist_and_fingerprint) {
                        return end();
                    }
                    dist_and_fingerprint = dist_inc(dist_and_fingerprint);
                    bucket_idx = next(bucket_idx);
                    bucket = &at(m_buckets, bucket_idx);
                }
            }

            template <typename K>
            auto do_find(K const& key) const -> const_iterator {
                return const_cast<table*>(this)->do_find(key); // NOLINT(cppcoreguidelines-pro-type-const-cast)
            }

            template <typename K, typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto do_at(K const& key) -> Q& {
                if (auto it = find(key); end() != it) [[likely]] {
                    return it->second;
                }
                on_error_key_not_found();
            }

            template <typename K, typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto do_at(K const& key) const -> Q const& {
                return const_cast<table*>(this)->at(key); // NOLINT(cppcoreguidelines-pro-type-const-cast)
            }

        public:
            explicit table(
                size_t bucket_count,
                Hash const& hash = Hash(),
                KeyEqual const& equal = KeyEqual(),
                allocator_type const& alloc_or_container = allocator_type()
            ) : m_values(alloc_or_container),
                m_hash(hash),
                m_equal(equal) {
                if (0 != bucket_count) {
                    reserve(bucket_count);
                } else {
                    allocate_buckets_from_shift();
                    clear_buckets();
                }
            }

            table() : table(0) {}

            table(size_t bucket_count, allocator_type const& alloc) : table(bucket_count, Hash(), KeyEqual(), alloc) {}

            table(size_t bucket_count, Hash const& hash, allocator_type const& alloc) : table(
                bucket_count,
                hash,
                KeyEqual(),
                alloc
            ) {}

            explicit table(allocator_type const& alloc) : table(0, Hash(), KeyEqual(), alloc) {}

            template <class InputIt>
            table(
                InputIt first,
                InputIt last,
                size_type bucket_count = 0,
                Hash const& hash = Hash(),
                KeyEqual const& equal = KeyEqual(),
                allocator_type const& alloc = allocator_type()
            ) : table(bucket_count, hash, equal, alloc) {
                insert(first, last);
            }

            template <class InputIt>
            table(InputIt first, InputIt last, size_type bucket_count, allocator_type const& alloc) : table(
                first,
                last,
                bucket_count,
                Hash(),
                KeyEqual(),
                alloc
            ) {}

            template <class InputIt>
            table(
                InputIt first,
                InputIt last,
                size_type bucket_count,
                Hash const& hash,
                allocator_type const& alloc
            ) : table(first, last, bucket_count, hash, KeyEqual(), alloc) {}

            table(table const& other) : table(other, other.m_values.get_allocator()) {}

            table(table const& other, allocator_type const& alloc) : m_values(other.m_values, alloc),
                                                                     m_max_load_factor(other.m_max_load_factor),
                                                                     m_hash(other.m_hash),
                                                                     m_equal(other.m_equal) {
                copy_buckets(other);
            }

            table(table&& other) noexcept : table(std::move(other), other.m_values.get_allocator()) {}

            table(table&& other, allocator_type const& alloc) noexcept : m_values(alloc) {
                *this = std::move(other);
            }

            table(
                std::initializer_list<value_type> ilist,
                size_t bucket_count = 0,
                Hash const& hash = Hash(),
                KeyEqual const& equal = KeyEqual(),
                allocator_type const& alloc = allocator_type()
            ) : table(bucket_count, hash, equal, alloc) {
                insert(ilist);
            }

            table(std::initializer_list<value_type> ilist, size_type bucket_count, allocator_type const& alloc) : table(
                ilist,
                bucket_count,
                Hash(),
                KeyEqual(),
                alloc
            ) {}

            table(
                std::initializer_list<value_type> init,
                size_type bucket_count,
                Hash const& hash,
                allocator_type const& alloc
            ) : table(init, bucket_count, hash, KeyEqual(), alloc) {}

            ~table() {
                if (nullptr != m_buckets) {
                    auto ba = bucket_alloc(m_values.get_allocator());
                    bucket_alloc_traits::deallocate(ba, m_buckets, bucket_count());
                }
            }

            auto operator=(table const& other) -> table& {
                if (&other != this) {
                    deallocate_buckets(); // deallocate before m_values is set (might have another allocator)
                    m_values = other.m_values;
                    m_max_load_factor = other.m_max_load_factor;
                    m_hash = other.m_hash;
                    m_equal = other.m_equal;
                    m_shifts = initial_shifts;
                    copy_buckets(other);
                }
                return *this;
            }

            auto operator=(
                table&& other
            ) noexcept(noexcept(std::is_nothrow_move_assignable_v<value_container_type> &&
                std::is_nothrow_move_assignable_v<Hash> && std::is_nothrow_move_assignable_v<KeyEqual>)) -> table& {
                if (&other != this) {
                    deallocate_buckets(); // deallocate before m_values is set (might have another allocator)
                    m_values = std::move(other.m_values);
                    other.m_values.clear();

                    // we can only reuse m_buckets when both maps have the same allocator!
                    if (get_allocator() == other.get_allocator()) {
                        m_buckets = std::exchange(other.m_buckets, nullptr);
                        m_num_buckets = std::exchange(other.m_num_buckets, 0);
                        m_max_bucket_capacity = std::exchange(other.m_max_bucket_capacity, 0);
                        m_shifts = std::exchange(other.m_shifts, initial_shifts);
                        m_max_load_factor = std::exchange(other.m_max_load_factor, default_max_load_factor);
                        m_hash = std::exchange(other.m_hash, {});
                        m_equal = std::exchange(other.m_equal, {});
                        other.allocate_buckets_from_shift();
                        other.clear_buckets();
                    } else {
                        // set max_load_factor *before* copying the other's buckets, so we have the same
                        // behavior
                        m_max_load_factor = other.m_max_load_factor;

                        // copy_buckets sets m_buckets, m_num_buckets, m_max_bucket_capacity, m_shifts
                        copy_buckets(other);
                        // clear's the other's buckets so other is now already usable.
                        other.clear_buckets();
                        m_hash = other.m_hash;
                        m_equal = other.m_equal;
                    }
                    // map "other" is now already usable, it's empty.
                }
                return *this;
            }

            auto operator=(std::initializer_list<value_type> ilist) -> table& {
                clear();
                insert(ilist);
                return *this;
            }

            auto get_allocator() const noexcept -> allocator_type {
                return m_values.get_allocator();
            }

            // iterators //////////////////////////////////////////////////////////////

            auto begin() noexcept -> iterator {
                return m_values.begin();
            }

            auto begin() const noexcept -> const_iterator {
                return m_values.begin();
            }

            auto cbegin() const noexcept -> const_iterator {
                return m_values.cbegin();
            }

            auto end() noexcept -> iterator {
                return m_values.end();
            }

            auto cend() const noexcept -> const_iterator {
                return m_values.cend();
            }

            auto end() const noexcept -> const_iterator {
                return m_values.end();
            }

            // capacity ///////////////////////////////////////////////////////////////

            [[nodiscard]] auto empty() const noexcept -> bool {
                return m_values.empty();
            }

            [[nodiscard]] auto size() const noexcept -> size_t {
                return m_values.size();
            }

            [[nodiscard]] static constexpr auto max_size() noexcept -> size_t {
                if constexpr ((std::numeric_limits<value_idx_type>::max)() == (std::numeric_limits<size_t>::max)()) {
                    return size_t { 1 } << (sizeof(value_idx_type) * 8 - 1);
                } else {
                    return size_t { 1 } << (sizeof(value_idx_type) * 8);
                }
            }

            // modifiers //////////////////////////////////////////////////////////////

            void clear() {
                m_values.clear();
                clear_buckets();
            }

            auto insert(value_type const& value) -> std::pair<iterator, bool> {
                return emplace(value);
            }

            auto insert(value_type&& value) -> std::pair<iterator, bool> {
                return emplace(std::move(value));
            }

            template <class P, std::enable_if_t<std::is_constructible_v<value_type, P&&>, bool>  = true>
            auto insert(P&& value) -> std::pair<iterator, bool> {
                return emplace(std::forward<P>(value));
            }

            auto insert(const_iterator /*hint*/, value_type const& value) -> iterator {
                return insert(value).first;
            }

            auto insert(const_iterator /*hint*/, value_type&& value) -> iterator {
                return insert(std::move(value)).first;
            }

            template <class P, std::enable_if_t<std::is_constructible_v<value_type, P&&>, bool>  = true>
            auto insert(const_iterator /*hint*/, P&& value) -> iterator {
                return insert(std::forward<P>(value)).first;
            }

            template <class InputIt>
            void insert(InputIt first, InputIt last) {
                while (first != last) {
                    insert(*first);
                    ++first;
                }
            }

            void insert(std::initializer_list<value_type> ilist) {
                insert(ilist.begin(), ilist.end());
            }

            // nonstandard API: *this is emptied.
            // Also see "A Standard flat_map" https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p0429r9.pdf
            auto extract() && -> value_container_type {
                return std::move(m_values);
            }

            // nonstandard API:
            // Discards the internally held container and replaces it with the one passed. Erases non-unique elements.
            auto replace(value_container_type&& container) {
                if (container.size() > max_size()) [[unlikely]] {
                    on_error_too_many_elements();
                }
                auto shifts = calc_shifts_for_size(container.size());
                if (0 == m_num_buckets || shifts < m_shifts || container.get_allocator() != m_values.get_allocator()) {
                    m_shifts = shifts;
                    deallocate_buckets();
                    allocate_buckets_from_shift();
                }
                clear_buckets();

                m_values = std::move(container);

                // can't use clear_and_fill_buckets_from_values() because container elements might not be unique
                auto value_idx = value_idx_type {};

                // loop until we reach the end of the container. duplicated entries will be replaced with back().
                while (value_idx != static_cast<value_idx_type>(m_values.size())) {
                    auto const& key = get_key(m_values[value_idx]);

                    auto hash = mixed_hash(key);
                    auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
                    auto bucket_idx = bucket_idx_from_hash(hash);

                    bool key_found = false;
                    while (true) {
                        auto const& bucket = at(m_buckets, bucket_idx);
                        if (dist_and_fingerprint > bucket.m_dist_and_fingerprint) {
                            break;
                        }
                        if (dist_and_fingerprint == bucket.m_dist_and_fingerprint && m_equal(
                            key,
                            get_key(m_values[bucket.m_value_idx])
                        )) {
                            key_found = true;
                            break;
                        }
                        dist_and_fingerprint = dist_inc(dist_and_fingerprint);
                        bucket_idx = next(bucket_idx);
                    }

                    if (key_found) {
                        if (value_idx != static_cast<value_idx_type>(m_values.size() - 1)) {
                            m_values[value_idx] = std::move(m_values.back());
                        }
                        m_values.pop_back();
                    } else {
                        place_and_shift_up({ dist_and_fingerprint, value_idx }, bucket_idx);
                        ++value_idx;
                    }
                }
            }

            template <class M, typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto insert_or_assign(Key const& key, M&& mapped) -> std::pair<iterator, bool> {
                return do_insert_or_assign(key, std::forward<M>(mapped));
            }

            template <class M, typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto insert_or_assign(Key&& key, M&& mapped) -> std::pair<iterator, bool> {
                return do_insert_or_assign(std::move(key), std::forward<M>(mapped));
            }

            template <typename K, typename M, typename Q = T, typename H = Hash, typename KE = KeyEqual,
                      std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE>, bool>  = true>
            auto insert_or_assign(K&& key, M&& mapped) -> std::pair<iterator, bool> {
                return do_insert_or_assign(std::forward<K>(key), std::forward<M>(mapped));
            }

            template <class M, typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto insert_or_assign(const_iterator /*hint*/, Key const& key, M&& mapped) -> iterator {
                return do_insert_or_assign(key, std::forward<M>(mapped)).first;
            }

            template <class M, typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto insert_or_assign(const_iterator /*hint*/, Key&& key, M&& mapped) -> iterator {
                return do_insert_or_assign(std::move(key), std::forward<M>(mapped)).first;
            }

            template <typename K, typename M, typename Q = T, typename H = Hash, typename KE = KeyEqual,
                      std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE>, bool>  = true>
            auto insert_or_assign(const_iterator /*hint*/, K&& key, M&& mapped) -> iterator {
                return do_insert_or_assign(std::forward<K>(key), std::forward<M>(mapped)).first;
            }

            // Single arguments for unordered_set can be used without having to construct the value_type
            template <class K, typename Q = T, typename H = Hash, typename KE = KeyEqual, std::enable_if_t<
                          !is_map_v<Q> && is_transparent_v<H, KE>, bool>  = true>
            auto emplace(K&& key) -> std::pair<iterator, bool> {
                auto hash = mixed_hash(key);
                auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
                auto bucket_idx = bucket_idx_from_hash(hash);

                while (dist_and_fingerprint <= at(m_buckets, bucket_idx).m_dist_and_fingerprint) {
                    if (dist_and_fingerprint == at(m_buckets, bucket_idx).m_dist_and_fingerprint && m_equal(
                        key,
                        m_values[at(m_buckets, bucket_idx).m_value_idx]
                    )) {
                        // found it, return without ever actually creating anything
                        return { begin() + static_cast<difference_type>(at(m_buckets, bucket_idx).m_value_idx), false };
                    }
                    dist_and_fingerprint = dist_inc(dist_and_fingerprint);
                    bucket_idx = next(bucket_idx);
                }

                // value is new, insert element first, so when exception happens we are in a valid state
                return do_place_element(dist_and_fingerprint, bucket_idx, std::forward<K>(key));
            }

            template <class... Args>
            auto emplace(Args&&... args) -> std::pair<iterator, bool> {
                // we have to instantiate the value_type to be able to access the key.
                // 1. emplace_back the object so it is constructed. 2. If the key is already there, pop it later in the loop.
                auto& key = get_key(m_values.emplace_back(std::forward<Args>(args)...));
                auto hash = mixed_hash(key);
                auto dist_and_fingerprint = dist_and_fingerprint_from_hash(hash);
                auto bucket_idx = bucket_idx_from_hash(hash);

                while (dist_and_fingerprint <= at(m_buckets, bucket_idx).m_dist_and_fingerprint) {
                    if (dist_and_fingerprint == at(m_buckets, bucket_idx).m_dist_and_fingerprint && m_equal(
                        key,
                        get_key(m_values[at(m_buckets, bucket_idx).m_value_idx])
                    )) {
                        m_values.pop_back(); // value was already there, so get rid of it
                        return { begin() + static_cast<difference_type>(at(m_buckets, bucket_idx).m_value_idx), false };
                    }
                    dist_and_fingerprint = dist_inc(dist_and_fingerprint);
                    bucket_idx = next(bucket_idx);
                }

                // value is new, place the bucket and shift up until we find an empty spot
                auto value_idx = static_cast<value_idx_type>(m_values.size() - 1);
                if (is_full()) [[unlikely]] {
                    // increase_size just rehashes all the data we have in m_values
                    increase_size();
                } else {
                    // place element and shift up until we find an empty spot
                    place_and_shift_up({ dist_and_fingerprint, value_idx }, bucket_idx);
                }
                return { begin() + static_cast<difference_type>(value_idx), true };
            }

            template <class... Args>
            auto emplace_hint(const_iterator /*hint*/, Args&&... args) -> iterator {
                return emplace(std::forward<Args>(args)...).first;
            }

            template <class... Args, typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto try_emplace(Key const& key, Args&&... args) -> std::pair<iterator, bool> {
                return do_try_emplace(key, std::forward<Args>(args)...);
            }

            template <class... Args, typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto try_emplace(Key&& key, Args&&... args) -> std::pair<iterator, bool> {
                return do_try_emplace(std::move(key), std::forward<Args>(args)...);
            }

            template <class... Args, typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto try_emplace(const_iterator /*hint*/, Key const& key, Args&&... args) -> iterator {
                return do_try_emplace(key, std::forward<Args>(args)...).first;
            }

            template <class... Args, typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto try_emplace(const_iterator /*hint*/, Key&& key, Args&&... args) -> iterator {
                return do_try_emplace(std::move(key), std::forward<Args>(args)...).first;
            }

            template <typename K, typename... Args, typename Q = T, typename H = Hash, typename KE = KeyEqual,
                      std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE> && is_neither_convertible_v<
                                           K&&, iterator, const_iterator>, bool>  = true>
            auto try_emplace(K&& key, Args&&... args) -> std::pair<iterator, bool> {
                return do_try_emplace(std::forward<K>(key), std::forward<Args>(args)...);
            }

            template <typename K, typename... Args, typename Q = T, typename H = Hash, typename KE = KeyEqual,
                      std::enable_if_t<is_map_v<Q> && is_transparent_v<H, KE> && is_neither_convertible_v<
                                           K&&, iterator, const_iterator>, bool>  = true>
            auto try_emplace(const_iterator /*hint*/, K&& key, Args&&... args) -> iterator {
                return do_try_emplace(std::forward<K>(key), std::forward<Args>(args)...).first;
            }

            auto erase(iterator it) -> iterator {
                auto hash = mixed_hash(get_key(*it));
                auto bucket_idx = bucket_idx_from_hash(hash);

                auto const value_idx_to_remove = static_cast<value_idx_type>(it - cbegin());
                while (at(m_buckets, bucket_idx).m_value_idx != value_idx_to_remove) {
                    bucket_idx = next(bucket_idx);
                }

                do_erase(bucket_idx, [](value_type&& /*unused*/) {});
                return begin() + static_cast<difference_type>(value_idx_to_remove);
            }

            auto extract(iterator it) -> value_type {
                auto hash = mixed_hash(get_key(*it));
                auto bucket_idx = bucket_idx_from_hash(hash);

                auto const value_idx_to_remove = static_cast<value_idx_type>(it - cbegin());
                while (at(m_buckets, bucket_idx).m_value_idx != value_idx_to_remove) {
                    bucket_idx = next(bucket_idx);
                }

                auto tmp = std::optional<value_type> {};
                do_erase(
                    bucket_idx,
                    [&tmp](value_type&& val) {
                        tmp = std::move(val);
                    }
                );
                return std::move(tmp).value();
            }

            template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto erase(const_iterator it) -> iterator {
                return erase(begin() + (it - cbegin()));
            }

            template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto extract(const_iterator it) -> value_type {
                return extract(begin() + (it - cbegin()));
            }

            auto erase(const_iterator first, const_iterator last) -> iterator {
                auto const idx_first = first - cbegin();
                auto const idx_last = last - cbegin();
                auto const first_to_last = std::distance(first, last);
                auto const last_to_end = std::distance(last, cend());

                // remove elements from left to right which moves elements from the end back
                auto const mid = idx_first + (std::min)(first_to_last, last_to_end);
                auto idx = idx_first;
                while (idx != mid) {
                    erase(begin() + idx);
                    ++idx;
                }

                // all elements from the right are moved, now remove the last element until all done
                idx = idx_last;
                while (idx != mid) {
                    --idx;
                    erase(begin() + idx);
                }

                return begin() + idx_first;
            }

            auto erase(Key const& key) -> size_t {
                return do_erase_key(key, [](value_type&& /*unused*/) {});
            }

            auto extract(Key const& key) -> std::optional<value_type> {
                auto tmp = std::optional<value_type> {};
                do_erase_key(
                    key,
                    [&tmp](value_type&& val) {
                        tmp = std::move(val);
                    }
                );
                return tmp;
            }

            template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool>  =
                          true>
            auto erase(K&& key) -> size_t {
                return do_erase_key(std::forward<K>(key), [](value_type&& /*unused*/) {});
            }

            template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool>  =
                          true>
            auto extract(K&& key) -> std::optional<value_type> {
                auto tmp = std::optional<value_type> {};
                do_erase_key(
                    std::forward<K>(key),
                    [&tmp](value_type&& val) {
                        tmp = std::move(val);
                    }
                );
                return tmp;
            }

            void swap(
                table& other
            ) noexcept(noexcept(std::is_nothrow_swappable_v<value_container_type> && std::is_nothrow_swappable_v<Hash>
                && std::is_nothrow_swappable_v<KeyEqual>)) {
                using std::swap;
                swap(other, *this);
            }

            // lookup /////////////////////////////////////////////////////////////////

            template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto at(key_type const& key) -> Q& {
                return do_at(key);
            }

            template <typename K, typename Q = T, typename H = Hash, typename KE = KeyEqual, std::enable_if_t<
                          is_map_v<Q> && is_transparent_v<H, KE>, bool>  = true>
            auto at(K const& key) -> Q& {
                return do_at(key);
            }

            template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto at(key_type const& key) const -> Q const& {
                return do_at(key);
            }

            template <typename K, typename Q = T, typename H = Hash, typename KE = KeyEqual, std::enable_if_t<
                          is_map_v<Q> && is_transparent_v<H, KE>, bool>  = true>
            auto at(K const& key) const -> Q const& {
                return do_at(key);
            }

            template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto operator[ ](Key const& key) -> Q& {
                return try_emplace(key).first->second;
            }

            template <typename Q = T, std::enable_if_t<is_map_v<Q>, bool>  = true>
            auto operator[](Key&& key) -> Q& {
                return try_emplace(std::move(key)).first->second;
            }

            template <typename K, typename Q = T, typename H = Hash, typename KE = KeyEqual, std::enable_if_t<
                          is_map_v<Q> && is_transparent_v<H, KE>, bool>  = true>
            auto operator[](K&& key) -> Q& {
                return try_emplace(std::forward<K>(key)).first->second;
            }

            auto count(Key const& key) const -> size_t {
                return find(key) == end() ? 0 : 1;
            }

            template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool>  =
                          true>
            auto count(K const& key) const -> size_t {
                return find(key) == end() ? 0 : 1;
            }

            auto find(Key const& key) -> iterator {
                return do_find(key);
            }

            auto find(Key const& key) const -> const_iterator {
                return do_find(key);
            }

            template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool>  =
                          true>
            auto find(K const& key) -> iterator {
                return do_find(key);
            }

            template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool>  =
                          true>
            auto find(K const& key) const -> const_iterator {
                return do_find(key);
            }

            auto contains(Key const& key) const -> bool {
                return find(key) != end();
            }

            template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool>  =
                          true>
            auto contains(K const& key) const -> bool {
                return find(key) != end();
            }

            auto equal_range(Key const& key) -> std::pair<iterator, iterator> {
                auto it = do_find(key);
                return { it, it == end() ? end() : it + 1 };
            }

            auto equal_range(const Key& key) const -> std::pair<const_iterator, const_iterator> {
                auto it = do_find(key);
                return { it, it == end() ? end() : it + 1 };
            }

            template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool>  =
                          true>
            auto equal_range(K const& key) -> std::pair<iterator, iterator> {
                auto it = do_find(key);
                return { it, it == end() ? end() : it + 1 };
            }

            template <class K, class H = Hash, class KE = KeyEqual, std::enable_if_t<is_transparent_v<H, KE>, bool>  =
                          true>
            auto equal_range(K const& key) const -> std::pair<const_iterator, const_iterator> {
                auto it = do_find(key);
                return { it, it == end() ? end() : it + 1 };
            }

            // bucket interface ///////////////////////////////////////////////////////

            [[nodiscard]] auto bucket_count() const noexcept -> size_t {
                // NOLINT(modernize-use-nodiscard)
                return m_num_buckets;
            }

            static constexpr auto max_bucket_count() noexcept -> size_t {
                // NOLINT(modernize-use-nodiscard)
                return max_size();
            }

            // hash policy ////////////////////////////////////////////////////////////

            [[nodiscard]] auto load_factor() const -> float {
                return bucket_count() ? static_cast<float>(size()) / static_cast<float>(bucket_count()) : 0.0F;
            }

            [[nodiscard]] auto max_load_factor() const -> float {
                return m_max_load_factor;
            }

            void max_load_factor(float ml) {
                m_max_load_factor = ml;
                if (m_num_buckets != max_bucket_count()) {
                    m_max_bucket_capacity = static_cast<value_idx_type>(static_cast<float>(bucket_count()) *
                        max_load_factor());
                }
            }

            void rehash(size_t count) {
                count = (std::min)(count, max_size());
                auto shifts = calc_shifts_for_size((std::max)(count, size()));
                if (shifts != m_shifts) {
                    m_shifts = shifts;
                    deallocate_buckets();
                    m_values.shrink_to_fit();
                    allocate_buckets_from_shift();
                    clear_and_fill_buckets_from_values();
                }
            }

            void reserve(size_t capa) {
                capa = (std::min)(capa, max_size());
                if constexpr (has_reserve<value_container_type>) {
                    // std::deque doesn't have reserve(). Make sure we only call when available
                    m_values.reserve(capa);
                }
                auto shifts = calc_shifts_for_size((std::max)(capa, size()));
                if (0 == m_num_buckets || shifts < m_shifts) {
                    m_shifts = shifts;
                    deallocate_buckets();
                    allocate_buckets_from_shift();
                    clear_and_fill_buckets_from_values();
                }
            }

            // observers //////////////////////////////////////////////////////////////

            auto hash_function() const -> hasher {
                return m_hash;
            }

            auto key_eq() const -> key_equal {
                return m_equal;
            }

            // nonstandard API: expose the underlying values container
            [[nodiscard]] auto values() const noexcept -> value_container_type const& {
                return m_values;
            }

            // non-member functions ///////////////////////////////////////////////////

            friend auto operator==(table const& a, table const& b) -> bool {
                if (&a == &b) {
                    return true;
                }
                if (a.size() != b.size()) {
                    return false;
                }
                for (auto const& b_entry : b) {
                    auto it = a.find(get_key(b_entry));
                    if constexpr (is_map_v<T>) {
                        // map: check that key is here, then also check that value is the same
                        if (a.end() == it || !(b_entry.second == it->second)) {
                            return false;
                        }
                    } else {
                        // set: only check that the key is here
                        if (a.end() == it) {
                            return false;
                        }
                    }
                }
                return true;
            }

            friend auto operator!=(table const& a, table const& b) -> bool {
                return !(a == b);
            }
        };
    } // namespace detail

    template <class Key, class T, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class
              AllocatorOrContainer = std::allocator<std::pair<Key, T>>, class Bucket = bucket_type::standard>
    using map = detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, false>;
    template <class Key, class T, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class
              AllocatorOrContainer = std::allocator<std::pair<Key, T>>, class Bucket = bucket_type::standard>
    using segmented_map = detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, true>;
    template <class Key, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class AllocatorOrContainer =
              std::allocator<Key>, class Bucket = bucket_type::standard>
    using set = detail::table<Key, void, Hash, KeyEqual, AllocatorOrContainer, Bucket, false>;
    template <class Key, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class AllocatorOrContainer =
              std::allocator<Key>, class Bucket = bucket_type::standard>
    using segmented_set = detail::table<Key, void, Hash, KeyEqual, AllocatorOrContainer, Bucket, true>;

    namespace pmr {
        template <class Key, class T, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class Bucket =
                  bucket_type::standard>
        using map = detail::table<Key, T, Hash, KeyEqual, std::pmr::polymorphic_allocator<std::pair<Key, T>>, Bucket,
                                  false>;

        template <class Key, class T, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class Bucket =
                  bucket_type::standard>
        using segmented_map = detail::table<Key, T, Hash, KeyEqual, std::pmr::polymorphic_allocator<std::pair<Key, T>>,
                                            Bucket, true>;

        template <class Key, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class Bucket =
                  bucket_type::standard>
        using set = detail::table<Key, void, Hash, KeyEqual, std::pmr::polymorphic_allocator<Key>, Bucket, false>;

        template <class Key, class Hash = hash<Key>, class KeyEqual = std::equal_to<Key>, class Bucket =
                  bucket_type::standard>
        using segmented_set = detail::table<Key, void, Hash, KeyEqual, std::pmr::polymorphic_allocator<Key>, Bucket,
                                            true>;
    } // namespace pmr

    // deduction guides ///////////////////////////////////////////////////////////

    // deduction guides for alias templates are only possible since C++20
    // see https://en.cppreference.com/w/cpp/language/class_template_argument_deduction
} // namespace bite::unordered_dense

// std extensions /////////////////////////////////////////////////////////////

namespace std {
    // NOLINT(cert-dcl58-cpp)

    template <class Key, class T, class Hash, class KeyEqual, class AllocatorOrContainer, class Bucket, class Pred, bool
              IsSegmented>
    // NOLINTNEXTLINE(cert-dcl58-cpp)
    auto erase_if(
        bite::unordered_dense::detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket, IsSegmented>& map,
        Pred pred
    ) -> size_t {
        using map_t = bite::unordered_dense::detail::table<Key, T, Hash, KeyEqual, AllocatorOrContainer, Bucket,
                                                           IsSegmented>;

        // going back to front because erase() invalidates the end iterator
        auto const old_size = map.size();
        auto idx = old_size;
        while (idx) {
            --idx;
            auto it = map.begin() + static_cast<typename map_t::difference_type>(idx);
            if (pred(*it)) {
                map.erase(it);
            }
        }

        return old_size - map.size();
    }
} // namespace std

#endif //UNORDERED_DENSE_H
