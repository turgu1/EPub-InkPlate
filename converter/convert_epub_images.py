"""

Guy Turcotte
2026-05-31

This script processes an EPUB file, converting all embedded images to 4-bit 
grayscale BMP format and resizing them to fit within specified maximum dimensions. 
It also updates the EPUB's XHTML/HTML files to reference the new BMP images. 
The modified EPUB is then saved to a new file with a "-converted" suffix.

The following libraries are required:

- ebooklib: For reading and writing EPUB files.
- BeautifulSoup: For parsing and modifying XHTML/HTML content within the EPUB.
- Pillow (PIL): For image processing tasks such as resizing and format conversion.

They can be installed using pip:
    pip install ebooklib beautifulsoup4 Pillow

Usage:
    python convert_epub_images.py <input.epub> <max_width> <max_height>

Example:
    python convert_epub_images.py my_book.epub 600 800

    The above command will process "my_book.epub", resizing images to fit 
    within 600x800 pixels, and save the modified EPUB as "my_book_converted.epub".

"""

import os
import sys
import io
import struct
import ebooklib
from ebooklib import epub
from bs4 import BeautifulSoup
from PIL import Image

def get_flattened_data(img):
    """
    Extracts pixel data safely without triggering Pillow 14 deprecation warnings.
    """
    return list(img.tobytes())

def convert_to_4bpp_bmp_stable(img_bytes, max_width, max_height, file_name):
    """
    Converts image to a flawless, standard uncompressed 4-bit per pixel (bpp) BMP.
    Packs exactly two pixels per byte and pads scanlines to 4-byte boundaries.
    """
    try:
        orig_img = Image.open(io.BytesIO(img_bytes))
        orig_fmt = orig_img.format or "UNKNOWN"
    except Exception:
        return None

    orig_width, orig_height = orig_img.size
    
    # Enforce original bounding box rules (stay at original size if smaller)
    if orig_width > max_width or orig_height > max_height:
        orig_img.thumbnail((max_width, max_height), Image.Resampling.LANCZOS if hasattr(Image, 'Resampling') else Image.ANTIALIAS)
    
    gray_img = orig_img.convert('L')
    quantized_img = gray_img.quantize(colors=16, method=0)
    
    width, height = quantized_img.size
    palette = quantized_img.getpalette()

    # Generate the standard 16-color BMP palette layout (4 bytes per color: B, G, R, Reserved)
    bmp_palette = bytearray()
    for i in range(16):
        if palette and (i * 3 + 2) < len(palette):
            r, g, b = palette[i*3], palette[i*3+1], palette[i*3+2]
        else:
            val = int(i * 255 / 15)
            r, g, b = val, val, val
        bmp_palette.extend([b, g, r, 0])

    # Extract pixel data safely via tobytes() mapping
    pixels = get_flattened_data(quantized_img)
    pixel_data = bytearray()
    
    # Calculate row dimensions and trailing padding to align to 32-bit (4-byte) boundaries
    row_bytes_len = (width + 1) // 2
    padding_len = (4 - (row_bytes_len % 4)) % 4
    
    # BMP files evaluate scan lines starting from bottom to top
    for y in reversed(range(height)):
        row_data = bytearray()
        for x in range(0, width, 2):
            p1 = pixels[y * width + x]
            p2 = pixels[y * width + x + 1] if (x + 1) < width else 0
            
            # Pack two 4-bit nibbles tightly into a single 8-bit byte chunk
            byte_val = ((p1 & 0x0F) << 4) | (p2 & 0x0F)
            row_data.append(byte_val)
        
        # Append zeros for the required 4-byte row padding boundary
        row_data.extend(b'\x00' * padding_len)
        pixel_data.extend(row_data)

    # Build Header Profiles
    offset = 14 + 40 + len(bmp_palette)
    file_size = offset + len(pixel_data)
    
    file_header = struct.pack('<2sIHHI', b'BM', file_size, 0, 0, offset)
    
    # Fields: Size(40), W, H, Planes(1), BitsPerPixel(4), Compression(0=BI_RGB uncompressed), DataSize...
    info_header = struct.pack('<IiiHHIIiiII', 40, width, height, 1, 4, 0, len(pixel_data), 2835, 2835, 16, 16)
    
    final_bmp_bytes = file_header + info_header + bmp_palette + pixel_data
    
    # LOGGING: Print metrics for this specific image transformation
    old_size = len(img_bytes)
    new_size = len(final_bmp_bytes)
    saved_pct = ((old_size - new_size) / old_size) * 100
    
    print(f" -> Image: {os.path.basename(file_name)}")
    print(f"    Format: {orig_fmt} -> BMP (Uncompressed 4bpp)")
    print(f"    Dimensions: {orig_width}x{orig_height} -> {width}x{height}")
    print(f"    File Size: {old_size:,} bytes -> {new_size:,} bytes ({saved_pct:+.2f}%)")
    print("-" * 50)
    
    return final_bmp_bytes

def process_epub(epub_path, max_width, max_height):
    base, ext = os.path.splitext(epub_path)
    output_path = f"{base}_converted{ext}"

    print(f"\nProcessing: {os.path.basename(epub_path)}")
    print("=" * 50)

    try:
        book = epub.read_epub(epub_path)
    except Exception as e:
        print(f"Error reading EPUB: {e}")
        return

    processed_items = {}
    images_converted_count = 0

    for item in book.get_items():
        if item.get_type() == ebooklib.ITEM_IMAGE:
            old_name = item.get_name()
            _, old_ext = os.path.splitext(old_name)
            
            # Ignore and bypass BOTH GIF and SVG files completely
            if old_ext.lower() in ('.gif', '.svg'):
                continue
            
            old_content = item.get_content()
            new_content = convert_to_4bpp_bmp_stable(old_content, max_width, max_height, old_name)
            
            if new_content:
                base_name, _ = os.path.splitext(old_name)
                new_name = base_name + '.bmp'
                
                processed_items[old_name] = new_name
                item.set_content(new_content)
                item.file_name = new_name
                item.media_type = 'image/bmp'
                images_converted_count += 1

    filename_map = {os.path.basename(k): os.path.basename(v) for k, v in processed_items.items()}

    for item in book.get_items_of_type(ebooklib.ITEM_DOCUMENT):
        content = item.get_content()
        soup = BeautifulSoup(content, 'html.parser')
        modified = False
        
        for img in soup.find_all('img'):
            src = img.get('src')
            if src:
                src_base = os.path.basename(src)
                if src_base in filename_map:
                    dir_name = os.path.dirname(src)
                    img['src'] = os.path.join(dir_name, filename_map[src_base]).replace('\\', '/')
                    modified = True
                
        for svg_img in soup.find_all('image'):
            href_attr = 'xlink:href' if svg_img.get('xlink:href') else 'href'
            src = svg_img.get(href_attr)
            if src:
                src_base = os.path.basename(src)
                if src_base in filename_map:
                    dir_name = os.path.dirname(src)
                    svg_img[href_attr] = os.path.join(dir_name, filename_map[src_base]).replace('\\', '/')
                    modified = True
                    
        if modified:
            item.set_content(str(soup).encode('utf-8'))

    epub.write_epub(output_path, book)
    print(f"Done! Converted {images_converted_count} images.")
    print(f"Output saved to: {output_path}\n")

if __name__ == '__main__':
    if len(sys.argv) < 4:
        print("Usage: python convert_epub_images.py <input.epub> <max_width> <max_height>")
        sys.exit(1)

    input_epub = sys.argv[1]
    width = int(sys.argv[2])
    height = int(sys.argv[3])

    process_epub(input_epub, width, height)
