# XT Format Specification v1.0
**Target Platform**: ESP32 E-Paper Display Devices

## Overview
Four binary formats for ESP32 e-paper displays:

| Format | Extension | Description |
|--------|-----------|-------------|
| **XTG** | `.xtg` | 1-bit monochrome bitmap |
| **XTH** | `.xth` | 2-bit (4-level) grayscale bitmap |
| **XTC** | `.xtc` | Container for multiple XTG pages |
| **XTCH** | `.xtch` | XTC variant (different magic) |

**Byte Order**: All multi-byte values are little-endian.

---

## Common Header Pattern (XTG/XTH)
Both image formats share this 14-byte header:

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | uint32_t | magic | File identifier |
| 0x04 | 2 | uint16_t | width | Image width (pixels) |
| 0x06 | 2 | uint16_t | height | Image height (pixels) |
| 0x08 | 1 | uint8_t | compression | 0 = uncompressed |
| 0x09 | 1 | uint8_t | reserved | Must be 0 |
| 0x0A | 4 | uint32_t | dataSize | Size of image data in bytes |

**Magic Values**:
- XTG: `0x00475458` ("XTG\0")
- XTH: `0x00485458` ("XTH\0")

---

## XTG Format (Monochrome)

### Data Layout
- 1 bit per pixel, rows stored top-to-bottom
- Each row: left-to-right, 8 pixels/byte (MSB = leftmost pixel)
- **Data size**: `((width + 7) / 8) * height` bytes

### Pixel Mapping
| Bit | Color |
|-----|-------|
| 0 | Black |
| 1 | White |

*Note: Devices may invert this mapping.*

---

## XTH Format (4-Level Grayscale)

### Data Layout
Two bit planes stored sequentially, each using **vertical scan order**:

1. **Bit plane 1** (MSB of pixel value): Sent via command `0x24`
2. **Bit plane 2** (LSB of pixel value): Sent via command `0x26`

**Scan order** (both planes):
- Columns: right-to-left (x = width-1 → 0)
- Within column: top-to-bottom, 8 pixels/byte (MSB = topmost)

**Data size**: `((width * height + 7) / 8) * 2` bytes

### Pixel Mapping
| Value | Bits (b1,b2) | Display Color |
|-------|--------------|---------------|
| 0 | `00` | White |
| 1 | `01` | Dark Grey |
| 2 | `10` | Light Grey |
| 3 | `11` | Black |

*Note: Values 1 and 2 are swapped vs. linear grayscale—matches device LUT.*

**Pixel value**: `(bit1 << 1) \| bit2`

---

## XTC/XTCH Container Format

### Header (28 bytes)

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 4 | uint32_t | magic | `0x00435458` (XTC) or `0x48435458` (XTCH) |
| 0x04 | 1 | uint8_t | versionMajor | 2 |
| 0x05 | 1 | uint8_t | versionMinor | 0 |
| 0x06 | 2 | uint16_t | pageCount | Total pages |
| 0x08 | 1 | uint8_t | readDirection | 0 = L→R, 1 = R→L |
| 0x09 | 1 | uint8_t | hasChapters | 0/1 |
| 0x0A | 2 | uint16_t | pageWidth | Pixels |
| 0x0C | 2 | uint16_t | pageHeight | Pixels |
| 0x0E | 2 | uint16_t | reserved | 0 |
| 0x10 | 4 | uint32_t | indexOffset | Offset to page index |
| 0x14 | 4 | uint32_t | dataOffset | Offset to page data |
| 0x18 | 4 | uint32_t | chapterOffset | Offset to chapters (if any) |

### Page Index (13 bytes per page)
Stored at `indexOffset`:

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 8 | uint64_t | offset | Absolute file offset to XTG/XTH data |
| 0x08 | 4 | uint32_t | size | Complete file size (including header) |
| 0x0C | 1 | uint8_t | headerLevel | 0 = none, 1-6 = H1-H6 |

### Chapter Structure (if hasChapters=1)
96 bytes per chapter at `chapterOffset`:

| Offset | Size | Type | Field | Description |
|--------|------|------|-------|-------------|
| 0x00 | 80 | char[80] | name | UTF-8, null-terminated |
| 0x50 | 2 | uint16_t | startPage | 0-based |
| 0x52 | 2 | uint16_t | endPage | 0-based, inclusive |
| 0x54 | 4 | uint32_t | reserved1 | Must be 0 |
| 0x58 | 4 | uint32_t | reserved2 | Must be 0 |
| 0x5C | 4 | uint32_t | reserved3 | Must be 0 |

### File Layout
```
[Header (28)]
[Chapters (96 × N)]           (if hasChapters)
[Page Index (13 × pageCount)]  (at indexOffset)
[Page Data (variable)]         (at dataOffset)
```

---

## Magic Values Reference
| Format | Value (LE) | Bytes | ASCII |
|--------|------------|-------|-------|
| XTG | 0x00475458 | `58 54 47 00` | "XTG\0" |
| XTH | 0x00485458 | `58 54 48 00` | "XTH\0" |
| XTC | 0x00435458 | `58 54 43 00` | "XTC\0" |
| XTCH | 0x48435458 | `58 54 43 48` | "XTCH" |