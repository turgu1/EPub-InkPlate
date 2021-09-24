#if TESTING && EPUB_LINUX_BUILD

#include "gtest/gtest.h"
#include "models/epub.hpp"

TEST(EpubTest, opening_epub_file) {
  EXPECT_TRUE(epub.open_file(BOOKS_FOLDER "/Austen, Jane - Pride and Prejudice.epub"));
}

TEST(EpubTest, reading_identifier) {
  EXPECT_TRUE(epub.open_file(BOOKS_FOLDER "/Austen, Jane - Pride and Prejudice.epub"));
  EXPECT_EQ(epub.get_unique_identifier(), "http://www.gutenberg.org/1342");
}

TEST(EpubTest, get_uuid) {
  EPub::BinUUID uuid_res = { 0x91, 0xd3, 0x52, 0xf0, 0x53, 0xc7, 0x43, 0x61, 
                             0xb8, 0x46, 0x17, 0x65, 0x9d, 0x64, 0xe9, 0x76 };
  EXPECT_TRUE(epub.open_file(BOOKS_FOLDER "/test_obfuscation1.epub"));
  const EPub::BinUUID & uuid = epub.get_bin_uuid();
  EXPECT_TRUE(memcmp(uuid, uuid_res, 16) == 0);
}

TEST(EpubTest, check_adobe_obfuscated_file) {
  EXPECT_TRUE(epub.open_file(BOOKS_FOLDER "/test_obfuscation2.epub"));
  EXPECT_TRUE(epub.encryption_is_present());
  EXPECT_TRUE(epub.get_file_obfuscation("fonts/00005.otf") == EPub::ObfuscationType::ADOBE);
  EXPECT_TRUE(epub.load_font("fonts/00005.otf", "Test", Fonts::FaceStyle::NORMAL));
  EXPECT_TRUE(epub.get_file_obfuscation("fonts/00000.otf") == EPub::ObfuscationType::NONE);
}

TEST(EpubTest, check_adobe_idpf_file) {
  EXPECT_TRUE(epub.open_file(BOOKS_FOLDER "/test_obfuscation4.epub"));
  EXPECT_TRUE(epub.encryption_is_present());
  EXPECT_TRUE(epub.get_file_obfuscation("OEBPS/Fonts/Palatino-Roman.ttf") == EPub::ObfuscationType::IDPF);
  EXPECT_TRUE(epub.load_font("OEBPS/Fonts/Palatino-Roman.ttf", "Test", Fonts::FaceStyle::NORMAL));
}

#endif
