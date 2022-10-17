#pragma once
#include "xstd/types.h"
#include "xutil/util.h"
#include "xutil/assert.h"

class xe_linked_node{
	union{
		struct{
			xe_linked_node* prev_;
			xe_linked_node* next_;
		};

		struct{
			xe_linked_node* tail;
			xe_linked_node* head;
		};
	};

	friend class xe_linked_list;
public:
	constexpr xe_linked_node(): prev_(), next_(){}

	constexpr xe_linked_node* prev(){
		return prev_;
	}

	constexpr xe_linked_node* next(){
		return next_;
	}

	constexpr const xe_linked_node* prev() const{
		return prev_;
	}

	constexpr const xe_linked_node* next() const{
		return next_;
	}

	constexpr void detach(){
		xe_assert(prev_);

		prev_ -> next_ = next_;
		next_ -> prev_ = prev_;
	}

	constexpr void erase(){
		detach();

		prev_ = null;
		next_ = null;
	}

	constexpr bool in_list() const{
		return prev_ != null;
	}

	constexpr operator bool() const{
		return in_list();
	}

	~xe_linked_node() = default;
};

template<class node_t>
class xe_linked_iterator{
private:
	node_t* node;
public:
	constexpr xe_linked_iterator(): node(){}
	constexpr xe_linked_iterator(node_t* node): node(node){}

	constexpr node_t* operator->() const{
		return node;
	}

	constexpr node_t& operator*() const{
		return *node;
	}

	constexpr xe_linked_iterator& operator++(){
		node = node -> next();

		return *this;
	}

	constexpr xe_linked_iterator operator++(int){
		xe_linked_iterator tmp = *this;

		node = node -> next();

		return tmp;
	}

	constexpr xe_linked_iterator& operator--(){
		node = node -> prev();
	}

	constexpr xe_linked_iterator operator--(int){
		xe_linked_iterator tmp = *this;

		node = node -> prev();

		return tmp;
	}

	constexpr bool operator==(const xe_linked_iterator& other) const{
		return node == other.node;
	}

	constexpr ~xe_linked_iterator() = default;
};

class xe_linked_list{
	xe_linked_node list;
public:
	typedef xe_linked_iterator<xe_linked_node> iterator;
	typedef xe_linked_iterator<const xe_linked_node> const_iterator;

	/* circularly & doubly linked list */
	constexpr xe_linked_list(){
		/* &list means null */
		list.tail = &list;
		list.head = &list;
	}

	xe_disallow_copy_move(xe_linked_list)

	constexpr xe_linked_node& head(){
		return *list.head;
	}

	constexpr xe_linked_node& tail(){
		return *list.tail;
	}

	constexpr void prepend(xe_linked_node& node){
		node.prev_ = &list;
		node.next_ = list.head;
		list.head -> prev_ = &node;
		list.head = &node;
	}

	constexpr void append(xe_linked_node& node){
		node.prev_ = list.tail;
		node.next_ = &list;
		list.tail -> next_ = &node;
		list.tail = &node;
	}

	constexpr void detach(xe_linked_node& node){
		node.detach();
	}

	constexpr void erase(xe_linked_node& node){
		node.erase();
	}

	constexpr iterator begin(){
		return iterator(list.head);
	}

	constexpr iterator end(){
		return iterator(&list);
	}

	constexpr const_iterator begin() const{
		return const_iterator(list.head);
	}

	constexpr const_iterator end() const{
		return const_iterator(&list);
	}

	constexpr const_iterator cbegin() const{
		return const_iterator(list.head);
	}

	constexpr const_iterator cend() const{
		return const_iterator(&list);
	}

	constexpr xe_linked_node& front(){
		xe_assert(list.head != &list);

		return *list.head;
	}

	constexpr const xe_linked_node& front() const{
		xe_assert(list.head != &list);

		return *list.head;
	}

	constexpr xe_linked_node& back(){
		xe_assert(list.tail != &list);

		return *list.tail;
	}

	constexpr const xe_linked_node& back() const{
		xe_assert(list.tail != &list);

		return *list.tail;
	}

	constexpr bool empty() const{
		return list.head == &list;
	}

	constexpr operator bool() const{
		return !empty();
	}

	~xe_linked_list() = default;
};