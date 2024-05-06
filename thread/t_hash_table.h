#ifndef __HASH_TABLE_
#define __HASH_TABLE_

#include "common.h"

template <typename Key, typename Value, typename  Hash = std::hash<Key> >
class lookup_table{
public:
    typedef Key key_type;
    typedef Value mapped_type;
    typedef Hash hash_type;

    lookup_table(unsigned num_buckets = 19, Hash const & hasher_ = Hash())
        :buckets(num_buckets),hasher(hasher_){
        for(unsigned i = 0; i < num_buckets; ++i){
            buckets[i] = new bucket_type;
        }
    }

    lookup_table(lookup_table const & other) = delete;
    lookup_table &operator=(lookup_table const & other) = delete;

    ~lookup_table(){
        for(unsigned i = 0; i < buckets.size(); ++i){
            delete buckets[i];
        }
    }

    Value value_for(Key const & key, Value const & default_value = Value()) const{
        bucket_type & bucket = get_bucket(key);
        std::shared_lock<std::shared_mutex> lock(bucket.mutex);
        bucket_iterator const found_entry = bucket.find_entry_for(key);
        return (found_entry == bucket.data.end()) ? default_value : found_entry->second;
    }

    void add_or_upadate_mapping(Key const & key,Value const & value){
        bucket_type & bucket = get_bucket(key);
        std::unique_lock<std::shared_mutex> lock(bucket.mutex);
        bucket_iterator const found_entry = bucket.find_entry_for(key);
        if(found_entry == bucket.data.end()){
            bucket.data.push_back(bucket_value(key,value));
        } else {
            found_entry->second = value;
        }
    }

    void remove_mapping(Key const & key){
        bucket_type & bucket = get_bucket(key);
        std::unique_lock<std::shared_mutex> lock(bucket.mutex);
        bucket_iterator const found_entry = bucket.find_entry_for(key);
        if(found_entry != bucket.data.end()){
            bucket.data.erase(found_entry);
        }
    }

    std::map<Key,Value> get_map() const{
        std::vector<std::unique_lock<std::shared_mutex> > locks;
        for(unsigned i = 0; i < buckets.size(); ++i){
            locks.push_back(std::unique_lock<std::shared_mutex>(buckets[i].mutex));
        }
        std::map<Key,Value> res;
        for(unsigned i = 0; i < buckets.size(); ++i){
            for(bucket_iterator it = buckets[i].data.begin(); it != buckets[i].data.end(); ++it){
                res.insert(*it);
            }
        }
        return res;
    }

private:
    typedef std::pair<Key,Value> bucket_value;
    typedef std::list<bucket_value> bucket_data;
    typedef typename bucket_data::iterator bucket_iterator;

    struct bucket_type{
        bucket_data data;
        std::shared_mutex mutex;
        bucket_iterator find_entry_for(Key const & key){
            return std::find_if(data.begin(),data.end(),[&](bucket_value const & item){return item.first == key;});
        }
    };

    bucket_type & get_bucket(Key const & key) const{
        std::size_t const bucket_index = hasher(key) % buckets.size();
        return *buckets[bucket_index];
    }

    std::vector<bucket_type *> buckets;
    Hash hasher;
};




#endif