#!/usr/bin/env python3
"""
test/fixtures/gen_epub.py — generate minimal EPUB fixture files used by test_epub.cpp.

Run from the repository root:
    python3 test/fixtures/gen_epub.py

Generated files (all in test/fixtures/):
  minimal.epub       — well-formed EPUB 2.0 with title, author, 2 chapters, CSS, cover
  table.epub         — EPUB 2.0 with a single XHTML chapter containing a table
  bad_mimetype.epub  — mimetype file contains wrong string → EPub::open() must return false
  no_container.epub  — META-INF/container.xml is absent → EPub::open() must return false
  no_metadata.epub   — OPF has no dc:title / dc:creator; getTitle() / getAuthor() return ""
"""

import os, zipfile

FIXTURE_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)))

# ---------------------------------------------------------------------------
# Shared content strings
# ---------------------------------------------------------------------------

MIMETYPE = "application/epub+zip"

CONTAINER_XML = """\
<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0"
    xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="OEBPS/content.opf"
              media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>
"""

CONTENT_OPF = """\
<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0"
         unique-identifier="uid">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/"
            xmlns:opf="http://www.idpf.org/2007/opf">
    <dc:title>Test Book Title</dc:title>
    <dc:creator opf:role="aut">Test Author</dc:creator>
    <dc:description>A minimal test epub for unit tests.</dc:description>
    <dc:identifier id="uid">urn:uuid:12345678-1234-1234-1234-123456789abc</dc:identifier>
    <meta name="cover" content="cover-img"/>
  </metadata>
  <manifest>
    <item id="ch1"       href="ch1.xhtml"   media-type="application/xhtml+xml"/>
    <item id="ch2"       href="ch2.xhtml"   media-type="application/xhtml+xml"/>
    <item id="cover-img" href="cover.jpg"   media-type="image/jpeg"/>
    <item id="style"     href="style.css"   media-type="text/css"/>
    <item id="ncx"       href="toc.ncx"     media-type="application/x-dtbncx+xml"/>
  </manifest>
  <spine toc="ncx">
    <itemref idref="ch1"/>
    <itemref idref="ch2"/>
  </spine>
</package>
"""

CH1_XHTML = """\
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN"
  "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
    <title>Chapter One</title>
    <link rel="stylesheet" type="text/css" href="style.css"/>
  </head>
  <body>
    <h1>Chapter One</h1>
    <p>This is the first chapter of the test book.</p>
  </body>
</html>
"""

CH2_XHTML = """\
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN"
  "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
    <title>Chapter Two</title>
    <link rel="stylesheet" type="text/css" href="style.css"/>
  </head>
  <body>
    <h1>Chapter Two</h1>
    <p>This is the second chapter of the test book.</p>
  </body>
</html>
"""

# Plain CSS — no @font-face rules so that font loading is a no-op in tests.
STYLE_CSS = """\
body { font-family: serif; font-size: 1em; }
h1   { font-size: 1.4em; font-weight: bold; }
p    { margin: 0.5em 0; }
"""

TABLE_STYLE_CSS = """\
body   { font-family: serif; font-size: 1em; }
h1     { font-size: 1.4em; font-weight: bold; }
p      { margin: 0.5em 0; }
table  { width: 100%; border-collapse: collapse; margin: 1em 0; }
caption { font-weight: bold; text-align: left; margin-bottom: 0.4em; }
th, td { border: 1px solid #333; padding: 0.35em 0.5em; vertical-align: top; }
th     { background: #e9e9e9; }
"""

TABLE_CHAPTER_XHTML = """\
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN"
  "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
  <head>
    <title>Table Fixture</title>
    <link rel="stylesheet" type="text/css" href="style.css"/>
  </head>
  <body>
    <h1>Table Fixture</h1>
    <p>This chapter exists to exercise table layout and cell wrapping.</p>
    <table>
      <caption>Quarterly Results</caption>
      <thead>
        <tr>
          <th scope="col">Item</th>
          <th scope="col">Q1</th>
          <th scope="col">Q2</th>
          <th scope="col">Notes</th>
        </tr>
      </thead>
      <tbody>
        <tr>
          <th scope="row">Alpha</th>
          <td>12</td>
          <td>18</td>
          <td>Short note.</td>
        </tr>
        <tr>
          <th scope="row">Beta</th>
          <td>7</td>
          <td>11</td>
          <td>A slightly longer note to force wrapping in a narrow layout.</td>
        </tr>
        <tr>
          <th scope="row">Gamma</th>
          <td colspan="2">Merged cell</td>
          <td>Tests colspan handling.</td>
        </tr>
      </tbody>
    </table>
    <p>End of table fixture.</p>
  </body>
</html>
"""

TABLE_CONTENT_OPF = """\
<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0"
         unique-identifier="uid">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/"
            xmlns:opf="http://www.idpf.org/2007/opf">
    <dc:title>Table Fixture Book</dc:title>
    <dc:creator opf:role="aut">EPub-InkPlate Test Fixture</dc:creator>
    <dc:description>A minimal EPUB used to exercise HTML table formatting.</dc:description>
    <dc:identifier id="uid">urn:uuid:9d9c5f3f-8c3e-4d64-98a4-4c3e8f8c0f61</dc:identifier>
    <meta name="cover" content="cover-img"/>
  </metadata>
  <manifest>
    <item id="table"     href="table.xhtml" media-type="application/xhtml+xml"/>
    <item id="cover-img" href="cover.jpg"   media-type="image/jpeg"/>
    <item id="style"     href="style.css"   media-type="text/css"/>
    <item id="ncx"       href="toc.ncx"     media-type="application/x-dtbncx+xml"/>
  </manifest>
  <spine toc="ncx">
    <itemref idref="table"/>
  </spine>
</package>
"""

TABLE_TOC_NCX = """\
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE ncx PUBLIC "-//NISO//DTD ncx 2005-1//EN"
  "http://www.daisy.org/z3986/2005/ncx-2005-1.dtd">
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
  <head>
    <meta name="dtb:uid" content="urn:uuid:9d9c5f3f-8c3e-4d64-98a4-4c3e8f8c0f61"/>
  </head>
  <docTitle><text>Table Fixture Book</text></docTitle>
  <navMap>
    <navPoint id="np1" playOrder="1">
      <navLabel><text>Table Fixture</text></navLabel>
      <content src="table.xhtml"/>
    </navPoint>
  </navMap>
</ncx>
"""

TOC_NCX = """\
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE ncx PUBLIC "-//NISO//DTD ncx 2005-1//EN"
  "http://www.daisy.org/z3986/2005/ncx-2005-1.dtd">
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
  <head>
    <meta name="dtb:uid" content="urn:uuid:12345678-1234-1234-1234-123456789abc"/>
  </head>
  <docTitle><text>Test Book Title</text></docTitle>
  <navMap>
    <navPoint id="np1" playOrder="1">
      <navLabel><text>Chapter One</text></navLabel>
      <content src="ch1.xhtml"/>
    </navPoint>
    <navPoint id="np2" playOrder="2">
      <navLabel><text>Chapter Two</text></navLabel>
      <content src="ch2.xhtml"/>
    </navPoint>
  </navMap>
</ncx>
"""

# Minimal 1×1 JPEG (valid JFIF marker sequence, white pixel).
COVER_JPG = bytes([
    0xff,0xd8,0xff,0xe0,0x00,0x10,0x4a,0x46,0x49,0x46,0x00,0x01,
    0x01,0x00,0x00,0x01,0x00,0x01,0x00,0x00,0xff,0xdb,0x00,0x43,
    0x00,0x08,0x06,0x06,0x07,0x06,0x05,0x08,0x07,0x07,0x07,0x09,
    0x09,0x08,0x0a,0x0c,0x14,0x0d,0x0c,0x0b,0x0b,0x0c,0x19,0x12,
    0x13,0x0f,0x14,0x1d,0x1a,0x1f,0x1e,0x1d,0x1a,0x1c,0x1c,0x20,
    0x24,0x2e,0x27,0x20,0x22,0x2c,0x23,0x1c,0x1c,0x28,0x37,0x29,
    0x2c,0x30,0x31,0x34,0x34,0x34,0x1f,0x27,0x39,0x3d,0x38,0x32,
    0x3c,0x2e,0x33,0x34,0x32,0xff,0xc0,0x00,0x0b,0x08,0x00,0x01,
    0x00,0x01,0x01,0x01,0x11,0x00,0xff,0xc4,0x00,0x1f,0x00,0x00,
    0x01,0x05,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,
    0x09,0x0a,0x0b,0xff,0xda,0x00,0x08,0x01,0x01,0x00,0x00,0x3f,
    0x00,0xfb,0x00,0x1e,0xff,0xd9,
])

# ---------------------------------------------------------------------------
# OPF without any Dublin Core metadata (for testNoMetadata)
# ---------------------------------------------------------------------------
NO_META_OPF = """\
<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0"
         unique-identifier="uid">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/"
            xmlns:opf="http://www.idpf.org/2007/opf">
    <dc:identifier id="uid">urn:uuid:aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee</dc:identifier>
  </metadata>
  <manifest>
    <item id="ch1" href="ch1.xhtml" media-type="application/xhtml+xml"/>
  </manifest>
  <spine>
    <itemref idref="ch1"/>
  </spine>
</package>
"""

# ---------------------------------------------------------------------------
# Helper
# ---------------------------------------------------------------------------

def write_epub(name, files):
    """Write a ZIP (EPUB) file.  The 'mimetype' entry, if present, is stored
    uncompressed as required by the EPUB spec."""
    path = os.path.join(FIXTURE_DIR, name)
    with zipfile.ZipFile(path, "w") as zf:
        for entry_name, content in files.items():
            method = zipfile.ZIP_STORED if entry_name == "mimetype" else zipfile.ZIP_DEFLATED
            if isinstance(content, str):
                content = content.encode("utf-8")
            zf.writestr(
                zipfile.ZipInfo(entry_name),
                content,
                compress_type=method,
            )
    print(f"  Written: {path}")

# ---------------------------------------------------------------------------
# Generate fixtures
# ---------------------------------------------------------------------------

print("Generating EPUB test fixtures …")

write_epub("minimal.epub", {
    "mimetype":                MIMETYPE,
    "META-INF/container.xml":  CONTAINER_XML,
    "OEBPS/content.opf":       CONTENT_OPF,
    "OEBPS/ch1.xhtml":         CH1_XHTML,
    "OEBPS/ch2.xhtml":         CH2_XHTML,
    "OEBPS/style.css":         STYLE_CSS,
    "OEBPS/toc.ncx":           TOC_NCX,
    "OEBPS/cover.jpg":         COVER_JPG,
})

write_epub("table.epub", {
    "mimetype":                MIMETYPE,
    "META-INF/container.xml":  CONTAINER_XML,
    "OEBPS/content.opf":       TABLE_CONTENT_OPF,
    "OEBPS/table.xhtml":       TABLE_CHAPTER_XHTML,
    "OEBPS/style.css":         TABLE_STYLE_CSS,
    "OEBPS/toc.ncx":           TABLE_TOC_NCX,
    "OEBPS/cover.jpg":         COVER_JPG,
})

write_epub("bad_mimetype.epub", {
    "mimetype":                "text/plain",   # deliberately wrong
    "META-INF/container.xml":  CONTAINER_XML,
    "OEBPS/content.opf":       CONTENT_OPF,
    "OEBPS/ch1.xhtml":         CH1_XHTML,
})

write_epub("no_container.epub", {
    "mimetype":         MIMETYPE,
    # META-INF/container.xml intentionally omitted
    "OEBPS/content.opf": CONTENT_OPF,
    "OEBPS/ch1.xhtml":   CH1_XHTML,
})

write_epub("no_metadata.epub", {
    "mimetype":                MIMETYPE,
    "META-INF/container.xml":  CONTAINER_XML,
    "OEBPS/content.opf":       NO_META_OPF,
    "OEBPS/ch1.xhtml":         CH1_XHTML,
})

print("Done.")
