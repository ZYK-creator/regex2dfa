#ifndef MBLIT_REGEX_TREE_NODE_H
#define MBLIT_REGEX_TREE_NODE_H

#include <set>
#include <vector>
#include <memory>

class node {
public:
	std::set<node*> followpos_;
public:
	virtual bool nullable() const = 0;
	virtual std::set<node*> firstpos() = 0;
	virtual std::set<node*> lastpos() = 0;
	virtual std::string to_string() const { return ""; }
	virtual int num_children() const = 0;
	
	virtual node *child(int) {
		return nullptr;
	}
	
	virtual void build_followpos() {
		for(int i = 0; i < num_children(); i++) {
			child(i)->build_followpos();
		}
	}
	std::set<node*> followpos() {
		return followpos_;
	}
};

class empty_node : public node {
public:
	empty_node() {}
	
	bool nullable() const {
		return true;
	}
	std::set<node*> firstpos() {
		return {};
	}
	std::set<node*> lastpos() {
		return {};
	}
	std::string to_string() const { return "empty"; }
	
	int num_children() const { return 0; }
};

class letter_node : public node {
	char letter_;
	int id_;
public:
	letter_node(char letter, int id) : letter_(letter), id_(id) {}
	
	bool nullable() const { 
		return false;
	}
	std::set<node*> firstpos() {
		return {this};
	}
	std::set<node*> lastpos() {
		return {this};
	}
	std::string to_string() const {
		return std::string() + letter_ + ":"	 + std::to_string(id_);
	}
	
	int num_children() const { return 0; }
	
	char letter() const { return letter_; }
	int id() const { return id_; }
	
	virtual bool is_terminator() const { return false; }
};

class terminator_node : public letter_node {
public:
	terminator_node(int id) : letter_node('#', id) {}
	bool is_terminator() const { return true; }
};

class or_node : public node {
	std::unique_ptr<node> child_[2];
	bool nullable_;
	std::set<node*> firstpos_;
	std::set<node*> lastpos_;
public:
	or_node(std::unique_ptr<node> &&lhs,
			std::unique_ptr<node> &&rhs)
	: child_{std::move(lhs), std::move(rhs)},
		nullable_(child_[0]->nullable() || child_[1]->nullable())
	{
		const auto &f1 = child_[0]->firstpos();
		const auto &f2 = child_[1]->firstpos();
		const auto &l1 = child_[0]->lastpos();
		const auto &l2 = child_[1]->lastpos();
		firstpos_.insert(std::begin(f1), std::end(f1));
		firstpos_.insert(std::begin(f2), std::end(f2));
		lastpos_.insert( std::begin(l1), std::end(l1));
		lastpos_.insert( std::begin(l2), std::end(l2));
	}
	
	bool nullable() const { 
		return nullable_;
	}
	std::set<node*> firstpos() {
		return firstpos_;
	}
	std::set<node*> lastpos() {
		return lastpos_;
	}
	std::string to_string() const { return "OR"; }
	
	int num_children() const { return 2; }
	
	node *child(int index) {
		if(index < 0 || index >= 3)
			return nullptr;
		return child_[index].get();
	}
};

class cat_node : public node {
	std::unique_ptr<node> child_[2];
	bool nullable_;
	std::set<node*> firstpos_;
	std::set<node*> lastpos_;	
public:
	cat_node(std::unique_ptr<node> &&lhs,
			std::unique_ptr<node> &&rhs)
	: child_{std::move(lhs), std::move(rhs)},
		nullable_(child_[0]->nullable() && child_[1]->nullable())
	{
		const auto &f1 = child_[0]->firstpos();
		const auto &f2 = child_[1]->firstpos();
		const auto &l1 = child_[0]->lastpos();
		const auto &l2 = child_[1]->lastpos();
		if(child_[0]->nullable()) {
			firstpos_.insert(std::begin(f1), std::end(f1));
			firstpos_.insert(std::begin(f2), std::end(f2));
		} else {
			firstpos_ = f1;
		}
		if(child_[1]->nullable()) {
			lastpos_.insert(std::begin(l1), std::end(l1));
			lastpos_.insert(std::begin(l2), std::end(l2));
		} else {
			lastpos_ = l2;
		}
	}
	
	bool nullable() const { 
		return nullable_;
	}
	std::set<node*> firstpos() {
		return firstpos_;
	}
	std::set<node*> lastpos() {
		return lastpos_;
	}	
	std::string to_string() const { return "CAT"; }
	
	int num_children() const { return 2; }
	
	node *child(int index) {
		if(index < 0 || index >= 3)
			return nullptr;
		return child_[index].get();
	}
	
	void build_followpos() {
		auto last_set  = child_[0]->lastpos();
		const auto first_set = child_[1]->firstpos();
		for(node *n : last_set)
			n->followpos_.insert(std::begin(first_set), std::end(first_set));
		node::build_followpos();
	}
};

class star_node : public node {
	std::unique_ptr<node> child_;
public:
	star_node(std::unique_ptr<node> &&child_)
	: child_(std::move(child_))
	{}
		
	bool nullable() const{
		return true;
	}
	std::set<node*> firstpos() {
		return child_->firstpos();
	};
	std::set<node*> lastpos() { 
		return child_->lastpos();
	}
	std::string to_string() const { return "STAR"; }
	
	int num_children() const { return 1; }
	
	node *child(int index) {
		if(index == 0)
			return child_.get();
		return nullptr;
	}
	
	void build_followpos() {
		auto fp = firstpos();
		auto lp = lastpos();
		for(node *n : lp) {
			n->followpos_.insert(std::begin(fp), std::end(fp));
		}
		node::build_followpos();
	}
};

#endif