#pragma once

#include <cstdint>
#include <string>

namespace xtc {

// XTC file magic numbers (little-endian)
// "XTC\0" = 0x58, 0x54, 0x43, 0x00
constexpr uint32_t XTC_MAGIC = 0x00435458;  // "XTC\0" in little-endian (1-bit fast mode)
// "XTCH" = 0x58, 0x54, 0x43, 0x48
constexpr uint32_t XTCH_MAGIC = 0x48435458;  // "XTCH" in little-endian (2-bit high quality mode)
// "XTG\0" = 0x58, 0x54, 0x47, 0x00
constexpr uint32_t XTG_MAGIC = 0x00475458;  // "XTG\0" for 1-bit page data
// "XTH\0" = 0x58, 0x54, 0x48, 0x00
constexpr uint32_t XTH_MAGIC = 0x00485458;  // "XTH\0" for 2-bit page data

// XTC file header (28 bytes)
#pragma pack(push, 1)
struct XtcHeader {
  uint32_t magic;            // 0x00: Magic number "XTC\0" (0x00435458)
  uint8_t versionMajor;      // 0x04: Format version major (2)
  uint8_t versionMinor;      // 0x05: Format version minor (0)
  uint16_t pageCount;        // 0x06: Total page count
  uint8_t readDirection;     // 0x08: Reading direction (0=L→R, 1=R→L)
  uint8_t hasChapters;       // 0x09: Has chapters (0 or 1)
  uint16_t pageWidth;        // 0x0A: Page width in pixels
  uint16_t pageHeight;       // 0x0C: Page height in pixels
  uint16_t reserved;         // 0x0E: Reserved for future use
  uint32_t indexOffset;      // 0x10: Index table offset
  uint32_t dataOffset;       // 0x14: Data area offset
  uint32_t chapterOffset;    // 0x18: Chapter data offset
};
#pragma pack(pop)

// Page table entry (13 bytes)
#pragma pack(push, 1)
struct PageTableEntry {
  uint64_t dataOffset;  // 0x00: Absolute offset to page data (8 bytes)
  uint32_t dataSize;    // 0x08: Page data size in bytes (4 bytes)
  uint8_t headerLevel;  // 0x0C: Header level (0 = no header, 1-6 = H1-H6) (1 byte)
};
#pragma pack(pop)

// XTG/XTH page data header (14 bytes)
// Used for both 1-bit (XTG) and 2-bit (XTH) formats
#pragma pack(push, 1)
struct XtgPageHeader {
  uint32_t magic;       // 0x00: File identifier
  uint16_t width;       // 0x04: Image width (pixels)
  uint16_t height;      // 0x06: Image height (pixels)
  uint8_t compression;  // 0x08: Compression (0=uncompressed)
  uint8_t reserved;     // 0x09: Reserved for future use
  uint32_t dataSize;    // 0x0A: Image data size (bytes)
};
#pragma pack(pop)

// Page information (internal use, optimized for memory)
struct PageInfo {
  uint32_t offset;      // File offset to page data (max 4GB file size)
  uint32_t size;        // Data size (bytes)
  uint8_t bitDepth;     // 1 = XTG (1-bit), 2 = XTH (2-bit grayscale)
  uint8_t headerLevel;  // 0 = no header, 1-6 = H1-H6
};  // 10 bytes total

struct ChapterInfo {
  std::string name;
  uint16_t startPage;
  uint16_t endPage;
};

// Error codes
enum class XtcError {
  OK = 0,
  FILE_NOT_FOUND,
  INVALID_MAGIC,
  INVALID_VERSION,
  CORRUPTED_HEADER,
  PAGE_OUT_OF_RANGE,
  READ_ERROR,
  WRITE_ERROR,
  MEMORY_ERROR,
  DECOMPRESSION_ERROR,
};

// Convert error code to string
inline const char* errorToString(XtcError err) {
  switch (err) {
    case XtcError::OK:
      return "OK";
    case XtcError::FILE_NOT_FOUND:
      return "File not found";
    case XtcError::INVALID_MAGIC:
      return "Invalid magic number";
    case XtcError::INVALID_VERSION:
      return "Unsupported version";
    case XtcError::CORRUPTED_HEADER:
      return "Corrupted header";
    case XtcError::PAGE_OUT_OF_RANGE:
      return "Page out of range";
    case XtcError::READ_ERROR:
      return "Read error";
    case XtcError::WRITE_ERROR:
      return "Write error";
    case XtcError::MEMORY_ERROR:
      return "Memory allocation error";
    case XtcError::DECOMPRESSION_ERROR:
      return "Decompression error";
    default:
      return "Unknown error";
  }
}

//Check if filename has XTC/XTCH extension
inline bool isXtcExtension(const char* filename) {
  if (!filename) return false;
  const char* ext = strrchr(filename, '.');
  if (!ext) return false;
  return (strcasecmp(ext, ".xtc") == 0 || strcasecmp(ext, ".xtch") == 0);
}

}  // namespace xtc