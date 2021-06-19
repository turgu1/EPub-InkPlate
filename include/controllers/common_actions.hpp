#pragma once

#if __COMMON_ACTIONS__
  #define PUBLIC
#else
  #define PUBLIC extern
#endif

class CommonActions
{
  public:
    static void return_to_last();
    static void refresh_books_dir();
    static void power_off();
    static void show_last_book();
    static void about();
    #if EPUB_LINUX_BUILD && DEBUGGING
      static void debugging();
    #endif
};
