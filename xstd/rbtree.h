#pragma once
#include "types.h"

template<typename T>
class xe_rbtree{
private:
	enum rbcolor{
		BLACK = 0x0,
		RED = 0x1
	};

	class rbnode{
	private:
		rbnode* left;
		rbnode* right;
		rbnode* parent;
		rbcolor color;

		template<typename U>
		friend class xe_rbtree;
	public:
		T key;
	};

	template<typename node_t>
	class rbiterator{
	private:
		node_t* node;
	public:
		rbiterator(): node(){}
		rbiterator(node_t* node): node(node){}

		node_t* operator->() const{
			return node;
		}

		node_t& operator*() const{
			return *node;
		}

		rbiterator& operator++(){
			node = next(node);

			return *this;
		}

		rbiterator operator++(int){
			rbiterator tmp = *this;

			node = next(node);

			return tmp;
		}

		rbiterator& operator--(){
			node = prev(node);
		}

		rbiterator operator--(int){
			rbiterator tmp = *this;

			node = prev(node);

			return tmp;
		}

		bool operator==(const rbiterator& other) const{
			return node == other.node;
		}

		~rbiterator() = default;
	};

	static rbnode* iterate(rbnode* node, rbnode* rbnode::*a, rbnode* rbnode::*b){
		rbnode* parent;

		if(node ->* b){
			node = node ->* b;

			while(node ->* a)
				node = node ->* a;
		}else{
			parent = node -> parent;

			while(parent && node == parent ->* b){
				node = parent;
				parent = node -> parent;
			}

			node = parent;
		}

		return node;
	}

	static rbnode* next(rbnode* node){
		return iterate(node, &rbnode::left, &rbnode::right);
	}

	static rbnode* prev(rbnode* node){
		return iterate(node, &rbnode::right, &rbnode::left);
	}

	void change_child(rbnode* parent, rbnode* from, rbnode* to){
		if(!parent)
			root = to;
		else if(parent -> left == from)
			parent -> left = to;
		else
			parent -> right = to;
	}

	void shift_down(rbnode* node, rbnode* parent, rbnode* rbnode::*a, rbnode* rbnode::*b){
		rbnode* tmp;

		/* move node's parent to node's child */
		tmp = node ->* a;
		node ->* a = parent;
		parent -> parent = node;
		parent ->* b = tmp;

		if(tmp) tmp -> parent = parent;
	}

	void rotate(rbnode* node, rbnode* parent, rbcolor node_color, rbcolor parent_color, rbnode* rbnode::*a, rbnode* rbnode::*b){
		rbnode* tmp;

		/*
		 * node is child or grandchild of parent
		 * move node to the top of the subtree
		 * and move parent to node's side
		 */

		/* update parents (and root) */
		tmp = parent -> parent;
		node -> parent = tmp;

		change_child(tmp, parent, node);

		/* move down parent */
		shift_down(node, parent, b, a);

		/* update color */
		node -> color = node_color;
		parent -> color = parent_color;
	}

	void rotate_swap(rbnode* node, rbnode* parent, rbnode* gparent, rbcolor node_color, rbcolor gparent_color, rbnode* rbnode::*a, rbnode* rbnode::*b){
		rotate(node, gparent, node_color, gparent_color, a, b);
		shift_down(node, parent, a, b);
	}

	void insert(rbnode* node){
		rbnode** link;
		rbnode* parent, *gparent, *uncle;

		node -> left = null;
		node -> right = null;
		size_++;

		if(!root){
			root = node;
			begin_ = node;
			node -> color = BLACK;
			node -> parent = null;

			return;
		}

		link = &root;

		while(*link){
			parent = *link;

			if(node -> key < parent -> key)
				link = &parent -> left;
			else
				link = &parent -> right;
		}

		if(parent == begin_ && link == &parent -> left)
			begin_ = node;
		*link = node;
		node -> parent = parent;
		node -> color = RED;

		while(parent -> color != BLACK){
			gparent = parent -> parent;
			uncle = gparent -> left;

			if(uncle == parent)
				uncle = gparent -> right;
			if(uncle && uncle -> color == RED){
				parent -> color = BLACK;
				uncle -> color = BLACK;
				node = gparent;
				parent = node -> parent;

				if(parent){
					node -> color = RED;

					continue;
				}else{
					/* we are at root */
					return;
				}
			}

			if(parent == gparent -> left){
				if(node == parent -> left){
					/* left - left */
					rotate(parent, gparent, BLACK, RED, &rbnode::left, &rbnode::right);
				}else{
					/* left - right */
					rotate_swap(node, parent, gparent, BLACK, RED, &rbnode::left, &rbnode::right);
				}
			}else{
				if(node == parent -> left){
					/* right - left */
					rotate_swap(node, parent, gparent, BLACK, RED, &rbnode::right, &rbnode::left);
				}else{
					/* right - right */
					rotate(parent, gparent, BLACK, RED, &rbnode::right, &rbnode::left);
				}
			}

			break;
		}
	}

	void balance_rotate(rbnode* child, rbnode* sibling, rbnode* parent, rbnode* rbnode::*a, rbnode* rbnode::*b){
		rotate(sibling, parent, parent -> color, BLACK, a, b);

		child -> color = BLACK;
	}

	bool balance(rbnode*& sibling, rbnode* parent, rbnode* rbnode::*a, rbnode* rbnode::*b){
		rbnode* child;

		if(sibling -> color == RED){
			rotate(sibling, parent, BLACK, RED, a, b);

			sibling = parent -> right;
		}

		child = sibling -> left;

		if(child && child -> color == RED){
			if(a == &rbnode::left)
				balance_rotate(child, sibling, parent, a, b);
			else
				rotate_swap(child, sibling, parent, parent -> color, BLACK, a, b);
			return true;
		}

		child = sibling -> right;

		if(child && child -> color == RED){
			if(a == &rbnode::left)
				rotate_swap(child, sibling, parent, parent -> color, BLACK, a, b);
			else
				balance_rotate(child, sibling, parent, a, b);
			return true;
		}

		return false;
	}

	void balance(rbnode* parent){
		rbnode* node, *sibling;

		node = null;

		while(parent){
			sibling = parent -> left;

			if(sibling == node){
				sibling = parent -> right;

				if(balance(sibling, parent, &rbnode::right, &rbnode::left))
					break;
			}else if(balance(sibling, parent, &rbnode::left, &rbnode::right)){
				break;
			}

			sibling -> color = RED;

			if(parent -> color == RED){
				parent -> color = BLACK;

				break;
			}else{
				node = parent;
				parent = parent -> parent;
			}
		}
	}

	void move_child(rbnode* to, rbnode* from, rbnode* rbnode::*side){
		rbnode* child = from ->* side;

		to ->* side = child;

		if(child) child -> parent = to;
	}

	rbnode* replace(rbnode* node, rbnode* replacement){
		/* single child replacement */
		change_child(node -> parent, node, replacement);

		if(replacement){
			replacement -> color = BLACK;
			replacement -> parent = node -> parent;
		}else if(node -> color == BLACK){
			return node -> parent;
		}

		return null;
	}

	void erase(rbnode* node){
		rbnode* double_black, *successor, *parent, *tmp;

		double_black = null;
		size_--;

		if(node == begin_)
			begin_ = next(begin_);
		if(!node -> left)
			double_black = replace(node, node -> right);
		else if(!node -> right)
			double_black = replace(node, node -> left);
		else{
			successor = node -> right;

			if(successor -> left){
				do{
					successor = successor -> left;
				}while(successor -> left);

				parent = successor -> parent;
				tmp = successor -> right;

				move_child(parent, tmp, &rbnode::left);
				move_child(successor, node, &rbnode::right);
			}else{
				tmp = successor -> right;
			}

			if(tmp)
				tmp -> color = BLACK;
			else if(successor -> color == BLACK)
				double_black = parent;
			move_child(successor, node, &rbnode::left);
			change_child(node -> parent, node, successor);

			successor -> parent = node -> parent;
			successor -> color = node -> color;
		}

		if(double_black) balance(double_black);
	}

	rbnode* root;
	rbnode* begin_;
	size_t size_;
public:
	using iterator = rbiterator<rbnode>;
	using const_iterator = rbiterator<const rbnode>;
	using node = rbnode;

	xe_rbtree(){
		root = null;
		begin_ = null;
		size_ = 0;
	}

	iterator insert(rbnode& node){
		insert(&node);

		return iterator(&node);
	}

	iterator erase(rbnode& node){
		erase(&node);

		return iterator(begin_);
	}

	iterator erase(iterator it){
		return erase(*it);
	}

	size_t size(){
		return size_;
	}

	iterator begin(){
		return iterator{begin_};
	}

	iterator end(){
		return iterator(null);
	}

	const_iterator cbegin(){
		return const_iterator(begin_);
	}

	const_iterator cend(){
		return const_iterator(null);
	}

	iterator find(const T& key){
		rbnode* node = root;

		while(node){
			if(key < node -> key)
				node = node -> left;
			else if(key == node -> key)
				break;
			else
				node = node -> right;
		}

		return iterator(node);
	}

	~xe_rbtree() = default;
};