#pragma once
#include <robin_hood.h>
#include "../types.h"
#include "../mem.h"

namespace xe_hash{

static inline size_t hash_bytes(xe_cptr data, size_t len){
	size_t n = sizeof(ulong) - 1;

	ulong seed = 0xe17a1465,
		m = 0xc6a4a7935bd1e995,
		r = 47;
	ulong h = seed ^ (len * m);
	ulong k;

	ulong* p = (ulong*)data;

	for(size_t i = 0; i < len >> 3; i++){
		k = *p++;

		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	len &= n;

	if(len){
		p--;
		k = *(ulong*)((xe_bptr)p + len);

		k >>= 64 - 8 * len;
		k *= m;
		k ^= k >> r;
		k *= m;

		h ^= k;
		h *= m;
	}

	h ^= h >> r;

	return h;
}

static inline size_t hash_int(ulong x){
	x ^= x >> 33;
	x *= 0xff51afd7ed558ccd;
	x ^= x >> 33;

	return x;
}

static inline size_t hash_combine(size_t h1, size_t h2){
	h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);

	return h1;
}

template<class T>
struct hash{
	size_t operator()(const T& key) const{
		return key.hash();
	}
};

}

template<typename key_t, typename value_t, class hash = xe_hash::hash<key_t>, class equal = std::equal_to<key_t>>
class xe_map{
private:
	typedef robin_hood::unordered_flat_map<key_t, value_t, hash, equal> hashmap;

	template<class T>
	struct wrap{
		union{
			T type;
			byte bytes[sizeof(T)];
		};

		T* operator->(){
			return &type;
		}

		void init() noexcept{
			xe_construct(&type);
		}

		void free(){
			xe_deconstruct(&type);
		}

		~wrap(){}

		wrap(){}
	};

	wrap<hashmap> map;
public:
	using iterator = typename hashmap::iterator;

	void init(){
		map.init();
	}

	void free(){
		map.free();
	}

	template<class K, class V>
	bool insert(K& key, V& value){
		try{
			map -> insert_or_assign(key, value);
		}catch(std::bad_alloc e){
			return false;
		}

		return true;
	}

	template<class K, class V>
	bool insert(K&& key, V&& value){
		try{
			map -> insert_or_assign(std::forward<K>(key), std::forward<V>(value));
		}catch(std::bad_alloc e){
			return false;
		}

		return true;
	}

	template<class K>
	iterator insert(K& key){
		try{
			return map -> insert_or_assign(key, value_t()).first;
		}catch(std::bad_alloc e){
			return end();
		}
	}

	template<class K>
	iterator insert(K&& key){
		try{
			return map -> insert_or_assign(std::forward<K>(key), value_t()).first;
		}catch(std::bad_alloc e){
			return end();
		}
	}

	template<class K, class V>
	bool emplace(K& key, V& value){
		try{
			map -> try_emplace(key, value);
		}catch(std::bad_alloc e){
			return false;
		}

		return true;
	}

	template<class K, class V>
	bool emplace(K&& key, V&& value){
		try{
			map -> try_emplace(std::forward<K>(key), std::forward<V>(value));
		}catch(std::bad_alloc e){
			return false;
		}

		return true;
	}

	template<class K>
	void erase(K& key){
		map -> erase(key);
	}

	template<class K>
	void erase(K&& key){
		map -> erase(std::forward<K>(key));
	}

	template<class K>
	iterator find(K& key){
		return map -> find(key);
	}

	template<class K>
	iterator find(K&& key){
		return map -> find(std::forward<K>(key));
	}

	void erase(iterator it){
		map -> erase(it);
	}

	template<class K>
	bool has(K& key){
		return find(key) != end();
	}

	template<class K>
	bool has(K&& key){
		return find(std::forward<K>(key)) != end();
	}

	void clear(){
		map -> clear();
	}

	iterator begin(){
		return map -> begin();
	}

	iterator end(){
		return map -> end();
	}
};