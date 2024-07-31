#ifndef PERFECT_HASHMAP_H
#define PERFECT_HASHMAP_H
#include <algorithm>
#include <string_view>
#include <vector>
#include <ranges>
#include <cstdint>

/**
 * Compile time constructed unordered map with O(1) worst case lookup thanks to perfect hashing.
 * May significantly slow down compile times!!!
 *
 * NOTE: this implementation is not really meant as a general purpose container
 * it lacks hash customizations, custom key types and many common operations
 * for better alternative try: https://github.com/serge-sans-paille/frozen/tree/master
 *
 * @tparam V value type
 * @tparam size Number of key-value pairs
 */
template <typename V, std::size_t size>
class perfect_map {
public:
    // heavily inspired from http://stevehanov.ca/blog/index.php?id=119
    // optim: investigate gperf for better performance

    // TODO: swap hash for rapidhash!

    using kv_pair = std::pair<std::string_view, V>;
    using dict_t = std::array<kv_pair, size>;

    constexpr explicit perfect_map(dict_t dictionary) {
        std::array<std::vector<std::string_view>, size> buckets;
        std::array<bool, size> has_value {};

        // Split keys into buckets
        for (auto& key : dictionary | std::views::keys) {
            buckets[fnv1(key) % size].push_back(key);
        }

        // Sort buckets in largest bucket first order
        std::ranges::sort(
            buckets,
            [](auto& a, auto& b) {
                return a.size() > b.size();
            }
        );

        std::size_t bucket_idx = 0;
        for (; bucket_idx < buckets.size(); ++bucket_idx) {
            auto& bucket = buckets[bucket_idx];

            if (bucket.size() <= 1)
                break;

            // try different offsets values until we find one that places all items into empty slots
            // optim: maybe random should try random offsets?
            std::uint64_t offset = 0;
            std::size_t item_idx = 0;
            std::vector<std::uint64_t> slots;
            while (item_idx < bucket.size()) {
                std::uint64_t slot = fnv1(bucket[item_idx], offset) % size;
                if (has_value[slot] || std::ranges::contains(slots, slot)) {
                    // collison detected - restart search with different offset
                    offset += 1;
                    item_idx = 0;
                    slots.clear();
                } else {
                    slots.push_back(slot);
                    item_idx += 1;
                }
            }
            // store found offset indexed by first bucket element
            offsets[fnv1(bucket[0]) % size] = { .value = offset, .is_pos = false };
            // remap values
            for (std::size_t i = 0; i < bucket.size(); ++i) {
                // note this operation is linear but performance is not as critical as it will be ran at compile time
                kv_pair kv = *std::ranges::find(dictionary, bucket[i], &kv_pair::first);
                values[slots[i]] = { .value = kv.second, .hash = fnv1(kv.first) };
                has_value[slots[i]] = true;
            }
        }

        // Now only buckets with one item remain
        // insert those item into remaining free slots
        std::vector<std::size_t> empty_positions;
        for (int i = 0; i < size; ++i) {
            if (!has_value[i]) {
                empty_positions.push_back(i);
            }
        }

        for (; bucket_idx < buckets.size(); ++bucket_idx) {
            auto& bucket = buckets[bucket_idx];
            if (bucket.empty())
                break;
            std::size_t slot = empty_positions.back();
            empty_positions.pop_back();
            offsets[fnv1(bucket[0]) % size] = { .value = slot, .is_pos = true };
            kv_pair kv = *std::ranges::find(dictionary, bucket[0], &kv_pair::first);
            values[slot] = { .value = kv.second, .hash = fnv1(kv.first) };
        }
    }

    /**
     * Lookup value using provided key
     * @param key key to lookup
     * @return returns mapped value if found
     */
    std::optional<V> constexpr operator[](std::string_view key) const {

        std::uint64_t key_hash = fnv1(key);
        Offset offset = offsets[key_hash % offsets.size()];
        ValueHashPair value_hash_pair;
        if (offset.is_pos) {
            value_hash_pair = values[offset.value];
        } else {
            value_hash_pair = values[fnv1(key, offset.value) % values.size()];
        }
        // return value only if hashes match
        if (value_hash_pair.hash == key_hash) {
            return value_hash_pair.value;
        }
        return {};
    }

private:
    constexpr static std::uint64_t FNV_offset_basis = 0xcbf29ce484222325;
    constexpr static std::uint64_t FNV_prime = 0x100000001b3;

    // FNV-1 hash https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
    [[nodiscard]] constexpr static std::uint64_t fnv1(
        std::string_view string,
        std::uint64_t offset = FNV_offset_basis
    ) {
        std::uint64_t hash = offset;
        for (char c : string) {
            hash = hash * FNV_prime ^ static_cast<std::uint64_t>(c);
        }
        return hash;
    }

    /**
     * Either stores offset value used by fnv1 hash function or position in values array if is_pos is true
     */
    struct Offset {
        std::uint64_t value;
        bool is_pos;
    };

    struct ValueHashPair {
        V value;
        std::uint64_t hash;
    };

    std::array<Offset, size> offsets {};
    std::array<ValueHashPair, size> values {};
};
#endif //PERFECT_HASHMAP_H
