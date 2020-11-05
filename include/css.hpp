#ifndef __CSS_HPP__
#define __CSS_HPP__

#include "global.hpp"

#include <string>
#include <unordered_map>
#include <list>
#include <forward_list>

#if 0
    This is the list of all css properties. The ones
    in use are marked with a star.
    
    background
    background-attachment
    background-color
    background-image
    background-position
    background-repeat
    border
    border-bottom
    border-bottom-color
    border-bottom-style
    border-bottom-width
    border-collapse
    border-color
    border-left
    border-left-color
    border-left-style
    border-left-width
    border-right
    border-right-color
    border-right-style
    border-right-width
    border-spacing
    border-style
    border-top
    border-top-color
    border-top-style
    border-top-width
    border-width
    bottom
    caption-side
    clear
    clip
    color
    content
    counter-increment
    counter-reset
    cursor
    direction
    display
    empty-cells
    float
    font
 *  font-family
 *  font-size
 *  font-style
    font-variant
 *  font-weight
 *  height
    left
    letter-spacing
 *  line-height
    list-style
    list-style-image
    list-style-position
    list-style-type
 *  margin
 *  margin-bottom
 *  margin-left
 *  margin-right
 *  margin-top
    max-height
    max-width
    min-height
    min-width
    outline
    outline-color
    outline-style
    outline-width
    overflow
    padding
    padding-bottom
    padding-left
    padding-right
    padding-top
    page-break-after
    page-break-before
    page-break-inside
    position
    quotes
    right
 *  src
    table-layout
 *  text-align
    text-decoration
 *  text-indent
 *  text-transform
    top
    vertical-align
    visibility
    white-space
 *  width
    word-spacing
    z-index
#endif

class CSS
{
  public:
    CSS(std::string folder_path, const char * buffer, int32_t size, const std::string & css_id);
   ~CSS();

    enum     ValueType : uint8_t { NOTHING, PT, PX, EM, CM, PERCENT, STR, URL, NOTYPE };
    enum         Align : uint8_t { LEFT_ALIGN = 0, CENTER_ALIGN, RIGHT_ALIGN, JUSTIFY };
    enum TextTransform : uint8_t { NO_TRANSFORM, UPPERCASE, LOWERCASE, CAPITALIZE };
    enum    PropertyId : uint8_t { NOT_USED,   FONT_FAMILY, FONT_SIZE,      FONT_STYLE,  FONT_WEIGHT,
                                   TEXT_ALIGN, TEXT_INDENT, TEXT_TRANSFORM, LINE_HEIGHT, SRC,
                                   MARGIN,     MARGIN_LEFT, MARGIN_RIGHT,   MARGIN_TOP,  MARGIN_BOTTOM,
                                   WIDTH,      HEIGHT
                                  };

    struct Value {
      float num;
      int32_t choice;
      std::string str;
      ValueType value_type;
    };

    static int16_t font_normal_size;

    typedef std::string                Selector;
    typedef std::forward_list<Value *> Values;
    typedef std::list<Selector>        Selectors;
    
    struct Property {
      PropertyId id;
      Values     values;
    };
    typedef std::forward_list<Property *> Properties;
    // typedef std::vector<Properties> PropertySuites;
    typedef std::list<Properties *> PropertySuite;
    typedef std::forward_list<Properties *> PropertySuiteList;
    typedef std::unordered_map<std::string, PropertyId> PropertyMap;
    typedef std::unordered_map<std::string, int16_t> FontSizeMap;

    static PropertyMap property_map;
    static FontSizeMap font_size_map;

  private:
    bool ghost; // Property suites come from other instances. So don't delete them...
    typedef std::unordered_map<Selector, PropertySuite> RulesMap;
    RulesMap rules_map;
    PropertySuiteList suites; // Linear line of suites to be deleted when the instance will be destroyed.

    std::string id;
    std::string file_folder_path; 
    const char * buffer_start;

    const char * parse_selector(const char * str, std::string & selector);
    static const char * parse_property_name(const char * str, std::string & name);
    static const char * parse_value(const char * str, Value * v, const char * buffer_start);

  public:
    const std::string * get_id() const { return &id; };
    const std::string & get_folder_path() const { return file_folder_path; }

    void clear();
    void show_properties(const Properties & props) const;
    void show() const;

    const PropertySuite * get_property_suite(const std::string & selector) const {
      // std::cout << "Searching: " << selector << "... ";
      CSS::RulesMap::const_iterator suite = rules_map.find(selector);
      if (suite != rules_map.end()) {
        // std::cout << "FOUND!" << std::endl;
        return &suite->second;
      }
      // std::cout << "No" << std::endl;
      return nullptr;
    };

    static const Values * get_values_from_suite(const PropertySuite & props, 
                                                PropertyId id) {
      Values * vals = nullptr;
      for (auto & bundle : props) {
        for (auto & prop : *bundle) {
          if (id == prop->id) vals = &prop->values;
        }
      }
      return vals;
    };

    /**
     * @brief Get the values from a property object
     * 
     * Search in *props* for the property name *id* and return the associated values.
     * 
     * @param props 
     * @param id 
     * @return const Values* 
     */
    const Values * get_values_from_props(const Properties & props, PropertyId id) const {
      const Values * vals = nullptr;
      for (auto & prop : props) {
        if (id == prop->id) vals = &prop->values;
      }
      return vals;
    };

    /**
     * @brief Search selection patterns in rules
     * 
     * The following patterns are supported: element_name.class_name, 
     * element_name, class_name and '*' 
     * 
     * @param element_name 
     * @param class_name 
     * @return const PropertySuite* 
     */
    const PropertySuite * search(const std::string & element_name, 
                                 const std::string & class_name) const {
      const PropertySuite * suite;
      if (class_name.size() > 0) {
        suite = get_property_suite(element_name + "." + class_name);
        if (suite == nullptr) {
          suite = get_property_suite(element_name);
          if (suite == nullptr) {
            suite = get_property_suite("." + class_name);
            // DEBUG2("Searching:", sel, ((suite == nullptr) ? "NO" : "YES"));
          }
        }
      }
      else {
        suite = get_property_suite(element_name);
      }
      if (suite == nullptr) suite = get_property_suite("*");
      return suite;
    }

    /**
     * @brief Add properties to a selector properties suite
     * 
     * This method is used to build a list of properties list (a property 
     * suite) associated with a selector. It is used externally to this 
     * class by the EPub class to generate a merged CSS class instance 
     * in relation to the current html file being processed.
     * 
     * @param sel 
     * @param properties 
     */
    void inline add_properties_to_selector(const Selector & sel, 
                                           Properties * properties) {
      rules_map[sel].push_back(properties);
    }

    /**
     * @brief Retrieve (merge) property suites from other CSS
     * 
     * To construct a single CSS object for the current html file 
     * being processed, this method retrieve the properties from 
     * object *css* and add them to the current CSS occurence.
     * 
     * @param css 
     */
    void retrieve_data_from_css(CSS & css) {
      for (auto & sel : css.rules_map) {
        for (auto & properties : sel.second) {
          add_properties_to_selector(sel.first, properties);
        }
      }
    }

    /**
     * @brief Parse a property list string to a CSS::Properties structure
     * 
     * Used to parse part of a CSS file and style attributes 
     * associated to an element in a html file.
     * 
     * @param buffer 
     * @param end 
     * @param buffer_start 
     * @return CSS::Properties* 
     */
    static CSS::Properties * parse_properties(const char ** buffer, 
                                              const char * end, 
                                              const char * buffer_start);
};

#endif