#pragma once
#include "xutil/util.h"
#include "types.h"

enum xe_rb_color{
	BLACK = 0x0,
	RED = 0x1
};

class xe_rb_node{
private:
	xe_rb_node* iterate(xe_rb_node* xe_rb_node::*a, xe_rb_node* xe_rb_node::*b){
		xe_rb_node* parent, *node = this;

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

	xe_rb_node* next(){
		return iterate(&xe_rb_node::left, &xe_rb_node::right);
	}

	xe_rb_node* prev(){
		return iterate(&xe_rb_node::right, &xe_rb_node::left);
	}

	xe_rb_node* left;
	xe_rb_node* right;
	xe_rb_node* parent;
	xe_rb_color color;

	template<class xe_node>
	friend class xe_rbtree;

	template<class xe_node>
	friend class xe_rb_iterator_base;
};

template<class xe_node>
class xe_rb_iterator_base{
private:
	xe_rb_node* node;
public:
	constexpr xe_rb_iterator_base(): node(){}
	constexpr xe_rb_iterator_base(xe_rb_node* node): node(node){}

	constexpr xe_node* operator->() const{
		return (xe_node*)node;
	}

	constexpr xe_node& operator*() const{
		return *(xe_node*)node;
	}

	constexpr xe_rb_iterator_base& operator++(){
		node = node -> next();

		return *this;
	}

	constexpr xe_rb_iterator_base operator++(int){
		xe_rb_iterator_base tmp = *this;

		node = node -> next();

		return tmp;
	}

	constexpr xe_rb_iterator_base& operator--(){
		node = node -> prev();
	}

	constexpr xe_rb_iterator_base operator--(int){
		xe_rb_iterator_base tmp = *this;

		node = node -> prev();

		return tmp;
	}

	constexpr bool operator==(const xe_rb_iterator_base& other) const{
		return node == other.node;
	}

	~xe_rb_iterator_base() = default;
};

typedef xe_rb_iterator_base<xe_rb_node> xe_rb_iterator;
typedef xe_rb_iterator_base<const xe_rb_node> xe_rb_const_iterator;

template<typename xe_node = xe_rb_node>
class xe_rbtree{
private:
	void change_child(xe_rb_node* parent, xe_rb_node* from, xe_rb_node* to){
		if(!parent)
			root = to;
		else if(parent -> left == from)
			parent -> left = to;
		else
			parent -> right = to;
	}

	void shift_down(xe_rb_node* node, xe_rb_node* parent, xe_rb_node* xe_rb_node::*a, xe_rb_node* xe_rb_node::*b){
		xe_rb_node* tmp;

		/* move node's parent to node's child */
		tmp = node ->* a;
		node ->* a = parent;
		parent -> parent = node;
		parent ->* b = tmp;

		if(tmp) tmp -> parent = parent;
	}

	void rotate(xe_rb_node* node, xe_rb_node* parent, xe_rb_color node_color, xe_rb_color parent_color, xe_rb_node* xe_rb_node::*a, xe_rb_node* xe_rb_node::*b){
		xe_rb_node* tmp;

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

	void rotate_swap(xe_rb_node* node, xe_rb_node* parent, xe_rb_node* gparent, xe_rb_color node_color, xe_rb_color gparent_color, xe_rb_node* xe_rb_node::*a, xe_rb_node* xe_rb_node::*b){
		/*
		 * move node from the bottom to the top
		 * grandparent moves down to node's side
		 */
		rotate(node, gparent, node_color, gparent_color, a, b);

		/* move down node's parent */
		shift_down(node, parent, a, b);
	}

	void insert(xe_rb_node* node){
		xe_rb_node** link;
		xe_rb_node* parent, *gparent, *uncle;

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

			if(*(xe_node*)node < *(xe_node*)parent)
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

			/* perform rotations */
			if(parent == gparent -> left){
				if(node == parent -> left){
					/* left - left */
					rotate(parent, gparent, BLACK, RED, &xe_rb_node::left, &xe_rb_node::right);
				}else{
					/* left - right */
					rotate_swap(node, parent, gparent, BLACK, RED, &xe_rb_node::left, &xe_rb_node::right);
				}
			}else{
				if(node == parent -> left){
					/* right - left */
					rotate_swap(node, parent, gparent, BLACK, RED, &xe_rb_node::right, &xe_rb_node::left);
				}else{
					/* right - right */
					rotate(parent, gparent, BLACK, RED, &xe_rb_node::right, &xe_rb_node::left);
				}
			}

			break;
		}
	}

	void balance_rotate(xe_rb_node* child, xe_rb_node* sibling, xe_rb_node* parent, xe_rb_node* xe_rb_node::*a, xe_rb_node* xe_rb_node::*b){
		rotate(sibling, parent, parent -> color, BLACK, a, b);

		child -> color = BLACK;
	}

	bool balance(xe_rb_node*& sibling, xe_rb_node* parent, xe_rb_node* xe_rb_node::*a, xe_rb_node* xe_rb_node::*b){
		xe_rb_node* child, *tmp;

		if(sibling -> color == RED){
			/*
			 * red sibling case
			 * sibling becomes the new parent
			 * look for red children in sibling's opposite child (new sibling)
			 * and perform optimized rotations
			 */
			tmp = parent -> parent;
			sibling -> parent = tmp;
			sibling -> color = BLACK;

			change_child(tmp, parent, sibling);

			child = sibling ->* b;
			tmp = child ->* a;

			if(tmp && tmp -> color == RED){
				/* left - left & right - right */
				sibling ->* b = child;
				child -> parent = sibling;

				shift_down(child, parent, b, a);

				child -> color = RED;
				tmp -> color = BLACK;

				return true;
			}

			tmp = child ->* b;

			if(tmp && tmp -> color == RED){
				/* left - right & right - left */
				sibling ->* b = tmp;
				tmp -> parent = sibling;

				shift_down(tmp, parent, b, a);
				shift_down(tmp, child, a, b);

				tmp -> color = RED;

				return true;
			}

			/* no red children, finish initial rotation */
			shift_down(sibling, parent, b, a);

			parent -> color = RED;
			sibling = parent ->* a;

			return false;
		}

		child = sibling -> left;

		if(child && child -> color == RED){
			if(a == &xe_rb_node::left){
				/* left - left */
				balance_rotate(child, sibling, parent, a, b);
			}else{
				/* right - left */
				rotate_swap(child, sibling, parent, parent -> color, BLACK, a, b);
			}

			return true;
		}

		child = sibling -> right;

		if(child && child -> color == RED){
			if(a == &xe_rb_node::left){
				/* left - right */
				rotate_swap(child, sibling, parent, parent -> color, BLACK, a, b);
			}else{
				/* right - right */
				balance_rotate(child, sibling, parent, a, b);
			}

			return true;
		}

		return false;
	}

	void balance(xe_rb_node* parent){
		xe_rb_node* node, *sibling;

		node = null;

		while(parent){
			sibling = parent -> left;

			if(sibling == node){
				sibling = parent -> right;

				if(balance(sibling, parent, &xe_rb_node::right, &xe_rb_node::left))
					break;
			}else if(balance(sibling, parent, &xe_rb_node::left, &xe_rb_node::right)){
				break;
			}

			/* no red children */
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

	void move_child(xe_rb_node* to, xe_rb_node* child, xe_rb_node* xe_rb_node::*side){
		to ->* side = child;

		if(child) child -> parent = to;
	}

	xe_rb_node* replace(xe_rb_node* node, xe_rb_node* replacement){
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

	void erase(xe_rb_node* node){
		xe_rb_node* double_black, *successor, *parent, *tmp;

		double_black = null;
		size_--;

		if(node == begin_)
			begin_ = begin_ -> next();
		if(!node -> left)
			double_black = replace(node, node -> right);
		else if(!node -> right)
			double_black = replace(node, node -> left);
		else{
			/*
			 * two children
			 * find successor, delete successor, replace node with successor
			 */
			successor = node -> right;

			if(successor -> left){
				do{
					successor = successor -> left;
				}while(successor -> left);

				parent = successor -> parent;
				tmp = successor -> right;

				move_child(parent, tmp, &xe_rb_node::left);
				move_child(successor, node -> right, &xe_rb_node::right);
			}else{
				parent = successor;
				tmp = successor -> right;
			}

			if(tmp)
				tmp -> color = BLACK;
			else if(successor -> color == BLACK)
				double_black = parent;
			move_child(successor, node -> left, &xe_rb_node::left);
			change_child(node -> parent, node, successor);

			successor -> parent = node -> parent;
			successor -> color = node -> color;
		}

		if(double_black) balance(double_black);
	}

	xe_rb_node* root;
	xe_rb_node* begin_;
	size_t size_;
public:
	typedef xe_rb_iterator_base<xe_node> iterator;
	typedef xe_rb_iterator_base<const xe_node> const_iterator;
	typedef xe_node node;

	xe_rbtree(): root(), begin_(), size_(){}

	xe_disable_copy_move(xe_rbtree)

	iterator insert(xe_node& node){
		insert(&node);

		return iterator(&node);
	}

	iterator erase(xe_node& node){
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
		return iterator(begin_);
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

	iterator find(const xe_node& key){
		xe_rb_node* node = root;

		while(node){
			if(*(xe_node*)node < key)
				node = node -> right;
			else if(*(xe_node*)node > key)
				node = node -> left;
			else
				break;
		}

		return iterator(node);
	}

	~xe_rbtree() = default;
};