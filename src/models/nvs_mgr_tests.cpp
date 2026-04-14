#if TESTING && EPUB_INKPLATE_BUILD

  #include "models/nvs_mgr.hpp"
  #include "gtest/gtest.h"

  TEST(NVSMgr, setup) { EXPECT_TRUE(nvsMgr.setup()); }

#endif
