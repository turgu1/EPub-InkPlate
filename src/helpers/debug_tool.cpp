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
  #include "models/display_list.hpp"
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
  void print_class_size(const char *name) {
    std::cout << std::left << std::setw(32) << name << " : " << std::setw(6) << sizeof(T)
              << " bytes" << std::endl;
  }

  #if EPUB_INKPLATE_BUILD
    struct HeapCap {
      const char *name;
      uint32_t caps;
    };

    void print_heap_cap_info(const HeapCap &entry) {
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

  void DebugTool::print_all_class_sizes() {
    std::cout << "========== Class Size Report (src + components) ==========" << std::endl;

    print_class_size<OptionController>("OptionController");
    print_class_size<BookController>("BookController");
    print_class_size<BookParamController>("BookParamController");
    print_class_size<AppController>("AppController");
    print_class_size<CommonActions>("CommonActions");
    print_class_size<TocController>("TocController");
    print_class_size<BooksDirController>("BooksDirController");
    print_class_size<EventMgr>("EventMgr");
    print_class_size<Duration>("Duration");
    print_class_size<BooksDir>("BooksDir");
    print_class_size<TOC>("TOC");
    print_class_size<IBMF>("IBMF");
    print_class_size<Fonts>("Fonts");
    print_class_size<TTF>("TTF");
    print_class_size<IBMFFont>("IBMFFont");

    #if EPUB_INKPLATE_BUILD
      print_class_size<NVSMgr>("NVSMgr");
    #endif

    print_class_size<DisplayListEntry>("DisplayListEntry");
    print_class_size<DisplayList>("DisplayList");
    print_class_size<FontFactory>("FontFactory");
    print_class_size<CSS>("CSS");
    print_class_size<EPub>("EPub");
    print_class_size<CSSParser>("CSSParser");
    print_class_size<PageLocs>("PageLocs");
    print_class_size<Font>("Font");
    print_class_size<DOM>("DOM");
    print_class_size<MenuViewer>("MenuViewer");
    print_class_size<LinearBooksDirViewer>("LinearBooksDirViewer");
    print_class_size<ScreenBottom>("ScreenBottom");
    print_class_size<BooksDirViewer>("BooksDirViewer");
    print_class_size<TocViewer>("TocViewer");
    print_class_size<FormField>("FormField");
    print_class_size<FormChoiceField>("FormChoiceField");
    print_class_size<VFormChoiceField>("VFormChoiceField");
    print_class_size<HFormChoiceField>("HFormChoiceField");
    print_class_size<FormUInt16>("FormUInt16");
    print_class_size<FieldFactory>("FieldFactory");
    print_class_size<FormViewer>("FormViewer");
    print_class_size<MsgViewer>("MsgViewer");
    print_class_size<Page>("Page");
    print_class_size<HTMLInterpreter>("HTMLInterpreter");
    print_class_size<MatrixBooksDirViewer>("MatrixBooksDirViewer");

    #if 0
    print_class_size<KeyboardViewer>("KeyboardViewer");
    #endif

    print_class_size<BookViewer>("BookViewer");
    print_class_size<KeypadViewer>("KeypadViewer");
    print_class_size<Screen>("Screen");
    print_class_size<Unzip>("Unzip");

    // #if EPUB_INKPLATE_BUILD
    //   print_class_size<PsramAllocator>("PsramAllocator");
    // #endif

    print_class_size<JPegPicture>("JPegPicture");
    print_class_size<BitmapPicture>("BitmapPicture");
    print_class_size<PngPicture>("PngPicture");
    print_class_size<Picture>("Picture");
    print_class_size<PictureFactory>("PictureFactory");
    print_class_size<SimpleDB>("SimpleDB");
    print_class_size<CharPool>("CharPool");

    #if INKPLATE_6PLUS || INKPLATE_6PLUS_V2 || INKPLATE_6FLICK
      print_class_size<BackLit>("BackLit");
    #endif

    #if DATE_TIME_RTC
      print_class_size<Clock>("Clock");
      print_class_size<NTP>("NTP");
    #endif

    #if EPUB_INKPLATE_BUILD
      print_class_size<WIFI>("WIFI");
    #endif
  }

  void DebugTool::print_memory_state() {
    #if EPUB_INKPLATE_BUILD
      std::cout << "========== Memory State Report ==========" << std::endl;
      std::cout << "Min free heap observed : " << esp_get_minimum_free_heap_size() << " bytes"
                << std::endl;

      TaskStatus_t task_info{};
      TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
      vTaskGetInfo(current_task, &task_info, pdTRUE, eInvalid);

      size_t stack_size_bytes = 0;
      if (task_info.pxStackBase != nullptr) {
        stack_size_bytes = heap_caps_get_allocated_size(task_info.pxStackBase);
      }

      const size_t stack_unused_bytes =
          static_cast<size_t>(task_info.usStackHighWaterMark) * sizeof(StackType_t);

      std::cout << "Current task stack size: " << stack_size_bytes << " bytes" << std::endl;
      std::cout << "Current task unused stack (high-water mark): " << stack_unused_bytes << " bytes"
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
        print_heap_cap_info(entry);
      }
    #else
      std::cout << "Memory state report is available only with EPUB_INKPLATE_BUILD." << std::endl;
    #endif
  }

#endif