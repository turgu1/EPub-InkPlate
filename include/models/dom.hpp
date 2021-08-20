#pragma once

#include <iostream>
#include <fstream>
#include <forward_list>
#include <map>
#include <string>
#include <sstream>
#include <iterator>

#include "memory_pool.hpp"

class DOM 
{
  public:
    // HTML tags supported by the application
    //
    // These are the tags required by the application. They will be used to limit the retrieval of
    // rules to the one that uses only these tags.
    //
    // The pseudo tags NONE and ANY are for selector definition, when no tag is identified 
    // in the selector or the '*' selector is used. The pseudo tag FONT_FACE is to grab
    // @font-face definitions in the rules-set. Same for @page vs PAGE.
    enum class Tag : uint8_t { NONE, BODY, P, LI, BREAK, H1, H2, H3, H4, H5, H6, 
                               B, I, A, IMG, IMAGE, EM, DIV, SPAN, PRE,
                               BLOCKQUOTE, STRONG, ANY, FONT_FACE, PAGE,
                               SUB, SUP };

    typedef std::map<std::string, Tag> Tags;

    static Tags tags;

    struct Node;

    typedef std::forward_list<std::string> ClassList;
    typedef std::forward_list<Node *>      NodeList;

    struct Node {
      Node *      father;
      Node *      predecessor;
      NodeList    children;
      ClassList   class_list;
      std::string id;
      Tag         tag;
      bool        first_child;

      Node(Node * the_father, Tag the_tag) {
        father = the_father;
        tag    = the_tag;
        if (father != nullptr) {
          first_child = father->children.empty();
          predecessor = first_child ? nullptr : father->children.front();
          father->children.push_front(this);    
        }
        else first_child = true;
      };

      ~Node() {
        for (auto * node : children) node_pool.deleteElement(node);
        children.clear();
      }

      Node * add_child(Tag the_tag) {
        return node_pool.newElement(this, the_tag);
      }

      Node * add_class(std::string the_class) {
        class_list.push_front(the_class);
        return this;
      }

      Node * add_classes(std::string the_classes) {
        std::istringstream iss(the_classes);
        std::string item;
        while (std::getline(iss, item, ' ')) {
          if (!item.empty()) class_list.push_front(item);
        }
        return this;
      }

      Node * add_id(std::string the_id) {
        id = the_id;
        return this;
      }

      void show_children(NodeList::const_iterator node_it, int8_t lev) const {
        #if DEBUGGING
          if (node_it != children.end()) {
            NodeList::const_iterator next_node_it = node_it;
            show_children(++next_node_it, lev);
            (*node_it)->show(lev);
          }
        #endif
      }

      void show(uint8_t level) const {
        #if DEBUGGING
          std::cout << std::string(level * 2, ' ');
          for (auto & t : tags) {
            if (t.second == tag) { std::cout << t.first; break; }
          }
          std::cout << " ";
          if (!id.empty()) std::cout << "#" << id;
          for (auto & c : class_list) std::cout << '.' << c;
          if (first_child) std::cout << ":first_child";
          std::cout << std::endl;

          show_children(children.cbegin(), level + 1); 
        #endif
      }
    };

    Node * body;

    DOM() {
      body = node_pool.newElement(nullptr, Tag::BODY);
    }

    ~DOM() {
      node_pool.deleteElement(body);
    }

    void show() {
      #if DEBUGGING
        std::cout << "DOM:" << std::endl;
        body->show(1);
        std::cout << "[END DOM]" << std::endl;
      #endif
    }

  private:
    static MemoryPool<Node> node_pool;
};
