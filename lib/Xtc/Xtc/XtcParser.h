#pragma once

#include <SdFat.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "XtcTypes.h"

namespace xtc {

class XtcParser {
 public:
  XtcParser();
  ~XtcParser();

  // File open/close
  XtcError open(const char* filepath);
  void close();
  bool isOpen() const { return m_isOpen; }

  // Header information access
  uint16_t getPageCount() const { return m_header.pageCount; }
  uint16_t getWidth() const { return m_header.pageWidth; }
  uint16_t getHeight() const { return m_header.pageHeight; }
  uint8_t getBitDepth() const { return m_bitDepth; }  // 1 = XTC/XTG, 2 = XTCH/XTH

  // Page information
  bool getPageInfo(uint32_t pageIndex, PageInfo& info) const;

  /**
   * Load page bitmap (raw 1-bit data, skipping XTG header)
   *
   * @param pageIndex Page index (0-based)
   * @param buffer Output buffer (caller allocated)
   * @param bufferSize Buffer size
   * @return Number of bytes read on success, 0 on failure
   */
  size_t loadPage(uint32_t pageIndex, uint8_t* buffer, size_t bufferSize) const;

  bool hasChapters() const { return m_hasChapters; }
  const std::vector<ChapterInfo>& getChapters() const { return m_chapters; }

 private:
  mutable FsFile m_file;
  bool m_isOpen;
  XtcHeader m_header;
  std::vector<PageInfo> m_pageTable;
  std::vector<ChapterInfo> m_chapters;
  uint8_t m_bitDepth;  // 1 = XTC/XTG (1-bit), 2 = XTCH/XTH (2-bit)
  bool m_hasChapters;
  mutable XtcError m_lastError;  // Mutable because error state can be updated during const operations

  // Internal helper functions
  XtcError readHeader();
  XtcError readPageTable();
  XtcError readChapters();
};

}  // namespace xtc