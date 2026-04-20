#if 1 // DEBUGGING_TOOL

  #include "helpers/debug_tool.hpp"

  #include "controllers/app_controller.hpp"
  #include "controllers/back_lit.hpp"
  #include "controllers/book_controller.hpp"
  #include "controllers/book_param_controller.hpp"
  #include "controllers/books_dir_controller.hpp"
  #include "controllers/clock.hpp"
  #include "controllers/common_actions.hpp"
  #include "controllers/event_mgr.hpp"
  #include "controllers/ntp.hpp"
  #include "controllers/option_controller.hpp"
  #include "controllers/toc_controller.hpp"
  #include "controllers/wifi.hpp"

  #include "helpers/duration.hpp"

  #include "models/books_dir.hpp"
  #include "models/css.hpp"
  #include "models/css_parser.hpp"
  #include "display_list.hpp"
  #include "models/dom.hpp"
  #include "models/epub.hpp"
  #include "models/font.hpp"
  #include "models/font_factory.hpp"
  #include "models/fonts.hpp"
  #include "models/ibmf.hpp"
  #include "models/ibmf_font.hpp"
  #include "models/nvs_mgr.hpp"
  #include "models/page_locs.hpp"
  #include "models/toc.hpp"
  #include "models/ttf2.hpp"

  #include "viewers/book_viewer.hpp"
  #include "viewers/books_dir_viewer.hpp"
  #include "viewers/form_viewer.hpp"
  #include "viewers/html_interpreter.hpp"
  #include "viewers/keyboard_viewer.hpp"
  #include "viewers/keypad_viewer.hpp"
  #include "viewers/linear_books_dir_viewer.hpp"
  #include "viewers/matrix_books_dir_viewer.hpp"
  #include "viewers/menu_viewer.hpp"
  #include "viewers/msg_viewer.hpp"
  #include "viewers/page.hpp"
  #include "viewers/screen_bottom.hpp"
  #include "viewers/toc_viewer.hpp"

  #include "screen.hpp"

  #include "bitmap_picture.hpp"
  #include "jpeg_picture.hpp"
  #include "picture.hpp"
  #include "picture_factory.hpp"
  #include "png_picture.hpp"

  #include "char_pool.hpp"
  #include "himem.hpp"
  #include "simple_db.hpp"
  #include "unzip.hpp"

  #if EPUB_INKPLATE_BUILD
    #include "esp_heap_caps.h"
    #include "esp_system.h"
    #include "freertos/task.h"
  #endif

  #include <iomanip>
  #include <iostream>

  namespace {

  template <typename T>
  void printClassSize(const char *name) {
    std::cout << std::left << std::setw(32) << name << " : " << std::setw(6) << sizeof(T)
              << " bytes" << std::endl;
  }

  #if EPUB_INKPLATE_BUILD
    struct HeapCap {
      const char *name;
      uint32_t caps;
    };

    void printHeapCapInfo(const HeapCap &entry) {
      multi_heap_info_t info;
      heap_caps_get_info(&info, entry.caps);

      std::cout << "\n[" << entry.name << "]\n"
                << "  total      : " << heap_caps_get_total_size(entry.caps) << " bytes\n"
                << "  free       : " << info.total_free_bytes << " bytes\n"
                << "  allocated  : " << info.total_allocated_bytes << " bytes\n"
                << "  min free   : " << info.minimum_free_bytes << " bytes\n"
                << "  largest    : " << info.largest_free_block << " bytes\n"
                << "  blocks     : " << info.total_blocks << " (free=" << info.free_blocks
                << ", alloc=" << info.allocated_blocks << ")" << std::endl;
    }
  #endif

  } // namespace

  auto DebugTool::printAllClassSizes() -> void {
    std::cout << "========== Class Size Report (src + components) ==========" << std::endl;

    printClassSize<OptionController>("OptionController");
    printClassSize<BookController>("BookController");
    printClassSize<BookParamController>("BookParamController");
    printClassSize<AppController>("AppController");
    printClassSize<CommonActions>("CommonActions");
    printClassSize<TocController>("TocController");
    printClassSize<BooksDirController>("BooksDirController");
    printClassSize<EventMgr>("EventMgr");
    printClassSize<Duration>("Duration");
    printClassSize<BooksDir>("BooksDir");
    printClassSize<TOC>("TOC");
    printClassSize<IBMF>("IBMF");
    printClassSize<Fonts>("Fonts");
    printClassSize<TTF>("TTF");
    printClassSize<IBMFFont>("IBMFFont");

    #if EPUB_INKPLATE_BUILD
      printClassSize<NVSMgr>("NVSMgr");
    #endif

    printClassSize<DisplayListEntry>("DisplayListEntry");
    printClassSize<DisplayList>("DisplayList");
    printClassSize<FontFactory>("FontFactory");
    printClassSize<CSS>("CSS");
    printClassSize<EPub>("EPub");
    printClassSize<CSSParser>("CSSParser");
    printClassSize<PageLocs>("PageLocs");
    printClassSize<Font>("Font");
    printClassSize<DOM>("DOM");
    printClassSize<MenuViewer>("MenuViewer");
    printClassSize<LinearBooksDirViewer>("LinearBooksDirViewer");
    printClassSize<ScreenBottom>("ScreenBottom");
    printClassSize<BooksDirViewer>("BooksDirViewer");
    printClassSize<TocViewer>("TocViewer");
    printClassSize<FormField>("FormField");
    printClassSize<FormChoiceField>("FormChoiceField");
    printClassSize<VFormChoiceField>("VFormChoiceField");
    printClassSize<HFormChoiceField>("HFormChoiceField");
    printClassSize<FormUInt16>("FormUInt16");
    printClassSize<FieldFactory>("FieldFactory");
    printClassSize<FormViewer>("FormViewer");
    printClassSize<MsgViewer>("MsgViewer");
    printClassSize<Page>("Page");
    printClassSize<HTMLInterpreter>("HTMLInterpreter");
    printClassSize<MatrixBooksDirViewer>("MatrixBooksDirViewer");

    #if 0
    printClassSize<KeyboardViewer>("KeyboardViewer");
    #endif

    printClassSize<BookViewer>("BookViewer");
    printClassSize<KeypadViewer>("KeypadViewer");
    printClassSize<Screen>("Screen");
    printClassSize<Unzip>("Unzip");

    // #if EPUB_INKPLATE_BUILD
    //   printClassSize<PsramAllocator>("PsramAllocator");
    // #endif

    printClassSize<JPegPicture>("JPegPicture");
    printClassSize<BitmapPicture>("BitmapPicture");
    printClassSize<PngPicture>("PngPicture");
    printClassSize<Picture>("Picture");
    printClassSize<PictureFactory>("PictureFactory");
    printClassSize<SimpleDB>("SimpleDB");
    printClassSize<CharPool>("CharPool");

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
      printClassSize<BackLit>("BackLit");
    #endif

    #if DATE_TIME_RTC
      printClassSize<Clock>("Clock");
      printClassSize<NTP>("NTP");
    #endif

    #if EPUB_INKPLATE_BUILD
      printClassSize<WIFI>("WIFI");
    #endif
  }

  auto DebugTool::printMemoryState() -> void {
    #if EPUB_INKPLATE_BUILD
      std::cout << "========== Memory State Report ==========" << std::endl;
      std::cout << "Min free heap observed : " << esp_get_minimum_free_heap_size() << " bytes"
                << std::endl;

      TaskStatus_t taskInfo{};
      TaskHandle_t currentTask = xTaskGetCurrentTaskHandle();
      vTaskGetInfo(currentTask, &taskInfo, pdTRUE, eInvalid);

      size_t stackSizeBytes = 0;
      if (taskInfo.pxStackBase != nullptr) {
        stackSizeBytes = heap_caps_get_allocated_size(taskInfo.pxStackBase);
      }

      const size_t stackUnusedBytes =
          static_cast<size_t>(taskInfo.usStackHighWaterMark) * sizeof(StackType_t);

      std::cout << "Current task stack size: " << stackSizeBytes << " bytes" << std::endl;
      std::cout << "Current task unused stack (high-water mark): " << stackUnusedBytes << " bytes"
                << std::endl;

      const HeapCap caps[] = {
          {"8-bit capable", MALLOC_CAP_8BIT},
          {"32-bit capable", MALLOC_CAP_32BIT},
          {"Internal", MALLOC_CAP_INTERNAL},
          {"SPI RAM", MALLOC_CAP_SPIRAM},
          {"DMA", MALLOC_CAP_DMA},
          {"Executable", MALLOC_CAP_EXEC},
      };

      for (const auto &entry : caps) {
        printHeapCapInfo(entry);
      }
    #else
      std::cout << "Memory state report is available only with EPUB_INKPLATE_BUILD." << std::endl;
    #endif
  }

#endif