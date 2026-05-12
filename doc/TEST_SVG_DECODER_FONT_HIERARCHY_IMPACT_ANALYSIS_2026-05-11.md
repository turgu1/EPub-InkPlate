# Impact Analysis: test_svg_decoder Font Dependency Hierarchy

Date: 2026-05-11

## Problem Statement

SVG support has been refactored to receive fonts through `PictureFactory::create()`. The FontPtr reference is normally supplied by the Fonts class, which depends on FontsDb class, which is maintained by the Config class.

Question: How should `test_svg_decoder` be adapted to respect this class hierarchy as much as possible?

## Current Architecture: Production Code

### Dependency Chain
```
Config (config.txt) 
  ↓
ConfigIdent::FONTS_DB 
  ↓
FontsDB* (loaded from config)
  ↓
Fonts (appFonts instance with thisIsAppFonts=true)
  ↓
Fonts::setup() loads FontsDB
  ↓
Fonts::getFont(index) → FontPtr
  ↓
PictureFactory::create(..., FontPtr) → PicturePtr
```

### Initialization Sequence (main.cpp)
1. **Config initialization** (line 101):
   ```cpp
   bool configErr = !config.read();  // Reads config.txt
   ```

2. **FontsDB retrieval** (fonts.cpp:84):
   ```cpp
   config.get(Config::Ident::FONTS_DB, &fontsDB);
   ```

3. **Fonts setup** (main.cpp:135 or 276):
   ```cpp
   if (appFonts.setup()) { ... }  // Loads system fonts from FontsDB
   ```

4. **Font retrieval** (various places):
   ```cpp
   appFonts.getFont(SYSTEM_REGULAR_FONT_INDEX)  // Get font #1 (system regular)
   ```

5. **Picture creation** (epub.cpp:1143):
   ```cpp
   auto pict = PictureFactory::create(
       filename, 
       Dim(Screen::getWidth(), Screen::getHeight()), 
       load,
       fonts.getFont(SYSTEM_REGULAR_FONT_INDEX)
   );
   ```

### Key Global Variables in Production
- `extern Config config;` (components/config/src/config.hpp:147)
- `extern Fonts appFonts;` (components/fonts/src/fonts.hpp:187)
- Both are defined with `#define __GLOBAL__ 1` in src/main.cpp

## Current test_svg_decoder Implementation

### Current Behavior
- **Direct SvgDecoder testing only**: test_svg_decoder bypasses PictureFactory entirely
- **No font dependencies**: SvgDecoder::openRAM() takes raw bytes and a callback, no font required
- **No Config/FontsDB/Fonts setup**: Test runs in isolation
- **Works fine today**: SVG fixtures render without font support (SvgDecoder falls back to embedded 5x7 bitmap font)

### Code Flow
```
testSvgDecoder()
  ↓
loadSvgFixtures()  // Load .svg files from test/fixtures/svgs/
  ↓
decodeFixtureWithDimensionChecks()
  ↓
SvgDecoder::openRAM(bytes, callback)
SvgDecoder::decode(0, 0, options)
```

### Current Limitation
- **PictureFactory cannot be tested** directly from test_svg_decoder
- **SvgPicture creation is untested** in isolation
- If SvgPicture::Make() has issues with font initialization, test_svg_decoder won't catch them

## Impact Analysis: Three Approaches

### Approach A: Minimal Change (Current Approach - No Font Dependencies)
**Feasibility: ✓ Works Now**

Keep test_svg_decoder testing only SvgDecoder directly. Don't integrate the font hierarchy.

**Pros:**
- Zero changes to test_svg_decoder
- Test remains simple and focused
- No Config/FontsDB initialization overhead
- Continues to work with embedded fallback font

**Cons:**
- PictureFactory::create() path remains untested
- SVG text rendering with actual system fonts is never validated in tests
- Cannot catch font resolution issues early
- Misses integration testing opportunity

**Scope:**
- No changes to test_svg_decoder.cpp
- No changes to test build configuration

**Recommendation:** Not recommended if SVG font support is a critical feature.

---

### Approach B: Full Hierarchy Integration (Respects Full Dependency Chain)
**Feasibility: ⭐⭐⭐⭐ (Preferred)**

Add Config/FontsDB/Fonts setup to test_svg_decoder, then optionally test via PictureFactory.

**Pros:**
- **Respects production hierarchy completely**
- Tests both SvgDecoder directly AND via PictureFactory
- Catches font initialization bugs
- SVG text rendering validated with real system fonts
- Mirrors production initialization sequence

**Cons:**
- Requires config.txt or test-specific config
- FontsDB must be available (fonts directory must exist)
- Test becomes slightly more complex
- Slower initialization for font system

**Implementation Steps:**

1. **Create minimal test config** (test/config_test.txt or embed in code):
   ```
   FONTS_DB: <reference to system fonts or test fonts>
   ```

2. **Initialize Config in test setup**:
   ```cpp
   Config config("test/config_test.txt", false);
   if (!config.read()) {
       // Handle config load failure
   }
   ```

3. **Retrieve FontsDB**:
   ```cpp
   FontsDB* fontsDB = nullptr;
   config.get(ConfigIdent::FONTS_DB, &fontsDB);
   ```

4. **Initialize Fonts**:
   ```cpp
   Fonts testFonts(true);  // thisIsAppFonts = true
   if (!testFonts.setup()) {
       // Handle font setup failure
   }
   ```

5. **Optional: Test PictureFactory path**:
   ```cpp
   FontPtr systemFont = testFonts.getFont(SYSTEM_REGULAR_FONT_INDEX);
   auto pict = PictureFactory::create(
       filename, 
       Dim(width, height), 
       true,
       systemFont
   );
   ```

**Test Structure:**
```cpp
class SvgDecoderTest {
    static Config* testConfig;
    static Fonts* testFonts;
    
    static void SetUpTestSuite() {
        // Initialize Config and Fonts once for all tests
        testConfig = new Config("test/config_test.txt", false);
        testConfig->read();
        
        testFonts = new Fonts(true);
        testFonts->setup();
    }
    
    static void TearDownTestSuite() {
        delete testFonts;
        delete testConfig;
    }
    
    // Test both direct SvgDecoder and PictureFactory paths
};
```

**New Test Cases to Add:**
- Test SVG with text elements using system font
- Test SvgPicture::Make() with proper font
- Test PictureFactory::create() with SVG + font
- Test font fallback behavior

**Dependencies:**
- config.txt must exist or be created for testing
- fonts directory structure must be present
- System fonts must be loadable

**File Changes Required:**
- test/test_svg_decoder.cpp: Add Config/Fonts setup + new test cases
- Possibly: test/config_test.txt (new)
- Possibly: Build system to include fonts in test resources

---

### Approach C: Partial Integration with Stubs/Mocks
**Feasibility: ⭐⭐⭐**

Keep test_svg_decoder mostly simple, but add optional font support via test stubs.

**Pros:**
- Lighter weight than full integration
- Can test both paths (with/without font)
- Doesn't require full Config infrastructure
- Faster test execution

**Cons:**
- Doesn't fully respect production hierarchy
- Stub/mock maintenance burden
- Gaps in integration testing
- Font setup logic not tested

**Implementation:**
```cpp
class FontsStub {
    FontPtr getFont(int index) { /* return test font or nullptr */ }
};

// In test:
FontsStub testFonts;
FontPtr font = testFonts.getFont(SYSTEM_REGULAR_FONT_INDEX);
auto pict = PictureFactory::create(filename, dim, true, font);
```

**Not Recommended:** Stubs defeat the purpose of integration testing.

---

## Recommendation: Approach B (Full Hierarchy Integration)

### Rationale
1. **Respects the stated requirement** to respect the class hierarchy "as much as possible"
2. **Validates production paths** end-to-end
3. **Catches integration bugs** that direct SvgDecoder testing misses
4. **Font support becomes testable** with real system fonts
5. **Mirrors main.cpp** initialization, making test maintenance easier
6. **Future-proof**: As SVG features expand, hierarchy is already in place

### Implementation Priority

**Phase 1: Core Setup (Required)**
- Add Config initialization to test_svg_decoder
- Add Fonts initialization to test_svg_decoder
- Keep existing SvgDecoder direct tests
- Verify existing fixture tests still pass

**Phase 2: Extended Coverage (Optional)**
- Add test cases that use PictureFactory::create()
- Add SVG text rendering tests with system font
- Add font resolution tests

**Phase 3: Robustness (Nice-to-have)**
- Handle missing config/fonts gracefully
- Provide test config template
- Document setup requirements

### Concrete Changes Needed

**In test/test_svg_decoder.cpp:**
1. Add includes:
   ```cpp
   #include "config.hpp"
   #include "fonts.hpp"
   ```

2. Add static test fixtures:
   ```cpp
   static Config* g_testConfig = nullptr;
   static Fonts* g_testFonts = nullptr;
   ```

3. Add setup/teardown:
   ```cpp
   void SetupTestEnvironment() {
       // Initialize Config from test config or default
       // Initialize Fonts from Config's FontsDB
   }
   
   void TeardownTestEnvironment() {
       // Cleanup
   }
   ```

4. Add optional font-aware tests:
   ```cpp
   void testSvgDecoderWithSystemFont() {
       // Use g_testFonts to create SvgPicture with real font
   }
   ```

5. Call setup/teardown at testSvgDecoder() entry/exit

**In build system (if needed):**
- Ensure test has access to fonts directory
- Ensure config.txt or test config is available

**Test config requirements:**
- Must reference valid system fonts directory
- Must match what main app uses for testing
- Can be minimal (only FONTS_DB parameter)

## Questions for Implementation

Before proceeding with Phase 1, clarify:

1. **Should test_svg_decoder run with or without actual font files?**
   - With: Full integration, realistic but requires setup
   - Without: Fast startup but less realistic

2. **If fonts required, where should test config/fonts live?**
   - test/config_test.txt + test/fonts/
   - Or use symbolic link/copy of system fonts?
   - Or embedded minimal test fonts?

3. **How to handle missing config gracefully?**
   - Fail test with clear message?
   - Skip font tests if config unavailable?
   - Use fallback embedded font?

4. **Performance sensitivity?**
   - Is slower test initialization acceptable?
   - FontsDB loading can take time

5. **Regression tolerance?**
   - Should test validate exact font rendering output?
   - Or just that font path doesn't crash?

## Summary

| Aspect               | Approach A | Approach B | Approach C |
| -------------------- | ---------- | ---------- | ---------- |
| Respects hierarchy   | ✗          | ✓          | △          |
| Tests PictureFactory | ✗          | ✓          | △          |
| Font integration     | ✗          | ✓          | △          |
| Implementation cost  | Low        | Medium     | Low        |
| Maintenance burden   | Low        | Medium     | Medium     |
| Future extensibility | Poor       | Good       | Fair       |
| **Recommendation**   | No         | **Yes**    | Maybe      |

**Conclusion:** Adopt **Approach B** with **Phase 1** implementation to respect the dependency hierarchy and gain integration test coverage of SVG font rendering.
