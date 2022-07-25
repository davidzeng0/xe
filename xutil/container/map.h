#pragma once
#include <robin_hood.h>
#include "../types.h"
#include "../mem.h"
#include "../hash.h"

template<typename key_t, typename value_t, class hash = xe_hash<key_t>, class equal = std::equal_to<key_t>>
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

		wrap(){}
		~wrap(){}
	};

	wrap<hashmap> map;
public:
	using iterator = typename hashmap::iterator;
	using const_iterator = typename hashmap::const_iterator;

	void init(){
		map.init();
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
	iterator insert(K&& key){
		try{
			return map -> insert_or_assign(std::forward<K>(key), value_t()).first;
		}catch(std::bad_alloc e){
			return end();
		}
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
	void erase(K&& key){
		map -> erase(std::forward<K>(key));
	}

	template<class K>
	iterator find(K&& key){
		return map -> find(std::forward<K>(key));
	}

	void erase(iterator it){
		map -> erase(it);
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

	const_iterator begin() const{
		return map -> begin();
	}

	const_iterator end() const{
		return map -> end();
	}

	const_iterator cbegin() const{
		return map -> cbegin();
	}

	const_iterator cend() const{
		return map -> cend();
	}

	void free(){
		map.free();
	}
};