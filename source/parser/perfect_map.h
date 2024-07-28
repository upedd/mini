#ifndef PERFET_HASHMAP_H
#define PERFET_HASHMAP_H
#include <algorithm>
#include <string_view>
#include <vector>
#include <ranges>
#include <unordered_map>
#include <unordered_set>

// inspired from http://stevehanov.ca/blog/index.php?id=119
// investigate gperf algo


template<typename V, std::size_t size>
class perfect_map {
public:
    using kv_pair = std::pair<std::string, V>;

    constexpr explicit perfect_map(std::array<kv_pair, size> dictionary)  {
        // string instead of string_view?
        std::array<std::vector<std::string_view>, size> buckets;
        std::array<bool, size> has_value;
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
            std::vector<unsigned int> slots;
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
                values[slots[i]] = dictionary[bucket[i]];
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
            values[slot] = dictionary[bucket[0]];
        }
    }

    constexpr std::optional<V> operator[](std::string_view key) {
        long long d = g[fnv1(0, key) % static_cast<long long>(g.size())];
        if (d < 0) {
            return values[- d - 1];
        }
        return values[fnv1(d, key) % values.size()];
    }
private:
    // TODO: check
    constexpr unsigned int fnv1(unsigned int d, std::string_view string) {
        if (d == 0) {
            d = 0x01000193;
        }
        for (char c : string) {
            d = d * 0x01000193 ^ static_cast<int>(c);
        }
        return d;
    }

    std::array<long long, size> g;
    std::array<V, size> values;

};
#endif //PERFET_HASHMAP_H
