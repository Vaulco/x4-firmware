#include "ChapterSelectionActivity.h"

#include <GfxRenderer.h>
#include <Utf8.h>

namespace {

// Manual truncation helper - avoids renderer.truncatedText() UTF-8 bug
std::string truncateToWidth(const GfxRenderer& renderer, GfxRenderer::FontSize size, 
                            const std::string& text, int maxWidth) {
    if (text.empty()) {
        return text;
    }
    
    // Check if text fits as-is
    int fullWidth = renderer.getTextWidth(size, text.c_str());
    if (fullWidth <= maxWidth) {
        return text;
    }
    
    const char* ellipsis = "...";
    int ellipsisWidth = renderer.getTextWidth(size, ellipsis);
    int availableForText = maxWidth - ellipsisWidth;
    
    if (availableForText <= 0) {
        return ellipsis;
    }
    
    // Iterate through UTF-8 codepoints to find safe truncation point
    const unsigned char* str = reinterpret_cast<const unsigned char*>(text.c_str());
    const unsigned char* lastSafePos = str;
    std::string candidate;
    
    while (*str) {
        const unsigned char* nextPos = str;
        uint32_t cp = utf8NextCodepoint(&nextPos);
        
        if (cp == 0) break;  // Invalid or end of string
        
        // Build candidate string up to this codepoint
        candidate = std::string(text.c_str(), nextPos - reinterpret_cast<const unsigned char*>(text.c_str()));
        int candidateWidth = renderer.getTextWidth(size, candidate.c_str());
        
        if (candidateWidth > availableForText) {
            // This character doesn't fit, use last safe position
            break;
        }
        
        lastSafePos = nextPos;
        str = nextPos;
    }
    
    // Build final truncated string
    if (lastSafePos == reinterpret_cast<const unsigned char*>(text.c_str())) {
        return ellipsis;  // Not even one character fits
    }
    
    size_t safeLength = lastSafePos - reinterpret_cast<const unsigned char*>(text.c_str());
    return text.substr(0, safeLength) + ellipsis;
}

}  // namespace

int ChapterSelectionActivity::findChapterIndexForPage(uint32_t page) const {
  if (!xtc) {
    return 0;
  }

  const auto& chapters = xtc->getChapters();
  
  for (size_t i = 0; i < chapters.size(); i++) {
    if (page >= chapters[i].startPage && page <= chapters[i].endPage) {
      return static_cast<int>(i);
    }
  }
  return 0;
}

void ChapterSelectionActivity::renderItem(int index, int x, int y, bool isSelected) const {
  const auto& chapters = xtc->getChapters();
  if (index < 0 || index >= static_cast<int>(chapters.size())) {
    return;
  }

  const auto& chapter = chapters[index];
  const auto pageWidth = renderer.getScreenWidth();
  
  // Get page info to determine header level
  xtc::PageInfo pageInfo;
  const uint8_t headerLevel = xtc->getPageInfo(chapter.startPage, pageInfo) ? pageInfo.headerLevel : 0;
  
  // Calculate indentation based on header level (0 = no header, 1-6 = H1-H6)
  // H1: no indent, H2: 10px, H3: 20px, etc.
  const int indent = headerLevel > 1 ? (headerLevel - 1) * 10 : 0;
  const int leftMargin = x + indent;
  
  // Prepare page number (to calculate available space for title)
  char pageNum[16];
  snprintf(pageNum, sizeof(pageNum), "%u", chapter.startPage + 1);
  const int pageNumWidth = renderer.getTextWidth(GfxRenderer::MEDIUM, pageNum);
  
  // Calculate available width for chapter title (15px gap from page number for spacing)
  const int availableWidth = pageWidth - leftMargin - pageNumWidth - 15 - 20;
  
  // Truncate chapter title if necessary using our manual truncation helper
  std::string displayTitle = chapter.name.empty() ? "Unnamed" : chapter.name;
  displayTitle = truncateToWidth(renderer, GfxRenderer::MEDIUM, displayTitle, availableWidth);
  
  // Draw chapter title on the left with indentation
  renderer.drawText(GfxRenderer::MEDIUM, leftMargin, y, displayTitle.c_str(), !isSelected);
  
  // Draw page number on the right
  renderer.drawText(GfxRenderer::MEDIUM, pageWidth - 20 - pageNumWidth, y, pageNum, !isSelected);
}