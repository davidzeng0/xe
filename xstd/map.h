#pragma once
#include "robin-hood-hashing/src/include/robin_hood.h"
#include "std.h"
#include "hash.h"
#include "xutil/mem.h"
#include "xutil/util.h"

template<typename key_t, typename value_t, class hash = xe_hash<key_t>, class equal = std::equal_to<key_t>>
class xe_map{
private:
	typedef robin_hood::unordered_flat_map<key_t, value_t, hash, equal> unordered_map;

	unordered_map map;
public:
	typedef typename unordered_map::iterator iterator;
	typedef typename unordered_map::const_iterator const_iterator;
	typedef key_t key_type;
	typedef value_t value_type;

	xe_map() = default;

	xe_map(xe_map&& other): map(std::move(other.map)){}
	xe_map& operator=(xe_map&& other){
		map = std::move(other.map);

		return *this;
	}

	xe_disable_copy(xe_map)

	template<class K, class V>
	bool insert(K&& key, V&& value){
		try{
			map.insert_or_assign(std::forward<K>(key), std::forward<V>(value));
		}catch(...){
			return false;
		}

		return true;
	}

	template<class K>
	iterator insert(K&& key){
		try{
			return map.try_emplace(std::forward<K>(key), value_t()).first;
		}catch(...){
			return end();
		}
	}

	template<class K, class V>
	bool emplace(K&& key, V&& value){
		try{
			map.try_emplace(std::forward<K>(key), std::forward<V>(value));
		}catch(...){
			return false;
		}

		return true;
	}

	template<class K>
	void erase(K&& key){
		map.erase(std::forward<K>(key));
	}

	template<class K>
	iterator find(K&& key){
		return map.find(std::forward<K>(key));
	}

	void erase(iterator it){
		map.erase(it);
	}

	template<class K>
	bool has(K&& key){
		return find(std::forward<K>(key)) != end();
	}

	bool empty(){
		return map.empty();
	}

	size_t size(){
		return map.size();
	}

	iterator begin(){
		return map.begin();
	}

	iterator end(){
		return map.end();
	}

	const_iterator begin() const{
		return map.begin();
	}

	const_iterator end() const{
		return map.end();
	}

	const_iterator cbegin() const{
		return map.cbegin();
	}

	const_iterator cend() const{
		return map.cend();
	}

	void trim(){
		try{
			map.compact();
		}catch(...){}
	}

	void clear(){
		xe_destruct(&map);
		xe_construct(&map);
	}

	~xe_map() = default;
};