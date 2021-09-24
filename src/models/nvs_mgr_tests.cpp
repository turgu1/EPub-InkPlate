#if TESTING && EPUB_INKPLATE_BUILD

#include "gtest/gtest.h"
#include "models/nvs_mgr.hpp"

TEST(NVSMgr, setup)
{
  EXPECT_TRUE(nvs_mgr.setup());
}

#endif

