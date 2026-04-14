// Copyright (c) 2020 Guy Turcotte
//
// MIT License. Look at file licenses.txt for details.

#include "viewers/html_interpreter.hpp"

#include "models/config.hpp"

MemoryPool<Page::Format> HTMLInterpreter::fmtPool;

// This method process a single xml node and recurse for the associated children.
// The method calls the pageEnd() method when it reachs the end of the page as
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
  if (level > maxLevel) maxLevel = level;

  if (level > 50) {
    LOG_E("Too many nested tag levels (max: 50).");
    return true;
  }

  if (node == nullptr) return false;
  if (atEnd()) return true;

  checkIfStarted();

  std::string pictureFilename;
  const char *name;
  const char *str           = nullptr;
  DOM::Node *domCurrentNode = domNode;
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
    if (node.attribute("hidden")) return true;

    // LOG_D("Node name: %s", name);
    // Do it only if we are now in the current page content

    fmt.display      = CSS::Display::INLINE;
    fmt.marginBottom = 0;
    fmt.marginLeft   = 0;
    fmt.marginRight  = 0;
    fmt.marginTop    = 0;

    if ((tagIt = DOM::tags.find(name)) != DOM::tags.end()) {

      // LOG_D("==> %10s [%5d] %5d", name, currentOffset, page->getPosY());

      if (tagIt->second != DOM::Tag::BODY) {
        domCurrentNode = dom->addChild(domNode, tagIt->second);
      } else {
        domCurrentNode = dom->body;
      }
      if ((attr = node.attribute("id"))) domCurrentNode->addId(attr.value());
      if ((attr = node.attribute("class"))) domCurrentNode->addClasses(attr.value());

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
              if ((attr = node.attribute("src")))
                pictureFilename = attr.value();
              else
                currentOffset++;
            } else
              currentOffset++;
          } else {
            if ((attr = node.attribute("alt")))
              str = attr.value();
            else
              currentOffset++;
          }
          break;

        case DOM::Tag::IMAGE:
          if (showPictures) {
            if (started) {
              if ((attr = node.attribute("xlink:href")))
                pictureFilename = attr.value();
              else
                currentOffset++;
            } else
              currentOffset++;
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
            if (!pageEnd(fmt)) return false;
            if (atEnd()) return true;
            page->lineBreak(fmt);
          }
        }
        currentOffset++;
        break;

      case DOM::Tag::B:
      case DOM::Tag::STRONG: {
        Fonts::FaceStyle style = fmt.fontStyle;
        if (style == Fonts::FaceStyle::NORMAL)
          style = Fonts::FaceStyle::BOLD;
        else if (style == Fonts::FaceStyle::ITALIC)
          style = Fonts::FaceStyle::BOLD_ITALIC;
        page->resetFontIndex(fmt, style);
      } break;

      case DOM::Tag::I:
      case DOM::Tag::EM: {
        Fonts::FaceStyle style = fmt.fontStyle;
        if (style == Fonts::FaceStyle::NORMAL)
          style = Fonts::FaceStyle::ITALIC;
        else if (style == Fonts::FaceStyle::BOLD)
          style = Fonts::FaceStyle::BOLD_ITALIC;
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
        elementCss         = CSS::Make("ELEMENT", tagIt->second, buffer, strlen(buffer), 99);
      }

      // Adjust the tag's format styling (the fmt struct) using both the current
      // DOM, the overall item css, and the element css data.
      page->adjustFormat(domCurrentNode, fmt, elementCss,
                         itemInfo.css); // Adjust format from element attributes

      if (started) showState(name, fmt, domCurrentNode /*, elementCss*/); // For debugging
    }

    if (fmt.display == CSS::Display::NONE) return true;
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
          if (!pageEnd(fmt)) return false;
          if (atEnd()) return true;
          page->endParagraph(fmt);
        }
      }

      showState("==> New Paragraph 1 <==", fmt);
      if (!page->newParagraph(fmt)) {
        if (!pageEnd(fmt)) return false;
        if (atEnd()) return true;
        page->newParagraph(fmt);
      }
    }
  } else {
    // We look now at the node content and prepare the glyphs to be put on a page.
    str = fmt.pre ? node.text().get() : node.value();
  }

  if (atEnd()) return true;

  if (showPictures && !pictureFilename.empty()) {
    if (currentOffset < startOffset) {
      // As we move from the beginning of a file, we bypass everything that is there before
      // the start of the page offset
      currentOffset++;
    } else {
      std::string fname = itemInfo.filePath;
      fname.append(pictureFilename);

      if (started && (currentOffset < endOffset)) {

        auto pict = epub->getPicture(fname, page->getComputeMode() == Page::ComputeMode::DISPLAY);
        if (pict != nullptr) {
          bool added            = false;
          std::tie(added, pict) = page->addPicture(std::move(pict), fmt /*, beginning_of_page */);
          if (!added) {
            if (page->isFull() && !pageEnd(fmt)) return false;
            if (atEnd()) return true;

            page->addPicture(std::move(pict), fmt /*, beginning_of_page */);
            if (page->isFull() && !pageEnd(fmt)) return false;
            if (atEnd()) return true;
          }
          showState("After IMG", fmt);
        } else {
          str = "[An picture is not compatible or not found]";
        }
      }
      currentOffset++;
    }
  }

  if (namedElement) { // The element possesses a tag
    // Here we recurse on each child of the currernt tag.
    currentOffset++;
    xml_node sub = node.first_child();
    while (sub != nullptr) {
      if (page->isFull() && !pageEnd(fmt)) return false;
      if (atEnd()) break;
      Page::Format *newFmt = duplicateFmt(fmt);
      if (!buildPagesRecurse(sub, *newFmt, domCurrentNode, level + 1)) {
        releaseFmt(newFmt);
        if (page->isFull() && !pageEnd(fmt)) return false;
        if (atEnd()) break;
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
            if (!pageEnd(fmt)) return false;
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
    } else {
      showState("==> STR <==", fmt);

      bool toBeStarted = !checkIfStarted();

      while (*str) {

        if (uint8_t(*str) <= ' ') {
          if (checkIfStarted()) {
            if (toBeStarted) {
              toBeStarted = false;
              page->newParagraph(fmt, true);
            }
            if ((*str == ' ') || (!fmt.pre && (*str == '\n'))) {
              if ((fmt.trim = !fmt.pre)) { // white spaces will be trimmed at the beginning and the
                                           // end of a line
                // get rid of multiple spaces in a row (if not pre)
                do {
                  str++;
                  currentOffset++;
                } while ((*str == ' ') || (*str == '\n'));
                str--;
                currentOffset--;
              }
              if (!page->addChar(" ", fmt)) {
                // Unable to add the character... the page must be full
                if (!pageEnd(fmt)) return false;
                if (atEnd()) {
                  page->breakParagraph(fmt);
                  return true;
                }
                // We are at the beginning of a new page. We skip white spaces
                showState("==> New Paragraph 2 <==", fmt);
                page->newParagraph(fmt, true);
              }
            } else if (fmt.pre && (*str == '\n')) {
              if (!page->lineBreak(fmt, 30)) {
                if (!pageEnd(fmt)) return false;
                if (atEnd()) return true;
              }
            }
          }
          str++;
          currentOffset++; // Not an UTF-8, so it's ok...
        } else {
          const char *w = str;
          int16_t count = 0;
          while (uint8_t(*str) > ' ') {
            str++;
            count++;
          }
          if (checkIfStarted()) {
            if (toBeStarted) {
              toBeStarted = false;
              page->newParagraph(fmt, true);
            }
            std::string word;
            word.assign(w, count);

            #if EPUB_INKPLATE_BUILD && (LOG_LOCAL_LEVEL == ESP_LOG_VERBOSE)
              static bool first = true;
              if (first) {
                LOG_D("before page->addWord()");
                ESP::show_heaps_info();
              }
            #endif

            if (!page->addWord(word.c_str(), fmt)) {
              if (!pageEnd(fmt)) return false;
              if (atEnd()) {
                page->breakParagraph(fmt);
                return true;
              }
              showState("==> New Paragraph 3 <==", fmt);
              page->newParagraph(fmt, true);
              showState("==> After New Paragraph 3 <==", fmt);
              page->addWord(word.c_str(), fmt);
            }
          }
          currentOffset += count;
        }
        // ToDo: Not sure the following test is required...
        // if (page->isFull()) {
        //   if (!pageEnd(fmt)) return false;
        // }
        // ToDo: This may have to be moved down after the while loop..
        if (atEnd()) {
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
