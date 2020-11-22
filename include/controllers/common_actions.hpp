#ifndef __COMMON_ACTIONS_HPP__
#define __COMMON_ACTIONS_HPP__

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
};

#endif