# components/simple_list — header-only singly-linked list template
#
# SimpleList<T> is a pure C++ template (no .cpp translation unit needed).
# We register it as an IDF component so that other components can list
# "simple_list" in their REQUIRES and get the include path automatically.

set(components "")   # no runtime dependency on other components

idf_component_register(
    INCLUDE_DIRS "src"
    REQUIRES     ${components}
)

project(simple_list)
