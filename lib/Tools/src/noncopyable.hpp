#ifndef __NON_COPYABLE_HPP__
#define __NON_COPYABLE_HPP__

// Requires C++ 11

/**
 * @brief Make children classes impossible to be copied.
 * 
 * This is required to ensure that singleton objects wouldn't be copied by
 * user code. To be used, a singleton class must be derived as a NonCopyable
 * 
 * 
 */
class NonCopyable
{
  public:
    NonCopyable(const NonCopyable &) = delete;
    NonCopyable & operator= (const NonCopyable &) = delete;
  
  protected:
    constexpr NonCopyable() = default;
    ~NonCopyable() = default;
};

#endif