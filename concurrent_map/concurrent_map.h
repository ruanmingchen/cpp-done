//
// Created by root on 23-4-4.
//

#ifndef A_OUT_CONCURRENTMAPT_H
#define A_OUT_CONCURRENTMAPT_H


#include <atomic>
#include <iostream>
#include <map>
#include <vector>


#include "boost/thread/locks.hpp"
#include "boost/thread/shared_mutex.hpp"

template <class KEY_T>
class CompInterface
{
public:
    virtual bool operator()(KEY_T key1, KEY_T key2) = 0;
};

template <class KEY_T, class VALUE_T, class Compare>
class ConcurrentBucket
{
public:
    ConcurrentBucket() = default;

    virtual ~ConcurrentBucket()
    {
        write_lock lock(rwlock_);
        map_.clear();
    }
    bool Lookup(const KEY_T& key, VALUE_T& value)
    {
        bool ret = false;
        read_lock lock(rwlock_);
        auto find = map_.find(key);
        if (find != map_.end()) {
            value = (*find).second;
            ret = true;
        }
        return ret;
    }

    uint64_t Size()
    {
        read_lock lock(rwlock_);
        return map_.size();
    }

    void Clear()
    {
        write_lock lock(rwlock_);
        map_.clear();
    }

    bool Contain(const KEY_T& key)
    {
        bool ret = false;
        read_lock lock(rwlock_);
        auto find = map_.find(key);
        if (find != map_.end()) {
            ret = true;
        }
        return ret;
    }
    bool Insert(const KEY_T& key, const VALUE_T& value)
    {
        bool ret = false;
        {
            write_lock lock(rwlock_);
            ret = InsertWithoutLock(key, value);
        }
        return ret;
    }

    bool Update(const KEY_T& key, const VALUE_T& value)
    {
        bool ret = false;
        {
            write_lock lock(rwlock_);
            ret = UpdateWithoutLock(key, value);
        }
        return ret;
    }

    void Remove(const KEY_T& key)
    {
        {
            write_lock lock(rwlock_);
            RemoveWithoutLock(key);
        }
    }

    void GetAllKey(std::vector<std::pair<KEY_T, VALUE_T>>& list)
    {
        read_lock lock(rwlock_);
        auto iter = map_.begin();
        for (; iter != map_.end(); ++iter) {
            list.push_back(std::pair<KEY_T, VALUE_T>(iter->first, iter->second));
        }
    }

    void UpdateKeyBatch(std::vector<std::pair<KEY_T, VALUE_T>>& list)
    {
        {
            write_lock lock(rwlock_);
            auto iter = list.begin();
            for (; iter != list.end(); ++iter) {
                UpdateWithoutLock(iter->first, iter->second);
            }
        }
    }

    void InsertKeyBatch(std::vector<std::pair<KEY_T, VALUE_T>>& list)
    {
        {
            write_lock lock(rwlock_);
            auto iter = list.begin();
            for (; iter != list.end(); ++iter) {
                InsertWithoutLock(iter->first, iter->second);
            }
        }
    }

    void RemoveKeyBatch(std::vector<KEY_T>& list)
    {
        {
            write_lock lock(rwlock_);
            auto iter = list.begin();
            for (; iter != list.end(); ++iter) {
                RemoveWithoutLock((*iter));
            }
        }
    }

private:
    void RemoveWithoutLock(const KEY_T& key)
    {
        auto find = map_.find(key);
        if (find != map_.end()) {
            map_.erase(find);
        }
    }
    bool InsertWithoutLock(const KEY_T& key, const VALUE_T& value)
    {
        bool ret = false;
        auto find = map_.find(key);
        if (find == map_.end()) {
            map_.insert(std::pair<KEY_T, VALUE_T>(key, value));
            ret = true;
        } else {
            if (Compare()(value, find->second)) {
                map_.erase(find);
                map_.insert(std::pair<KEY_T, VALUE_T>(key, value));
                ret = true;
            }
        }

        return ret;
    }

    bool UpdateWithoutLock(const KEY_T& key, const VALUE_T& value)
    {
        bool ret = false;
        auto find = map_.find(key);
        if (find != map_.end()) {
            if (Compare()(value, find->second)) {
                map_.erase(find);
                map_.insert(std::pair<KEY_T, VALUE_T>(key, value));
                ret = true;
            }
        }

        return ret;
    }

private:
    typedef boost::shared_lock<boost::shared_mutex> read_lock;
    typedef boost::unique_lock<boost::shared_mutex> write_lock;

    std::map<KEY_T, VALUE_T> map_;

    boost::shared_mutex rwlock_;
};

template <typename KEY_T, typename VALUE_T, typename Compare, typename Hash = std::hash<KEY_T>>
class ConcurrentMap
{
public:
    explicit ConcurrentMap(uint64_t bucket_nums = 61, Hash const& hasher = Hash()) : buckets_(bucket_nums), hasher_(hasher)
    {
        for (uint64_t i = 0; i < bucket_nums; ++i) {
            buckets_[i].reset(new ConcurrentBucket<KEY_T, VALUE_T, Compare>);
        }
    }

    bool LookUp(const KEY_T& key, VALUE_T& value) { return GetBucket(key).Lookup(key, value); }

    bool Contain(const KEY_T& key) { return GetBucket(key).Contain(key); }

    bool Insert(const KEY_T& key, const VALUE_T& value) { return GetBucket(key).Insert(key, value); }

    bool Update(const KEY_T& key, const VALUE_T& value) { return GetBucket(key).Update(key, value); }

    bool Delete(const KEY_T& key)
    {
        GetBucket(key).Remove(key);
        return true;
    }

    void GetAllKey(std::vector<std::pair<KEY_T, VALUE_T>>& list)
    {
        list.clear();
        for (size_t bucket_index = 0; bucket_index < buckets_.size(); ++bucket_index) {
            buckets_[bucket_index]->GetAllKey(list);
        }

    }

    void UpdateKeyBatch(std::vector<std::pair<KEY_T, VALUE_T>>& list)
    {
        std::vector<std::vector<std::pair<KEY_T, VALUE_T>>> update_lists(buckets_.size());

        typename std::vector<std::pair<KEY_T, VALUE_T>>::iterator iter = list.begin();
        for (; iter != list.end(); ++iter) {
            std::size_t bucket_index = hasher_(iter->first) % buckets_.size();
            update_lists[bucket_index].push_back(std::pair<KEY_T, VALUE_T>(iter->first, iter->second));
        }

        for (size_t bucket_index = 0; bucket_index < buckets_.size(); ++bucket_index) {
            buckets_[bucket_index]->UpdateKeyBatch(update_lists[bucket_index]);
        }
    }

    void RemoveKeyBatch(std::vector<KEY_T>& list)
    {
        std::vector<std::vector<KEY_T>> remove_lists(buckets_.size());

        typename std::vector<KEY_T>::iterator iter = list.begin();
        for (; iter != list.end(); ++iter) {
            std::size_t bucket_index = hasher_(*iter) % buckets_.size();
            remove_lists[bucket_index].push_back(*iter);
        }
        for (size_t bucket_index = 0; bucket_index < buckets_.size(); ++bucket_index) {
            buckets_[bucket_index]->RemoveKeyBatch(remove_lists[bucket_index]);
        }
    }

    void InsertKeyBatch(std::vector<std::pair<KEY_T, VALUE_T>>& list)
    {
        std::vector<std::vector<std::pair<KEY_T, VALUE_T>>> Insert_lists(buckets_.size());

        typename std::vector<std::pair<KEY_T, VALUE_T>>::iterator iter = list.begin();
        for (; iter != list.end(); ++iter) {
            std::size_t bucket_index = hasher_(iter->first) % buckets_.size();
            Insert_lists[bucket_index].push_back(std::pair<KEY_T, VALUE_T>(iter->first, iter->second));
        }

        for (size_t bucket_index = 0; bucket_index < buckets_.size(); ++bucket_index) {
            buckets_[bucket_index]->InsertKeyBatch(Insert_lists[bucket_index]);
        }
    }

    ConcurrentBucket<KEY_T, VALUE_T, Compare>& GetBucket(KEY_T const& key) const
    {
        std::size_t const bucket_index = hasher_(key) % buckets_.size();
        return *buckets_[bucket_index];
    }

    uint64_t Size()
    {
        uint64_t total_size = 0;
        for (size_t i = 0; i < buckets_.size(); ++i) {
            total_size += buckets_[i]->Size();
        }
        return total_size;
    }

    void Clear()
    {
        for (size_t i = 0; i < buckets_.size(); ++i) {
            buckets_[i]->Clear();
        }
    }

    ConcurrentMap(ConcurrentMap const& other) = delete;

    ConcurrentMap& operator=(ConcurrentMap const& other) = delete;

private:
    std::vector<std::unique_ptr<ConcurrentBucket<KEY_T, VALUE_T, Compare>>> buckets_;
    Hash hasher_;
};


#endif //A_OUT_CONCURRENTMAPT_H
