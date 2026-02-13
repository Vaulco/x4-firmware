#include "XtcParser.h"

#include <HardwareSerial.h>
#include <SDCardManager.h>

#include <cstring>

namespace xtc {

XtcParser::XtcParser()
    : m_isOpen(false),
      m_defaultWidth(0),
      m_defaultHeight(0),
      m_bitDepth(1),
      m_hasChapters(false),
      m_lastError(XtcError::OK) {
  memset(&m_header, 0, sizeof(m_header));
}

XtcParser::~XtcParser() { close(); }

XtcError XtcParser::open(const char* filepath) {
  // Close if already open
  if (m_isOpen) {
    close();
  }

  // Open file
  if (!SdMan.openFileForRead("XTC", filepath, m_file)) {
    m_lastError = XtcError::FILE_NOT_FOUND;
    return m_lastError;
  }

  // Read header
  m_lastError = readHeader();
  if (m_lastError != XtcError::OK) {
    Serial.printf("[%lu] [XTC] Failed to read header: %s\n", millis(), errorToString(m_lastError));
    m_file.close();
    return m_lastError;
  }

  // Read title if available
  readTitle();

  // Read page table (with format auto-detection)
  m_lastError = readPageTable();
  if (m_lastError != XtcError::OK) {
    Serial.printf("[%lu] [XTC] Failed to read page table: %s\n", millis(), errorToString(m_lastError));
    m_file.close();
    return m_lastError;
  }

  // Read chapters if present
  m_lastError = readChapters();
  if (m_lastError != XtcError::OK) {
    Serial.printf("[%lu] [XTC] Failed to read chapters: %s\n", millis(), errorToString(m_lastError));
    m_file.close();
    return m_lastError;
  }

  m_isOpen = true;
  return XtcError::OK;
}

void XtcParser::close() {
  if (m_isOpen) {
    m_file.close();
    m_isOpen = false;
  }
  m_pageTable.clear();
  m_chapters.clear();
  m_title.clear();
  m_hasChapters = false;
  memset(&m_header, 0, sizeof(m_header));
}

XtcError XtcParser::readHeader() {
  // Read 24 bytes of header
  size_t bytesRead = m_file.read(reinterpret_cast<uint8_t*>(&m_header), sizeof(XtcHeader));
  if (bytesRead != sizeof(XtcHeader)) {
    return XtcError::READ_ERROR;
  }

  // Verify magic number (accept both XTC and XTCH)
  if (m_header.magic != XTC_MAGIC && m_header.magic != XTCH_MAGIC) {
    Serial.printf("[%lu] [XTC] Invalid magic: 0x%08X (expected 0x%08X or 0x%08X)\n", millis(), m_header.magic,
                  XTC_MAGIC, XTCH_MAGIC);
    return XtcError::INVALID_MAGIC;
  }

  // Determine bit depth from file magic
  m_bitDepth = (m_header.magic == XTCH_MAGIC) ? 2 : 1;

  // Check version
  // Currently, version 1.0 is the only valid version, however some generators are swapping the bytes around, so we
  // accept both 1.0 and 0.1 for compatibility
  const bool validVersion = m_header.versionMajor == 1 && m_header.versionMinor == 0 ||
                            m_header.versionMajor == 0 && m_header.versionMinor == 1;
  if (!validVersion) {
    Serial.printf("[%lu] [XTC] Unsupported version: %u.%u\n", millis(), m_header.versionMajor, m_header.versionMinor);
    return XtcError::INVALID_VERSION;
  }

  // Basic validation
  if (m_header.pageCount == 0) {
    return XtcError::CORRUPTED_HEADER;
  }

  Serial.printf("[%lu] [XTC] Header: magic=0x%08X (%s), ver=%u.%u, pages=%u, bitDepth=%u\n", millis(), m_header.magic,
                (m_header.magic == XTCH_MAGIC) ? "XTCH" : "XTC", m_header.versionMajor, m_header.versionMinor,
                m_header.pageCount, m_bitDepth);

  return XtcError::OK;
}

XtcError XtcParser::readTitle() {
  // Title is usually at offset 0x18 (24) for 24-byte headers
  // Read title as null-terminated UTF-8 string
  const uint64_t titleOffset = 0x18;  // Default offset after 24-byte header

  if (!m_file.seek(titleOffset)) {
    return XtcError::READ_ERROR;
  }

  char titleBuf[128] = {0};
  m_file.read(reinterpret_cast<uint8_t*>(titleBuf), sizeof(titleBuf) - 1);
  m_title = titleBuf;

  Serial.printf("[%lu] [XTC] Title: %s\n", millis(), m_title.c_str());
  return XtcError::OK;
}

XtcError XtcParser::readPageTable() {
  if (m_header.indexOffset == 0) {
    Serial.printf("[%lu] [XTC] Page table offset is 0, cannot read\n", millis());
    return XtcError::CORRUPTED_HEADER;
  }

  // Seek to page table
  if (!m_file.seek(m_header.indexOffset)) {
    Serial.printf("[%lu] [XTC] Failed to seek to page table at %u\n", millis(), m_header.indexOffset);
    return XtcError::READ_ERROR;
  }

  // Validate page table size
  const uint32_t availableBytes = m_header.dataOffset - m_header.indexOffset;
  const uint32_t expectedTableSize = m_header.pageCount * sizeof(PageTableEntry);  // 18 bytes per entry

  if (availableBytes < expectedTableSize) {
    Serial.printf("[%lu] [XTC] Page table size mismatch: available=%u, expected=%u\n",
                  millis(), availableBytes, expectedTableSize);
    return XtcError::CORRUPTED_HEADER;
  }

  m_pageTable.resize(m_header.pageCount);

  // Read modern format (18 bytes per entry, includes header level)
  for (uint16_t i = 0; i < m_header.pageCount; i++) {
    PageTableEntry entry;
    size_t bytesRead = m_file.read(reinterpret_cast<uint8_t*>(&entry), sizeof(PageTableEntry));
    if (bytesRead != sizeof(PageTableEntry)) {
      Serial.printf("[%lu] [XTC] Failed to read page table entry %u\n", millis(), i);
      return XtcError::READ_ERROR;
    }

    m_pageTable[i].offset = static_cast<uint32_t>(entry.dataOffset);
    m_pageTable[i].size = entry.dataSize;
    m_pageTable[i].width = entry.width;
    m_pageTable[i].height = entry.height;
    m_pageTable[i].bitDepth = m_bitDepth;
    m_pageTable[i].headerLevel = entry.headerLevel;

    // Update default dimensions from first page
    if (i == 0) {
      m_defaultWidth = entry.width;
      m_defaultHeight = entry.height;
    }
  }

  Serial.printf("[%lu] [XTC] Read %u page table entries\n", millis(), m_header.pageCount);
  return XtcError::OK;
}

XtcError XtcParser::readChapters() {
  m_hasChapters = false;
  m_chapters.clear();

  if (m_header.hasChapters != 1) {
    return XtcError::OK;
  }

  if (m_header.chapterOffset == 0) {
    return XtcError::OK;
  }

  const uint64_t fileSize = m_file.size();
  if (m_header.chapterOffset < sizeof(XtcHeader) || m_header.chapterOffset >= fileSize || 
      m_header.chapterOffset + 96 > fileSize) {
    return XtcError::OK;
  }

  uint32_t maxOffset = 0;
  if (m_header.indexOffset > m_header.chapterOffset) {
    maxOffset = m_header.indexOffset;
  } else if (m_header.dataOffset > m_header.chapterOffset) {
    maxOffset = m_header.dataOffset;
  } else {
    maxOffset = fileSize;
  }

  if (maxOffset <= m_header.chapterOffset) {
    return XtcError::OK;
  }

  constexpr size_t chapterSize = 96;
  const uint32_t available = maxOffset - m_header.chapterOffset;
  const size_t chapterCount = static_cast<size_t>(available / chapterSize);
  if (chapterCount == 0) {
    return XtcError::OK;
  }

  if (!m_file.seek(m_header.chapterOffset)) {
    return XtcError::READ_ERROR;
  }

  std::vector<uint8_t> chapterBuf(chapterSize);
  for (size_t i = 0; i < chapterCount; i++) {
    if (m_file.read(chapterBuf.data(), chapterSize) != chapterSize) {
      return XtcError::READ_ERROR;
    }

    char nameBuf[81];
    memcpy(nameBuf, chapterBuf.data(), 80);
    nameBuf[80] = '\0';
    const size_t nameLen = strnlen(nameBuf, 80);
    std::string name(nameBuf, nameLen);

    uint16_t startPage = 0;
    uint16_t endPage = 0;
    memcpy(&startPage, chapterBuf.data() + 0x50, sizeof(startPage));
    memcpy(&endPage, chapterBuf.data() + 0x52, sizeof(endPage));

    if (name.empty() && startPage == 0 && endPage == 0) {
      break;
    }

    if (startPage > 0) {
      startPage--;
    }
    if (endPage > 0) {
      endPage--;
    }

    if (startPage >= m_header.pageCount) {
      continue;
    }

    if (endPage >= m_header.pageCount) {
      endPage = m_header.pageCount - 1;
    }

    if (startPage > endPage) {
      continue;
    }

    ChapterInfo chapter{std::move(name), startPage, endPage};
    m_chapters.push_back(std::move(chapter));
  }

  m_hasChapters = !m_chapters.empty();
  Serial.printf("[%lu] [XTC] Chapters: %u\n", millis(), static_cast<unsigned int>(m_chapters.size()));
  return XtcError::OK;
}

bool XtcParser::getPageInfo(uint32_t pageIndex, PageInfo& info) const {
  if (pageIndex >= m_pageTable.size()) {
    return false;
  }
  info = m_pageTable[pageIndex];
  return true;
}

size_t XtcParser::loadPage(uint32_t pageIndex, uint8_t* buffer, size_t bufferSize) const {
  if (!m_isOpen) {
    m_lastError = XtcError::FILE_NOT_FOUND;
    return 0;
  }

  if (pageIndex >= m_header.pageCount) {
    m_lastError = XtcError::PAGE_OUT_OF_RANGE;
    return 0;
  }

  const PageInfo& page = m_pageTable[pageIndex];

  // Seek to page data
  if (!m_file.seek(page.offset)) {
    Serial.printf("[%lu] [XTC] Failed to seek to page %u at offset %lu\n", millis(), pageIndex, page.offset);
    m_lastError = XtcError::READ_ERROR;
    return 0;
  }

  // Read page header (XTG for 1-bit, XTH for 2-bit - same structure)
  XtgPageHeader pageHeader;
  size_t headerRead = m_file.read(reinterpret_cast<uint8_t*>(&pageHeader), sizeof(XtgPageHeader));
  if (headerRead != sizeof(XtgPageHeader)) {
    Serial.printf("[%lu] [XTC] Failed to read page header for page %u\n", millis(), pageIndex);
    m_lastError = XtcError::READ_ERROR;
    return 0;
  }

  // Verify page magic (XTG for 1-bit, XTH for 2-bit)
  const uint32_t expectedMagic = (m_bitDepth == 2) ? XTH_MAGIC : XTG_MAGIC;
  if (pageHeader.magic != expectedMagic) {
    Serial.printf("[%lu] [XTC] Invalid page magic for page %u: 0x%08X (expected 0x%08X)\n", millis(), pageIndex,
                  pageHeader.magic, expectedMagic);
    m_lastError = XtcError::INVALID_MAGIC;
    return 0;
  }

  // Calculate bitmap size based on bit depth
  // XTG (1-bit): Row-major, ((width+7)/8) * height bytes
  // XTH (2-bit): Two bit planes, column-major, ((width * height + 7) / 8) * 2 bytes
  size_t bitmapSize;
  if (m_bitDepth == 2) {
    // XTH: two bit planes, each containing (width * height) bits rounded up to bytes
    bitmapSize = ((static_cast<size_t>(pageHeader.width) * pageHeader.height + 7) / 8) * 2;
  } else {
    bitmapSize = ((pageHeader.width + 7) / 8) * pageHeader.height;
  }

  // Check buffer size
  if (bufferSize < bitmapSize) {
    Serial.printf("[%lu] [XTC] Buffer too small: need %u, have %u\n", millis(), bitmapSize, bufferSize);
    m_lastError = XtcError::MEMORY_ERROR;
    return 0;
  }

  // Read bitmap data
  size_t bytesRead = m_file.read(buffer, bitmapSize);
  if (bytesRead != bitmapSize) {
    Serial.printf("[%lu] [XTC] Page read error: expected %u, got %u\n", millis(), bitmapSize, bytesRead);
    m_lastError = XtcError::READ_ERROR;
    return 0;
  }

  m_lastError = XtcError::OK;
  return bytesRead;
}

}  // namespace xtc