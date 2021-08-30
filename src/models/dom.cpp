// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "models/dom.hpp"

MemoryPool<DOM::Node> DOM::node_pool;

DOM::Tags DOM::tags
  = {{"p",           Tag::P}, {"div",               Tag::DIV}, {"span", Tag::SPAN}, {"br",   Tag::BREAK}, {"h1",                 Tag::H1},  
     {"h2",         Tag::H2}, {"h3",                 Tag::H3}, {"h4",     Tag::H4}, {"h5",      Tag::H5}, {"h6",                 Tag::H6}, 
     {"b",           Tag::B}, {"i",                   Tag::I}, {"em",     Tag::EM}, {"body",  Tag::BODY}, {"a",                   Tag::A},
     {"img",       Tag::IMG}, {"image",           Tag::IMAGE}, {"li",     Tag::LI}, {"pre",    Tag::PRE}, {"blockquote", Tag::BLOCKQUOTE},
     {"strong", Tag::STRONG}, {"sub",               Tag::SUB}, {"sup",   Tag::SUP}, {"none",  Tag::NONE}, {"*",                 Tag::ANY}, 
     {"@page",    Tag::PAGE}, {"@font-face",  Tag::FONT_FACE},
    };
