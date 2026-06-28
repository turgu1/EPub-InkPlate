// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "viewers/html_interpreter.hpp"

#include "config.hpp"
#include "fonts.hpp"
#include "helpers/show_load_icon.hpp"
#include "screen.hpp"

#include "controllers/event_mgr.hpp"

// MemoryPool<Page::Format> HTMLInterpreter::fmtPool;

// This method process a single xml node and recurse for the associated children.
// The method calls the pageEndProcessing() method when it reachs the end of the page as
// defined by the endOffset variable, or when the page class indicated that the page
// is full (through the page_full() method or false value returned by some of its methods).
//
// This method is used both to compute the pages location, size, and offsets, but also
// to display a single page on screen.
//
// Before this method can be called, the setLimits() method must be called to set
// proper control variables that will impact the method results.
//
// For pages location computation the limits must be 0 (beginning of the html file) and
// 999999 (infinite end of the html file).
//
// For single page output on screen, the limits must be set to the offset of the first
// location in file to show on screen, and to the last offset for that page.
//
// The offsets are not character indexes in the html file, but internal computation of the location,
// depending on the kind of tag encountered and the strings of characters present in each
// block to be displayed (paragraphs, headers, etc.)

auto HTMLInterpreter::buildPagesRecurse(xml_node node, Page::Format &fmt, DOM::Node *domNode,
                                        int16_t level) -> bool {
  if (level > maxLevel) { maxLevel = level; }

  if (level > 50) {
    LOG_E("Too many nested tag levels (max: 50).");
    return true;
  }

  if (node == nullptr) { return false; }
  if (atEndOfPageOffset()) { return true; }

  (void)checkIfStarted();

  std::string         pictureFilename;
  const char *        name;
  const char *        str           = nullptr;
  DOM::Node *         domCurrentNode = domNode;
  DOM::Tags::iterator tagIt = DOM::tags.end();

  // xml node without a tag name are internal data to be processed as string of chars
  bool namedElement = *(name = node.name()) != 0;

  if (namedElement) {

    xml_attribute attr;

    if ((page->getComputeMode() == Page::ComputeMode::LOCATION) && epub->toc->thereIsSomeIds() &&
        (attr = node.attribute("id"))) {
      std::string id = attr.value();
      epub->toc->set(id, currentOffset);
    }

    if (node.attribute("hidden")) { return true; }

    // Do it only if we are now in the current page content

    fmt.display      = CSS::Display::INLINE;
    fmt.marginBottom = 0;
    fmt.marginLeft   = 0;
    fmt.marginRight  = 0;
    fmt.marginTop    = 0;

    if ((tagIt = DOM::tags.find(name)) != DOM::tags.end()) {

      if (tagIt->second != DOM::Tag::BODY) {
        domCurrentNode = dom->addChild(domNode, tagIt->second);
        if (domCurrentNode == nullptr) {
          // OOM while creating a DOM node: keep parent context to continue parsing.
          domCurrentNode = domNode;
        }
      } else {
        domCurrentNode = dom->body;
      }
      if (domCurrentNode != nullptr) {
        if ((attr = node.attribute("id"))) { domCurrentNode->addId(attr.value()); }
        if ((attr = node.attribute("class"))) { domCurrentNode->addClasses(attr.value()); }
      }

      switch (tagIt->second) {
      case DOM::Tag::A:
      case DOM::Tag::BODY:
      case DOM::Tag::SPAN:
        break;

        #if NO_IMAGE
          case IMG:
          case IMAGE:
            break;

        #else
          case DOM::Tag::IMG:
            if (showPictures) {
              if (started) {
                if ((attr = node.attribute("src"))) {
                  pictureFilename = attr.value();
                }
                else {
                  ++currentOffset;
                }
              } else {
                ++currentOffset;
              }
            } else {
              if ((attr = node.attribute("alt"))) {
                str = attr.value();
              }
              else {
                ++currentOffset;
              }
            }
            break;

          case DOM::Tag::IMAGE:
            if (showPictures) {
              if (started) {
                if ((attr = node.attribute("xlink:href"))) {
                  pictureFilename = attr.value();
                }
                else {
                  ++currentOffset;
                }
              } else {
                ++currentOffset;
              }
            }
            break;

            #endif
          case DOM::Tag::PRE:
            fmt.pre     = true;
            fmt.display = CSS::Display::BLOCK;
            break;

          case DOM::Tag::BLOCKQUOTE:
            fmt.display = CSS::Display::BLOCK;
            break;

          case DOM::Tag::LI:
          case DOM::Tag::DIV:
          case DOM::Tag::P:
            fmt.display = CSS::Display::BLOCK;
            break;

          case DOM::Tag::BREAK:
            if (started) {
              showState("Line Break", fmt);
              if (!page->lineBreak(fmt)) {
                // At the end of the page
                if (!pageEndProcessing(fmt)) { return false; }
                if (atEndOfPageOffset()) { return true; }
                page->lineBreak(fmt);
              }
            }
            ++currentOffset;
            break;

          case DOM::Tag::B:
          case DOM::Tag::STRONG: {
            FaceStyle style = fmt.fontStyle;
            if (style == FaceStyle::NORMAL) {
              style = FaceStyle::BOLD;
            }
            else if (style == FaceStyle::ITALIC) {
              style = FaceStyle::BOLD_ITALIC;
            }
            page->resetFontIndex(fmt, style);
          } break;

          case DOM::Tag::I:
          case DOM::Tag::EM: {
            FaceStyle style = fmt.fontStyle;
            if (style == FaceStyle::NORMAL) {
              style = FaceStyle::ITALIC;
            }
            else if (style == FaceStyle::BOLD) {
              style = FaceStyle::BOLD_ITALIC;
            }
            page->resetFontIndex(fmt, style);
          } break;

          case DOM::Tag::H1:
            fmt.fontSize = 1.25 * fmt.fontSize;
            // fmt.lineHeightFactor = 1.25 * fmt.lineHeightFactor;
            fmt.display = CSS::Display::BLOCK;
            break;

          case DOM::Tag::H2:
            fmt.fontSize = 1.1 * fmt.fontSize;
            // fmt.lineHeightFactor = 1.1 * fmt.lineHeightFactor;
            fmt.display = CSS::Display::BLOCK;
            break;

          case DOM::Tag::H3:
            fmt.fontSize = 1.05 * fmt.fontSize;
            // fmt.lineHeightFactor = 1.05 * fmt.lineHeightFactor;
            fmt.display = CSS::Display::BLOCK;
            break;

          case DOM::Tag::H4:
            fmt.display = CSS::Display::BLOCK;
            break;

          case DOM::Tag::H5:
            fmt.fontSize = 0.8 * fmt.fontSize;
            // fmt.lineHeightFactor = 0.8 * fmt.lineHeightFactor;
            fmt.display = CSS::Display::BLOCK;
            break;

          case DOM::Tag::H6:
            fmt.fontSize = 0.7 * fmt.fontSize;
            // fmt.lineHeightFactor = 0.7 * fmt.lineHeightFactor;
            fmt.display = CSS::Display::BLOCK;
            break;

          case DOM::Tag::SUB:
            fmt.fontSize      = 0.75 * fmt.fontSize;
            fmt.verticalAlign = 5;
            break;

          case DOM::Tag::SUP:
            fmt.fontSize      = 0.75 * fmt.fontSize;
            fmt.verticalAlign = -5;
            break;

          case DOM::Tag::TABLE:
          case DOM::Tag::COLGROUP:
          case DOM::Tag::TR:
          case DOM::Tag::TD:
          case DOM::Tag::TH:
          case DOM::Tag::TFOOT:
          case DOM::Tag::THEAD:
          case DOM::Tag::COL:
          case DOM::Tag::CAPTION:
          case DOM::Tag::TBODY:
            // For now, we do not support tables, so we just ignore the content of the
            // table tags and do not display them. We set the display to NONE, but we still process the
            // content of the table tags to compute the page offsets properly.
            fmt.display = CSS::Display::NONE;
            break;

          case DOM::Tag::NONE:
          case DOM::Tag::ANY:
          case DOM::Tag::FONT_FACE:
          case DOM::Tag::PAGE:
            return true;
      }

      // if a 'style' attribute is present, parse it's content as it will be used
      // in the processing of the tag's format styling

      CSSPtr elementCss = nullptr;
      if ((attr = node.attribute("style"))) {
        const char *buffer = attr.value();
        elementCss =
          CSS::Make("ELEMENT", tagIt->second, buffer, strlen(buffer), 99, epub->getCssPools());
      }

      // Adjust the tag's format styling (the fmt struct) using both the current
      // DOM, the overall item css, and the element css data.
      page->adjustFormat(domCurrentNode, fmt, elementCss,
                         itemInfo.css); // Adjust format from element attributes

      if (started) { showState(name, fmt, domCurrentNode /*, elementCss*/); } // For debugging
    }

    if (fmt.display == CSS::Display::NONE) { return true; }
    if (tagIt->second == DOM::Tag::BODY) {
      if (epub->getBookFormatParams()->useFontsInBook == 0) {
        fmt.fontSize = epub->getBookFormatParams()->fontSize;
        // fmt.fontIndex = ;
      }
    }

    if (started && (fmt.display == CSS::Display::BLOCK)) {

      int8_t iter = 5; // Allow for a paragraph taking at most 5 pages
      while ((iter-- > 0) && page->someDataWaiting()) {
        showState("==> End Paragraph 1 <==", fmt);
        if (!page->endParagraph(fmt)) {
          if (!pageEndProcessing(fmt)) { return false; }
          if (atEndOfPageOffset()) { return true; }
          page->endParagraph(fmt);
        }
      }

      showState("==> New Paragraph 1 <==", fmt);
      if (!page->newParagraph(fmt)) {
        if (!pageEndProcessing(fmt)) { return false; }
        if (atEndOfPageOffset()) { return true; }
        page->newParagraph(fmt);
      }
    }
  } else {
    // We look now at the node content and prepare the glyphs to be put on a page.
    str = fmt.pre ? node.text().get() : node.value();
  }

  if (atEndOfPageOffset()) { return true; }

  if (!pictureFilename.empty()) {
    if (showPictures) {
      if (currentOffset < startOffset) {
        // As we move from the beginning of a file, we bypass everything that is there before
        // the start of the page offset
        ++currentOffset;
      } else {
        HimemString fname = itemInfo.filePath;
        fname.append(pictureFilename);

        // LOG_I("Processing image ({}): {} at offset {}",
        //       page->getComputeMode() == Page::ComputeMode::DISPLAY ? "DISPLAY" : "OTHER",
        //       pictureFilename, currentOffset);

        if (started && (currentOffset < endOffset)) {
          const bool displayMode = page->getComputeMode() == Page::ComputeMode::DISPLAY;

          auto       pict = epub->getPicture(fname, displayMode);

          // If the image is large, show a loading icon while it loads to avoid a
          // long wait with a blank page.
          if (displayMode) {
            if ((pict != nullptr) && eventMgr.someEventWaiting()) {
              showLoadIcon(pict->getDim());
            }
          }

          if (pict != nullptr) {
            bool added            = false;
            std::tie(added, pict) = page->addPicture(std::move(pict), fmt /*, beginning_of_page */);
            if (!added) {
              if (page->isFull() && !pageEndProcessing(fmt)) { return false; }
              if (atEndOfPageOffset()) { return true; }

              page->addPicture(std::move(pict), fmt /*, beginning_of_page */);
              if (page->isFull() && !pageEndProcessing(fmt)) { return false; }
              if (atEndOfPageOffset()) { return true; }
            }
            showState("After IMG", fmt);
          } else {
            str              = "(An image is not compatible or not found)";
            offsetAdjustment = 41;
          }
        }
        ++currentOffset;
      }
    }
  }

  if (namedElement) { // The element possesses a tag
    // Here we recurse on each child of the currernt tag.
    ++currentOffset;
    xml_node sub = node.first_child();
    while (sub != nullptr) {
      if (page->isFull() && !pageEndProcessing(fmt)) { return false; }
      if (atEndOfPageOffset()) { break; }
      Page::Format *newFmt = duplicateFmt(fmt);
      if (!buildPagesRecurse(sub, *newFmt, domCurrentNode, level + 1)) {
        releaseFmt(newFmt);
        if (page->isFull() && !pageEndProcessing(fmt)) { return false; }
        if (atEndOfPageOffset()) { break; }
        return false;
      }
      releaseFmt(newFmt);
      sub = sub.next_sibling();
    }

    // The sub-nodes have been processed. Complete the block if not Inline and check
    // for remaining data if it's the head of the hierarchy ('body' tag)
    if (checkIfStarted()) {
      if (fmt.display == CSS::Display::BLOCK) {
        if ((currentOffset != startOffset) || page->someDataWaiting()) {
          showState("==> End Paragraph 2 <==", fmt);
          page->endParagraph(fmt);
          showState("==> After End Paragraph 2 <==", fmt);
        }
      }

      // In case that we are at the end of an html file and there remains
      // characters in the page pipeline, to get them out on the page...
      if ((tagIt != DOM::tags.end()) && (tagIt->second == DOM::Tag::BODY)) {
        int8_t iter = 5; // limit of 5 pages for a single paragraph...
        // Loop until the complete paragraph has been processed
        while ((iter-- > 0) && page->someDataWaiting()) {
          showState("==> End Paragraph 3 <==", fmt);
          if (!page->endParagraph(fmt)) {
            // The paragraph may not have been completly rendered. This means
            // that we found the end of a page
            if (!pageEndProcessing(fmt)) { return false; }
            page->endParagraph(fmt);
          }
        }
      }
    }
  }

  if (str != nullptr) {
    int16_t size;

    if (currentOffset + (size = strlen(str)) <= startOffset) {
      // As we move from the beginning of a file, we bypass everything that is there before
      // the start of the page offset
      currentOffset += size;
      currentOffset -= offsetAdjustment;
      offsetAdjustment = 0;
    } else {
      showState("==> STR <==", fmt);

      currentOffset -= offsetAdjustment;
      offsetAdjustment = 0;

      bool toBeStarted = !checkIfStarted();

      while (*str) {

        if (uint8_t(*str) <= ' ') {
          if (checkIfStarted()) {
            if (toBeStarted) {
              toBeStarted = false;
              page->newParagraph(fmt, true);
            }
            if ((*str == ' ') || (!fmt.pre && (*str == '\n'))) {
              if ((fmt.trim = !fmt.pre)) { // white spaces will be trimmed at the beginning and
                                           // the end of a line
                // get rid of multiple spaces in a row (if not pre)
                do {
                  ++str;
                  ++currentOffset;
                } while ((*str == ' ') || (*str == '\n'));
                --str;
                --currentOffset;
              }
              if (!page->addChar(" ", fmt)) {
                // Unable to add the character... the page must be full
                if (!pageEndProcessing(fmt)) { return false; }
                if (atEndOfPageOffset()) {
                  page->breakParagraph(fmt);
                  return true;
                }
                // We are at the beginning of a new page. We skip white spaces
                showState("==> New Paragraph 2 <==", fmt);
                page->newParagraph(fmt, true);
              }
            } else if (fmt.pre && (*str == '\n')) {
              if (!page->lineBreak(fmt, 30)) {
                if (!pageEndProcessing(fmt)) { return false; }
                if (atEndOfPageOffset()) { return true; }
              }
            }
          }
          ++str;
          ++currentOffset; // Not an UTF-8, so it's ok...
        } else {
          const char *w = str;
          int16_t     count = 0;
          while (uint8_t(*str) > ' ') {
            ++str;
            ++count;
          }

          if (!started && ((currentOffset + count) > startOffset)) {
            w += (startOffset - currentOffset);
            count -= (startOffset - currentOffset);
            currentOffset = startOffset;
          }

          if (checkIfStarted()) {
            if (toBeStarted) {
              toBeStarted = false;
              page->newParagraph(fmt, true);
            }

            std::string word(w, count);

            #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
              static bool first = true;
              if (first) {
                LOG_D("before page->addWord()");
                ESP::show_heaps_info();
              }
            #endif

            // addWord() returns nullptr if the whole word was
            // added to the page, or a pointer inside the word if
            // a part of it or none of it was entered.
            if ((w = page->addWord(word.c_str(), fmt)) != nullptr) {
              currentOffset += (w - word.c_str());
              if (!pageEndProcessing(fmt)) { return false; }
              if (atEndOfPageOffset()) {
                page->breakParagraph(fmt);
                return true;
              }
              showState("==> New Paragraph 3 <==", fmt);
              page->newParagraph(fmt, true);
              showState("==> After New Paragraph 3 <==", fmt);
              if (w != word.c_str()) { word = word.erase(0, w - word.c_str()); }
              w = page->addWord(word.c_str(), fmt);
              currentOffset += (w == nullptr) ? word.size() : w - word.c_str();
            } else {
              currentOffset += count;
            }
          } else {
            currentOffset += count;
          }
        }

        if (atEndOfPageOffset()) {
          if (*str) {
            showState("==> Before Break Paragraph 1 <==", fmt);
            page->breakParagraph(fmt);
            showState("==> After Break Paragraph 1 <==", fmt);
          }
          break;
        }
      }
    }
  }

  return true;
}
