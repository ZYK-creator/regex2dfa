#ifndef MBLIT_REGEX_TREE_H
#define MBLIT_REGEX_TREE_H

#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <iomanip>
#include <set>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <deque>
#include "regex_tree_node.h"

/*
Regex Grammar:
	start -> regex EOF
	regex -> expr {'|' expr}
	expr  ->   term {term} 
	term  ->   letter {'*'}
	         | '(' regex ')' {'*'}
*/
struct dfa {
	std::vector<
		std::map<char, int>
	> transitions;
	std::vector<int> accepting_;
	
	int next(int state, char c) const {
		//out of range
		if(state > size())
			throw std::runtime_error("out of raaange");
		//dead state
		if(state == size())
			return size();
		//see if there's a transition
		auto n = transitions[state].find(c);
		//if no transition, return dead state
		if(n == std::end(transitions[state]))
			return size();
		//otherwise return transition
		return n->second;
	}
	
	bool accepting(int state) const {
		if(state > size())
			throw std::runtime_error("OMG");
		if(state == size())
			return false;
		return accepting_[state];
	}
	
	std::size_t size() const {
		return accepting_.size();
	}
	
	std::string graph() const {
		std::set<int> visited = {0};
		std::vector<int> q = {0};
		
		std::stringstream ss;
		ss << "digraph G {\n\tgraph [ordering=\"out\" overlap=scale splines=true];\nrankdir=LR;\n";
		while(!q.empty()) {
			int a = q.back(); q.pop_back();
			if(accepting(a))
				ss << a << " [shape=doublecircle];\n";
			else
				ss << a << " [shape=circle];\n";
			
			for(const auto &p : transitions[a]) {
				if(!visited.count(p.second)) {
					q.push_back(p.second);
					visited.insert(p.second);
				}
				ss << "\t" << a << " -> " << p.second
				   << " [label=\"" << p.first << "\"];\n";
			}
		}
		ss << "}\n";
		return ss.str();
	}

	int &distinct(int s1, int s2, std::vector<std::vector<int>> &table) const
	{
		return table[std::max(s1,s2)][std::min(s1,s2)];
	}
	
	bool update(int s1, int s2, std::vector<std::vector<int>> &table) const {
		bool updated = false;
		if(distinct(s1,s2,table))
			return false;
		std::map<char,int> empty;
		const auto &m1 = ((s1 == size()) ? empty : transitions[s1]);
		const auto &m2 = ((s2 == size()) ? empty : transitions[s2]);		
		for(const auto &m : {m1,m2})
		for(const auto &p : m) {
			char c = p.first;
			int d1 = next(s1, c);
			int d2 = next(s2, c);
			if(distinct(d1,d2, table)) {
				distinct(s1,s2, table) = true;
				updated = true;
			}
		}
		return updated;
	}
		
	dfa minimize() const {
		std::set<std::set<int>> groups;
		{
			std::vector<std::vector<int>> table(
				size()+1, std::vector<int>(size()+1)
			);
			
			for(std::size_t p = 0; p <= size(); p++)
			for(std::size_t q = 0; q < p; q++)
				table[p][q] = accepting(p) != accepting(q); 
			
			bool updated;
			do { 
				updated = false;
				for(std::size_t p = 0; p <= size(); p++)
				for(std::size_t q = 0; q < p;      q++)
					updated |= update(p,q,table);
			} while(updated);
			
			for(std::size_t p = 0; p <= size(); p++) {
				for(std::size_t q = 0; q < p;      q++)	{
					//std::cerr  << distinct(p,q,table);
				}
				//std::cerr << "\n";
			}
			
			std::vector<int> grouped(size()+1);
			for(std::size_t p = 0; p <= size(); p++) {
				if(grouped[p])
					continue;
				std::set<int> group;
				group.insert(p);
				for(std::size_t q = 0; q <= size(); q++) {
					if(p != q && distinct(p,q,table))
						continue;
					group.insert(q);
					grouped[q] = true;
				}
				groups.insert(group);
			}
		}
		const dfa &orig = *this;
		dfa res;
		std::vector<std::set<int>> groups_arr(groups.begin(), groups.end());
		res.transitions.resize(groups_arr.size());
		
		auto find_group_index = [&](int state) -> std::size_t {
		    for(int i = 0; i < groups_arr.size(); i++) {
			if(groups_arr[i].count(state))
			    return i;
		    }
		    throw std::runtime_error("no group found?");
		};
		//ensure that the group containing the start state is the first group
		int start_group = find_group_index(0);
		std::swap(groups_arr[start_group], groups_arr[0]);

		//iterate over the original states
		//inserting transitions between groups
		for(int i = 0; i < orig.transitions.size(); i++) {
		    //iterate over the original edges
		    for(const std::pair<char, int> &p : orig.transitions[i]) {
			//insert the proper transition into the new DFA (group->group)
			res.transitions[find_group_index(i)][p.first] = find_group_index(p.second);
		    }
		}
		
		//determine which groups are accepting
		for(int i = 0; i < groups_arr.size(); i++) {
		    bool is_accepting = false;
		    for(int state : groups_arr[i])
			is_accepting |= accepting(state);
		    res.accepting_.push_back(is_accepting);
		}
		
		//std::cerr << groups.size() << "\n";
		return res;
	}		
};

class regex_tree {
private:
	//node and input types
	enum class symbol {oparen, cparen, star, plus, bar, empty, letter, concat};
	
	//for giving unique ID's to leaf nodes
	int current_id;
	//the input string to build a tree out of
	std::string str;
	//the current parsing position in the input string
	typename std::string::iterator pos;
	//the root of the tree
	std::unique_ptr<node> root;
	
	
public:
	/* DFA construction algorithm from dragon book 2nd ed. figure 3.62 */
	dfa construct_dfa() {
		dfa res;

		auto initial = root.get()->firstpos();
		int current_state = 0;

		//Mapping from sets of positions (states) to numeric ids
		std::map<std::set<node*>, int> state_id = {{initial, current_state++}};
		//set of unmarked states
		std::deque<std::set<node*>> unmarked = {initial};
		
		while(!unmarked.empty()) {
			//The unmarked state that will be populated
			//(and inserted into res)
			std::map<char, int> S;
			bool accepting = false;
			
			//The positions in said state
			auto positions = unmarked.front();
			unmarked.pop_front();
            
			//LHS is `a` (from algo) RHS is `U` (destination state)
			std::map<char, std::set<node*>> u_map;
			for(node *n : positions) {
				letter_node *ln = static_cast<letter_node*>(n);
				if(ln->is_terminator()) {
					accepting = true;
					continue;
				}
				else {
					char letter = ln->letter();
					auto fp = n->followpos();
					u_map[letter].insert(std::begin(fp), std::end(fp));
				}
			}
			for(auto &u_pair : u_map) {
				char a  = u_pair.first;
				auto &U = u_pair.second;
                
				//Add U as new state to Dstates
				if(state_id.count(U) == 0) {
					state_id[U] = current_state++;
					unmarked.push_back(U);
				}
				
				//Add transition from S to U
				S[a] = state_id[U];
			}
			
			//add S to result
			res.transitions.emplace_back(std::move(S));
			res.accepting_.push_back(accepting);
		}		
		return res;
	}
		
	std::string graph() {
		std::stringstream ss;
		ss << "digraph G {\n\tgraph [ordering=\"out\"];\n";
 		std::vector<node*> pending = {root.get()};
		while(!pending.empty()) {
			node *n = pending.back(); pending.pop_back();

			/* ugh yes I know this is terrible */			
			auto firstpos = n->firstpos();
			auto lastpos = n->lastpos();
			auto followpos = n->followpos();
			std::string firstpos_str  = "firstpos: {",
			            lastpos_str   = "lastpos: {",
						followpos_str = "followpos: {";
			for(auto p : firstpos) {
				int id = dynamic_cast<letter_node*>(p)->id();
				firstpos_str += std::to_string(id) + " ";
			}
			firstpos_str += "}";
			for(auto p : lastpos) {
				int id = dynamic_cast<letter_node*>(p)->id();
				lastpos_str += std::to_string(id) + " ";
			}
			lastpos_str += "}";
			for(auto p : followpos) {
				int id = dynamic_cast<letter_node*>(p)->id();
				followpos_str += std::to_string(id) + " ";
			}
			followpos_str += "}";			
			
			ss << long(n) << " [label=<" << n->to_string() << "<BR />\n"
			                 "<FONT POINT-SIZE=\"10\">"
			   << firstpos_str << "<BR />\n" << lastpos_str << "<BR />"
			   << followpos_str << "</FONT>>];\n";
			for(int i = 0; i < n->num_children(); i++) {
				ss << "\t" << long(n) << " -> " << long(n->child(i))  << ";\n";
				pending.push_back(n->child(i));
			}
		}
		ss << "}\n";
		return ss.str();
	}	
					
	//peek at the current symbol without consuming it
	symbol peek() {
		if(pos == std::end(str))
			return symbol::empty;
		switch(*pos) {
			case '(': return symbol::oparen;
			case ')': return symbol::cparen;
			case '*': return symbol::star;
			case '+': return symbol::plus;
			case '|': return symbol::bar;
			default:  return symbol::letter;
		}
	}
	
	//consume and match the current symbol
	//throw an exception if it does not match 's'
	//return the input character that was matched (or -1 for end)
	int match(symbol s) {
		int res = (pos != std::end(str)) ? *pos : -1;
		if(accept(s))
			return res;
		throw std::runtime_error("Invalid Regex");
	}
	//try to consume the current symbol, returning true if it was consumed
	//and false otherwise
	bool accept(symbol s) {
		if(s == peek()) {
			if(s != symbol::empty)
				pos++;
			return true;
		}
		else return false;
	}

	regex_tree(const std::string str_)
	: current_id(0), str(str_), pos(std::begin(str))
	{
		root = start();
		auto end_marker = std::make_unique<terminator_node>(current_id++);
		root = std::make_unique<cat_node>(std::move(root),
										  std::move(end_marker));
		root->build_followpos();
	}
	
	std::unique_ptr<node> start() {
		auto res = regex();
		match(symbol::empty);
		return res;
	}

	std::unique_ptr<node> regex() {		
		//If at EOF or next character would be invalid, then just return
		if(   peek() == symbol::empty)
			return std::make_unique<empty_node>();
		
		auto left = expr();
		while(accept(symbol::bar)) {
			left = std::make_unique<or_node>(std::move(left), expr());
		}
		return left;		
	}

	std::unique_ptr<node> expr() {
		auto left = term();
		std::unique_ptr<node> right;
		while((right = term())) {
			left = std::make_unique<cat_node>(std::move(left),
										      std::move(right));
		}
		return left;
	}
	
	std::unique_ptr<node> term() {
		std::unique_ptr<node> left;
		//match parenthized regular expression
		if(accept(symbol::oparen)) {
			left = regex();
			match(symbol::cparen);
		}
		//or match a letter
		//letters each get a unique ID for DFA construction purposes
		else if(peek() == symbol::letter) {
			char c = match(symbol::letter);
			left = std::make_unique<letter_node>(c,current_id++);
		}
		//or return null
		else
			return left;

		//match unary operators
		while(accept(symbol::star)) {
			left = std::make_unique<star_node>(std::move(left));
		}
		return left;
	}
};


#endif