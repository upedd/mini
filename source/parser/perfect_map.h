#ifndef PERFET_HASHMAP_H
#define PERFET_HASHMAP_H
#include <algorithm>
#include <string_view>
#include <vector>
#include <ranges>

// heavily inspired from http://stevehanov.ca/blog/index.php?id=119
// investigate gperf for better performance

/**
 * Compile time constructed unordered map with O(1) worst case lookup due to perfect hashing
 * @tparam V value type
 * @tparam size Number of key-value pairs
 */
template<typename V, std::size_t size>
class perfect_map {
public:
    using kv_pair = std::pair<std::string_view, V>;
    using dict_t = std::array<kv_pair, size>;

    constexpr explicit perfect_map(dict_t dictionary) {
        std::array<std::vector<std::string_view>, size> buckets;
        std::array<bool, size> has_value {};

        for (auto& key : dictionary | std::views::keys) {
            buckets[fnv1(0, key) % size].push_back(key);
        }

        std::ranges::sort(buckets, [](auto& a, auto& b) {
            return a.size() > b.size();
        });

        int b = 0;

        for (; b < buckets.size(); ++b) {
            auto& bucket = buckets[b];
            if (bucket.size() <= 1) break;
            unsigned int d = 0;
            int item = 0;
            std::vector<unsigned int> slots(size);
            while (item < bucket.size()) {
                unsigned int slot = fnv1(d, bucket[item]) % size;
                if (has_value[slot] || std::ranges::contains(slots, slot)) {
                    d += 1;
                    item = 0;
                    slots.clear();
                } else {
                    slots.push_back(slot);
                    item += 1;
                }
            }
            auto ss = fnv1(0, bucket[0]);
            g[ss % size] = d;
            for (int i = 0; i < size; ++i) {
                values[slots[i]] = get_from_dictionary_slow(dictionary, bucket[i]);
                has_value[slots[i]] = true;
            }
        }

        std::vector<int> freelist;
        for (int i = 0; i < size; ++i) {
            if (!has_value[i]) {
                freelist.push_back(i);
            }
        }

        for (; b < buckets.size(); ++b) {
            auto& bucket = buckets[b];
            if (bucket.empty()) break;
            int slot = freelist.back(); freelist.pop_back();
            g[fnv1(0, bucket[0]) % size] = -slot - 1;
            values[slot] = get_from_dictionary_slow(dictionary, bucket[0]);
        }
    }

    std::optional<V> constexpr operator[](std::string_view key) const {
        long long d = g[fnv1(0, key) % static_cast<long long>(g.size())];
        if (d < 0) {
            return values[- d - 1];
        }
        return values[fnv1(d, key) % values.size()];
    }
private:
    constexpr static std::uint64_t FNV_offset_basis = 0xcbf29ce484222325;
    constexpr static std::uint64_t FNV_prime = 0x100000001b3;

    // TODO: check
     [[nodiscard]] constexpr static unsigned int fnv1(std::string_view string, std::uint64_t offset = FNV_offset_basis) {
         std::uint64_t hash = offset;
        for (char c : string) {
            hash = hash * FNV_prime ^ static_cast<std::uint64_t>(c);
        }
        return hash;
    }
    [[nodiscard]] constexpr int get_from_dictionary_slow(const dict_t& dict, std::string_view key) {
        for (auto& [k, v] : dict) {
            if (k == key) return v;
        }
        std::unreachable();
    }

    std::array<long long, size> g {};
    std::array<V, size> values {};
};
#endif //PERFET_HASHMAP_H
