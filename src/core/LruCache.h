#pragma once

#include <list>
#include <optional>
#include <unordered_map>

template <typename Key, typename Value>
class LruCache {
public:
    explicit LruCache(std::size_t maxItems = 128)
        : maxItems_(maxItems)
    {
    }

    [[nodiscard]] std::optional<Value> get(const Key& key)
    {
        const auto it = map_.find(key);
        if (it == map_.end()) {
            return std::nullopt;
        }
        order_.splice(order_.begin(), order_, it->second.second);
        return it->second.first;
    }

    void put(const Key& key, Value value)
    {
        const auto it = map_.find(key);
        if (it != map_.end()) {
            it->second.first = std::move(value);
            order_.splice(order_.begin(), order_, it->second.second);
            return;
        }
        order_.push_front(key);
        map_.emplace(key, std::make_pair(std::move(value), order_.begin()));
        evictIfNeeded();
    }

    void clear()
    {
        map_.clear();
        order_.clear();
    }

    [[nodiscard]] std::size_t size() const { return map_.size(); }

    void setMaxItems(std::size_t maxItems)
    {
        maxItems_ = maxItems;
        evictIfNeeded();
    }

private:
    std::size_t maxItems_;
    std::list<Key> order_;
    std::unordered_map<Key, std::pair<Value, typename std::list<Key>::iterator>> map_;

    void evictIfNeeded()
    {
        while (map_.size() > maxItems_) {
            const Key& oldest = order_.back();
            map_.erase(oldest);
            order_.pop_back();
        }
    }
};
