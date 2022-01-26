# EPub-InkPlate An EPub Reader for InkPlate devices

## Last news

(Updated 2022.1.26)

Update to version 2.0.0

The following are the main aspects that have been updated in this release:

- Added Inkplate-6PLUS support as a new release. Mainly the touch screen, the WakeUp button, and the screen backlit capabilities. A calibration function is available to adjust the touch screen response. All interaction tools (menus, forms, viewers) have been extended to support gesture interactions. Gestures supported are Tap, Touch and hold, Pinching, Swipe left, Swipe right. 
- Date/Time support for all Inkplate device types. Include both Date/Time adjustment by hand and through the Internet (NTP server). Uses the RTC Chip when available, or the ESP32 internal real-time clock.
- Bitmap fonts to get more precise pixels painting on low-resolution screens. A specific font format was created to support such a feature (fonts with extension *.IBMF*)
- Fonts configuration file (*font_list.xml*) allows for changing the fonts used by the application.
- New fonts supplied with the distribution. Can be used to replace the fonts listed in the fonts configuration file.
- Better support of some EPUB books that use XML namespace extensively.

Installing this version requires the complete replacement of the micro SD Card content. The *SDCard* folder that is part of the release must be used to initialize the micro SD Card. The content of the config.txt file will need to be updated as stated in the installation manual.

The supplied documents (installation guide and user's guide) have been modified to take into account the new functionalities.

---

- [x] Font classes re-org to allow bitmap fonts
- [x] Tests with single PK file format
- [x] Create the IBMF font file format (my own file format design!)
- [x] Suite of tools to genereate IBMF font files
- [x] New class to manage Integrated BitMap Fonts (IBMF file format)
- [x] Character set translation
- [x] Support for both [x] monochrome and [x] grayscale
- [x] Glyphs translation to latin1 (multiple glyphs merge for accents)
- [x] Latin-A
- [x] `fonts_list.xml` format adjustments
- [x] Clear screen region performance improvement (20x faster)
- [x] RTC Date/Time support for all Inkplate devices
- [x] Inkplate-6PLUS touch screen calibration
- [x] FormViewer class redesign to support number fields
- [x] EPUB XML namespace parsing support
- [x] Numerical keypad for entering numbers in forms
- [x] Inkplate-6PLUS Debugging
- [x] Valgrind
- [x] Tests Inkplate-6 / Inkplate-10
- [x] Tests Inkplate-6PLUS
- [x] Documentation
- [x] Release generation script to be modified for Inkplate-6PLUS integration
- [x] New version release packaging

Update to version 1.3.1

- [x] Corrected an issue with books being read cleanup
- [x] Books being read are put on top of the Books list
- [x] Modifiable fonts list in the `fonts_list.xml` file
- [x] Wordsmithing of the user's guide (long overdue...)

- [x] Tests Inkplate-6 / Inkplate-10
- [x] Valgrind
- [x] Documentation
- [x] New version release packaging

---

Update to version 1.3.0

New User functionalities:

- [x] New ebook list Covers Matrix View (see pictures below).
- [x] Integration with new image streaming classes (png and jpeg).
- [x] Table of Content to select location to read inside an ebook.
- [x] Ebook deletion from the device.
- [x] Subscript/Superscript support.
- [x] Tracking of the current page location of the last 10 books read by the user.
- [x] Number of allowed ebooks changed from 100 to 200 maximum.

Internal changes:

- [x] Now using ESP-IDF version 4.3.0 through PlatformIO.
- [x] Complete redesign of the CSS Interpreter.
- [x] Integration in a single algorithm of the HTML Interpreter used by both the page location process and the screen painting process.
- [x] Correction of many small page painting issues.
- [x] Corrected an issue with the ebooks folder scanning process (simple db mgr bug).
- [x] Corrected an issue with the PowerOff menu entry.
- [x] Testing with 200 ebooks under Linux (page formatting and general functionalities).
- [x] Changes related to the new PlatformIO way of managing sdkconfig.
- [x] PNG image files transparency.
- [x] Stack usage optimization.
- [x] Web Server Enhancements (comma separated file size thousands, .epub file extension check).
- [x] Font obfuscation support (both Adobe and IDPF methods).
- [x] Non-volatile memory class to manage last books read tracking.
- [x] Testing ebooks on InkPlate-6.
- [x] Valgrind tests.
- [x] Documentation update.

Remaining steps to release:

- [x] Testing ebooks on InkPlate-10.
- [x] New version releases packaging.

---

- [ ] Support of the forecoming Inkplate 6Plus (touch screen, backlit). Debugging remains to be completed (will happen for a future release).

---

Update to version 1.2

- [x] Recompiled to integrate ESP-IDF-Inkplate library v0.9.4
- [x] Adjustments for the ESP-IDF v4.2 framework.
- [x] Support for the 6-buttons extended board in new specific releases.
- [x] bld_all.sh script to automatically build all releases zip files.

---

Update to version 1.1.1

- [x] Recompiled to integrate ESP-IDF-InkPlate library v0.9.2 (A2D attenuation correction).

v1.1.0 information:

This is in preparation for version 1.1.0 The main ongoing modifications are:

- [x] Support of the new upcoming Inkplate-10 device.
- [x] Just in time calculation of pages locations through multithreading (a **BIG** change...).
- [x] Integration with the new ESP-IDF-Inkplate library.
- [x] Trigger pages location recalculation when parameters have changed.
- [x] Stop light-sleep, deep-sleep while pages location is being calculated.
- [x] Adjust web server to only show epub files and remove params/locs files on ebook delete.
- [x] Adjust Web server to stop page-locs to free memory before launching the server.
- [x] Save computed pages location to sdcard for quick ebook load.
- [x] Repair some known bugs (some issues to be addressed later).
- [ ] Code refactoring and cleanup (tbd v1.2).
- [x] Udate documentation.
- [ ] Tests, tests, tests, ...

Added functional features:

- [x] Title showned at top of pages (as an option).
- [x] Heap Size showned at bottom of pages (as an option).
- [x] Form entry simplified (OK / Cancel param no longer there).
- [x] Parameters for each ebook specific font/pictures adjustments.
- [ ] Augmented CSS features (tbd v1.2).

Known bugs:

- [x] ebooks list geometry when screen orientation change.
- [x] recovery from too large image (memory allocation).
- [x] Page refresh on orientation/resolution changes.
- [ ] PNG image files transparency (tbd v1.2).

-----

v1.0 information:

The development is complete. The application is at version 1.0.0. Please look at the installation guide located in file `doc/INSTALL.md` and the user's guide located in `doc/USER GUIDE.md`. PDF versions of these guides are also available.

- [x] Integration of touch buttons through interrupts (not perfect. to be revisited).
- [x] Menu capability.
- [x] Options / Parameters menus.
- [x] Error dialogs.
- [x] About box.
- [x] Low-Level InkPlate Drivers refactoring.
- [x] Power management (Deep-Sleep after 15 minutes timeout, Light-Sleep between touchpad events).
- [x] Return to current book location between restarts.
- [x] Configuration management (save/load from the SD-Card).
- [x] Form tool to show / edit options / parameters.
- [x] books directory refresh dialog.
- [!] Over the Air (OTA) updates (No hope... not enough flash space).
- [x] WiFi access to update ebooks (This item only added 600KB of code!).
- [x] Performance on new book scans (80% completion to be revisited after first release).
- [x] Options / Parameters form.
- [x] Battery level display.
- [x] Screen orientation (touchpads to the left (portrait) / right (portrait) / down (landscape) modes).
- [x] User's Guide and installation manuals.
- [x] Error dialog use (100% completion).
- [x] Debugging remaining issues.

---
---


