import os
import sys
import struct
from dataclasses import dataclass
from typing import List, Tuple, Optional, Generator
import fitz
from PIL import Image, ImageDraw, ImageTk
import pyphen
import base64
from tkinter import filedialog, messagebox, Tk, Frame, Button, Label, Scrollbar, Canvas
import threading
import re
import markdown
from bs4 import BeautifulSoup, NavigableString

@dataclass
class RenderConfig:
    font_path: str = ''
    font_size: int = 30
    margin: int = 8
    line_height: float = 1.1
    font_weight: int = 600
    bottom_padding: int = 0
    top_padding: int = 20
    text_align: str = "left"
    screen_width: int = 480
    screen_height: int = 800
    heading_scale: float = 1.4
    heading_weight_delta: int = 200
    # Individual header level sizes (if None, use heading_scale)
    h1_size: Optional[int] = None
    h2_size: Optional[int] = None
    h3_size: Optional[int] = None
    h4_size: Optional[int] = None
    h5_size: Optional[int] = None
    h6_size: Optional[int] = None


@dataclass
class ChapterInfo:
    name: str
    start_page: int
    end_page: int

@dataclass
class PageInfo:
    """Information about a rendered page including header level."""
    doc_index: int
    page_index: int
    header_level: int = 0  # NEW: 0 = no header, 1-6 = H1-H6

def get_cmu_font_path() -> Optional[str]:
    """Get the path to the CMU font file if it exists."""
    base_path = os.path.dirname(sys.executable if getattr(sys, 'frozen', False) 
                                else os.path.abspath(__file__))
    # Fixed path - added 'CMU' subdirectory
    cmu_path = os.path.join(base_path, '../lib/EpdFont/builtinFonts/source/CMU/CMUSerif-Regular.ttf')
    if os.path.exists(cmu_path):
        return os.path.abspath(cmu_path)
    print(f'Warning: cmu.ttf not found at {cmu_path}')
    return None

def get_font_variants(font_path: str) -> dict:
    """Get font variant paths for different styles."""
    if not font_path or not os.path.exists(font_path):
        return {}
    regular_path = font_path.replace('\\', '/')
    return {k: regular_path for k in ['regular', 'italic', 'bold', 'bold_italic']}

def detect_header_level(soup: BeautifulSoup) -> int:
    """
    Detect the header level of a page's content.
    Returns 0 for no header, 1-6 for H1-H6.
    Uses the highest priority (lowest number) header found.
    """
    headers = soup.find_all(['h1', 'h2', 'h3', 'h4', 'h5', 'h6'])
    
    if not headers:
        return 0
    
    # Extract header levels and return the most important (lowest number)
    levels = [int(h.name[1]) for h in headers]
    return min(levels)

def hyphenate_html_text(soup: BeautifulSoup, language_code: str = 'en') -> BeautifulSoup:
    """Add soft hyphens to long words in HTML for better text wrapping."""
    try:
        dic = pyphen.Pyphen(lang=language_code)
    except Exception as e:
        print(f'Warning: Failed to load hyphenation for {language_code}: {e}')
        try:
            dic = pyphen.Pyphen(lang='en')
        except Exception as e2:
            print(f'Warning: Failed to load English hyphenation: {e2}')
            return soup
    
    word_pattern = re.compile(r'\w+', re.UNICODE)
    skip_tags = {'script', 'style', 'head', 'title', 'meta'}
    
    for text_node in soup.find_all(string=True):
        if text_node.parent.name in skip_tags or not text_node.strip():
            continue
        
        original_text = str(text_node)
        clean_text = original_text.replace('\xa0', ' ')
        
        def replace_match(match):
            word = match.group(0)
            return dic.inserted(word, hyphen='\xad') if len(word) >= 6 else word
        
        new_text = word_pattern.sub(replace_match, clean_text)
        if new_text != original_text:
            text_node.replace_with(NavigableString(new_text))
    
    return soup

class CSSGenerator:
    """Generates CSS styles for rendering HTML pages."""
    
    @staticmethod
    def generate_font_face_rules(variants: dict) -> str:
        """Generate @font-face rules for custom fonts."""
        if not variants:
            return ''
        
        rules = []
        font_configs = [
            ('regular', 'normal', 'normal'),
            ('italic', 'normal', 'italic'),
            ('bold', 'bold', 'normal'),
            ('bold_italic', 'bold', 'italic')
        ]
        
        for variant_key, weight, style in font_configs:
            if variants.get(variant_key):
                rules.append(
                    f'@font-face {{ font-family: "CustomFont"; '
                    f'src: url("{variants[variant_key]}"); '
                    f'font-weight: {weight}; font-style: {style}; }}'
                )
        
        return '\n'.join(rules)
    
    @staticmethod
    def generate_page_css(config: RenderConfig, use_custom_font: bool) -> str:
        """Generate complete CSS for page rendering based on configuration."""
        font_family = '"CustomFont"' if use_custom_font else 'serif'
        variants = get_font_variants(config.font_path) if use_custom_font else {}
        font_face_rule = CSSGenerator.generate_font_face_rules(variants)
        
        # Calculate header sizes - use individual sizes if specified, otherwise use heading_scale
        heading_weight = min(900, config.font_weight + config.heading_weight_delta)
        h1_size = config.h1_size if config.h1_size else config.font_size * config.heading_scale
        h2_size = config.h2_size if config.h2_size else config.font_size * config.heading_scale
        h3_size = config.h3_size if config.h3_size else config.font_size * config.heading_scale
        h4_size = config.h4_size if config.h4_size else config.font_size * config.heading_scale
        h5_size = config.h5_size if config.h5_size else config.font_size * config.heading_scale
        h6_size = config.h6_size if config.h6_size else config.font_size * config.heading_scale
        
        return f'''
                <style>
                    {font_face_rule}
                    @page {{ size: {config.screen_width}pt {config.screen_height}pt; margin: 0; }}
                    body {{
                        font-family: {font_family} !important;
                        font-size: {config.font_size}pt !important;
                        font-weight: {config.font_weight} !important;
                        line-height: {config.line_height} !important;
                        text-align: {config.text_align} !important;
                        color: black !important;
                        margin: 0 !important;
                        padding: 0 {config.margin}px !important;
                        background-color: white !important;
                        width: 100% !important; 
                        height: 100% !important;
                        overflow-wrap: break-word;
                    }}
                    * {{ margin-top: 0 !important; }}
                    p, div, li, blockquote, dd, dt {{
                        font-family: inherit !important;
                        font-size: inherit !important;
                        font-weight: inherit !important;
                        line-height: inherit !important;
                        text-align: {config.text_align} !important;
                        color: inherit !important;
                    }}
                    p {{ margin-bottom: 0.8em !important; }}
                    p + p {{ margin-top: 0.8em !important; }}
                    h1, h2, h3, h4, h5, h6 {{ margin-bottom: 0.5em !important; margin-top: 1em; text-align: center !important; font-weight: {heading_weight} !important; }}
                    h1 + *, h2 + *, h3 + *, h4 + *, h5 + *, h6 + * {{ margin-top: 0.5em !important; }}
                    * + h1, * + h2, * + h3, * + h4, * + h5, * + h6 {{ margin-top: 0.5yem !important; }}
                    li + li {{ margin-top: 0.3em !important; }}
                    span {{
                        font-family: {font_family} !important;
                        font-size: inherit !important;
                        line-height: inherit !important;
                        color: inherit !important;
                    }}
                    img {{width: 100% !important;  max-width: none !important; height: auto !important; display: block; margin-bottom: 8px; }}
                    h1 {{ font-size: {h1_size}pt !important; }}
                    h2 {{ font-size: {h2_size}pt !important; }}
                    h3 {{ font-size: {h3_size}pt !important; }}
                    h4 {{ font-size: {h4_size}pt !important; }}
                    h5 {{ font-size: {h5_size}pt !important; }}
                    h6 {{ font-size: {h6_size}pt !important; }}
                    code {{ font-family: monospace !important; background-color: #f0f0f0; padding: 2px 4px; }}
                    pre {{ background-color: #f0f0f0; padding: 10px; overflow-x: auto; }}
                    blockquote {{ border-left: 4px solid #ccc; padding-left: 15px; margin: 15px 0; }}
                    table {{ border-collapse: collapse; width: 100%; margin: 15px 0; }}
                    th, td {{ border: 1px solid black; padding: 8px; text-align: left; }}
                    th {{ background-color: #f0f0f0; font-weight: bold; }}
                    ul, ol {{ margin-left: 8px !important; }}
                </style>
                '''

class XTCWriter:
    """Writes XTC (eReader format) files with chapter metadata and page images."""
    
    HEADER_SIZE = 56
    CHAPTER_ENTRY_SIZE = 96
    INDEX_ENTRY_SIZE = 18  # UPDATED: was 16, now 18 to include header_level
    XTG_MAGIC = 4674648
    XTC_MAGIC = 0x00435458
    
    @staticmethod
    def write_xtc(file_path: str, page_generator: Generator[Tuple[Image.Image, int], None, None], 
                  total_pages: int, chapters: List[ChapterInfo]) -> None:
        """Write an XTC file with streaming page generation for memory efficiency."""
        chapter_count = len(chapters)
        has_chapters = 1 if chapter_count > 0 else 0
        
        current_offset = XTCWriter.HEADER_SIZE
        chapter_offset = current_offset if has_chapters else 0
        
        if has_chapters:
            current_offset += chapter_count * XTCWriter.CHAPTER_ENTRY_SIZE
        
        index_offset = current_offset
        data_offset = current_offset + total_pages * XTCWriter.INDEX_ENTRY_SIZE
        
        with open(file_path, 'wb') as f:
            XTCWriter._write_header(f, total_pages, has_chapters, index_offset, 
                                   data_offset, chapter_offset)
            
            if has_chapters:
                XTCWriter._write_chapters(f, chapters)
            
            XTCWriter._write_pages_streaming(f, page_generator, total_pages, data_offset)
    
    @staticmethod
    def _write_pages_streaming(f, page_generator: Generator[Tuple[Image.Image, int], None, None], 
                              total_pages: int, start_offset: int) -> None:
        """Write pages to XTC file using streaming to minimize memory usage."""
        index_position = f.tell()
        f.seek(start_offset)
        
        idx_accumulator = []
        current_offset = start_offset
        
        # Serial processing to avoid UI freezes and maintain memory efficiency
        for page_img, header_level in page_generator:
            page_blob = XTCWriter._encode_page(page_img)
            blob_size = len(page_blob)
            w = struct.unpack('<H', page_blob[4:6])[0]
            h = struct.unpack('<H', page_blob[6:8])[0]
            
            # NEW: Include header_level (1 byte) and reserved (1 byte) in index entry
            idx_entry = struct.pack('<QIHHBB', current_offset, blob_size, w, h, 
                                   header_level, 0)
            idx_accumulator.append(idx_entry)
            
            f.write(page_blob)
            current_offset += blob_size
        
        # Write index entries
        f.seek(index_position)
        for idx_chunk in idx_accumulator:
            f.write(idx_chunk)
    
    @staticmethod
    def _encode_page(img_rgb: Image.Image) -> bytes:
        """Encode a page image to XTG format (1-bit black and white)."""
        img_final = img_rgb.convert('1')
        w, h = img_final.size
        img_bytes = img_final.tobytes()
        xtg_header = struct.pack('<IHHBBIQ', XTCWriter.XTG_MAGIC, w, h, 
                                0, 0, (w + 7) // 8 * h, 0)
        return xtg_header + img_bytes
    
    @staticmethod
    def _write_header(f, page_count: int, has_chapters: int, index_offset: int,
                     data_offset: int, chapter_offset: int) -> None:
        """Write XTC file header."""
        header = struct.pack(
            '<IHHBBBBIQQQQQ',
            XTCWriter.XTC_MAGIC,
            0x0100,
            page_count,
            0,
            0,
            0,
            has_chapters,
            0,
            0,
            index_offset,
            data_offset,
            0,
            chapter_offset
        )
        f.write(header)
    
    @staticmethod
    def _write_chapters(f, chapters: List[ChapterInfo]) -> None:
        """Write chapter metadata to XTC file."""
        for chapter in chapters:
            chapter_name = chapter.name.encode('utf-8')[:79] + b'\x00'
            chapter_name = chapter_name.ljust(80, b'\x00')
            
            chapter_entry = (
                chapter_name +
                struct.pack('<H', chapter.start_page) +
                struct.pack('<H', chapter.end_page) +
                b'\x00' * 12
            )
            f.write(chapter_entry)

class MarkdownProcessor:
    """Processes Markdown files and renders them as paginated images."""

    def _disable_ordered_lists(self, md: str) -> str:
        return re.sub(r'^(\s*\d+\.)\s', r'\1' + '\u200B' + ' ', md, flags=re.MULTILINE)
    
    def __init__(self):
        self.input_file: str = ''
        self.raw_chapters: List[dict] = []
        self.config: RenderConfig = RenderConfig(font_path=get_cmu_font_path() or '')
        self.fitz_docs: List[fitz.Document] = []
        self.page_map: List[PageInfo] = []  # UPDATED: Now stores PageInfo objects
        self.total_pages: int = 0
        self.is_ready: bool = False
        self.chapter_info: List[ChapterInfo] = []
    
    def parse_book_structure(self, input_path: str) -> bool:
        """Parse Markdown file and split into chapters based on headings."""
        self.input_file = input_path
        self.raw_chapters = []
        
        try:
            with open(self.input_file, 'r', encoding='utf-8') as f:
                md_content = f.read()
            md_content = self._disable_ordered_lists(md_content)
        except Exception as e:
            print(f'Error reading Markdown file: {e}')
            return False
        
        self.raw_chapters = self._split_markdown_by_headings(md_content)
        self._embed_local_images()
        return True
    
    def _split_markdown_by_headings(self, md_content: str) -> List[dict]:
        """Split Markdown content into chapters based on heading elements."""
        # Convert footnotes to inline format before processing
        md_content = self._convert_footnotes_inline(md_content)
        
        # Preprocess: Add two spaces before single newlines to force line breaks
        # This makes single newlines render as <br> tags
        lines = md_content.split('\n')
        processed_lines = []
        for i, line in enumerate(lines):
            # Don't add spaces after empty lines (paragraph breaks should stay as is)
            if line.strip() and i < len(lines) - 1:
                # Check if next line is not empty (to preserve paragraph breaks)
                if lines[i + 1].strip():
                    processed_lines.append(line + '  ')  # Add two spaces
                else:
                    processed_lines.append(line)
            else:
                processed_lines.append(line)
        md_content = '\n'.join(processed_lines)
        
        chapters = []
        html = markdown.markdown(md_content, extensions=['extra', 'codehilite', 'tables', 'nl2br'])
        soup = BeautifulSoup(html, 'html.parser')
        headings = soup.find_all(['h1', 'h2', 'h3', 'h4', 'h5', 'h6'])
        
        if not headings:
            return [{'title': 'Document', 'soup': soup}]
        
        all_elements = list(soup.body.children if soup.body else soup.children)
        # Precompute element indices for O(1) lookup instead of O(n) scanning
        element_to_index = {id(elem): idx for idx, elem in enumerate(all_elements)}
        
        first_heading_pos = element_to_index.get(id(headings[0]))
        
        if first_heading_pos and first_heading_pos > 0:
            intro_soup = self._extract_elements(all_elements, 0, first_heading_pos)
            if intro_soup.body.get_text(strip=True):
                chapters.append({'title': 'First Page', 'soup': intro_soup})
        
        # Group consecutive headers together
        i = 0
        while i < len(headings):
            heading = headings[i]
            chapter_title = heading.get_text().strip()
            heading_pos = element_to_index.get(id(heading))
            
            if heading_pos is None:
                i += 1
                continue
            
            # Look ahead to see if the next heading is consecutive (only whitespace between)
            next_header_idx = i + 1
            while next_header_idx < len(headings):
                next_heading = headings[next_header_idx]
                next_heading_pos = element_to_index.get(id(next_heading))
                
                if next_heading_pos is None:
                    break
                
                # Check if there's only whitespace between current and next heading
                has_content_between = False
                for k in range(heading_pos + 1, next_heading_pos):
                    elem = all_elements[k]
                    if hasattr(elem, 'get_text'):
                        if elem.get_text(strip=True):
                            has_content_between = True
                            break
                    elif str(elem).strip():
                        has_content_between = True
                        break
                
                if has_content_between:
                    # There's content between headings, stop grouping
                    break
                
                # No content between, include this heading in the group
                heading_pos = next_heading_pos
                heading = next_heading
                next_header_idx += 1
            
            # Now create chapter from heading_pos (first in group) to next non-grouped heading
            if next_header_idx < len(headings):
                next_heading_pos = element_to_index.get(id(headings[next_header_idx]))
            else:
                next_heading_pos = len(all_elements)
            
            chapter_soup = self._extract_elements(all_elements, 
                                                  element_to_index.get(id(headings[i])), 
                                                  next_heading_pos)
            chapters.append({'title': chapter_title, 'soup': chapter_soup})
            
            # Move to next ungrouped heading
            i = next_header_idx
        
        return chapters
    
    def _convert_footnotes_inline(self, content: str) -> str:
        """Convert markdown footnotes to inline [footnote text] format."""
        # Extract all footnote definitions
        footnote_pattern = r'^\[\^(\w+)\]:\s*(.+?)(?=\n\[\^|\n#|\Z)'
        footnotes = {}
        
        for match in re.finditer(footnote_pattern, content, re.MULTILINE | re.DOTALL):
            footnote_id = match.group(1)
            footnote_text = match.group(2).strip()
            # Remove newlines and extra whitespace from footnote text
            footnote_text = re.sub(r'\s+', ' ', footnote_text)
            footnotes[footnote_id] = footnote_text
        
        # Remove footnote definitions from content
        content = re.sub(footnote_pattern, '', content, flags=re.MULTILINE | re.DOTALL)
        
        # Replace footnote references with inline text
        def replace_footnote_ref(match):
            footnote_id = match.group(1)
            if footnote_id in footnotes:
                return f' [{footnotes[footnote_id]}]'
            return match.group(0)  # Keep original if no definition found
        
        content = re.sub(r'\[\^(\w+)\]', replace_footnote_ref, content)
        
        return content
    
    @staticmethod
    def _find_next_heading_pos(element_to_index: dict, headings: list, current_idx: int) -> int:
        """Find the position of the next heading after current_idx (O(k) instead of O(n*k))."""
        for j in range(current_idx + 1, len(headings)):
            pos = element_to_index.get(id(headings[j]))
            if pos is not None:
                return pos
        return max(element_to_index.values()) + 1
    
    @staticmethod
    def _extract_elements(all_elements: list, start: int, end: int) -> BeautifulSoup:
        """Extract a range of elements and return as a new BeautifulSoup object."""
        soup = BeautifulSoup('<body></body>', 'html.parser')
        for idx in range(start, end):
            elem = all_elements[idx]
            if hasattr(elem, 'name') and elem.name:
                soup.body.append(elem.__copy__())
            elif str(elem).strip():
                soup.body.append(NavigableString(str(elem)))
        return soup
    
    def _embed_local_images(self) -> None:
        """Convert local image file references to base64 data URIs."""
        md_dir = os.path.dirname(self.input_file)
        mime_types = {
            '.png': 'image/png', '.jpg': 'image/jpeg', '.jpeg': 'image/jpeg',
            '.gif': 'image/gif', '.bmp': 'image/bmp'
        }
        
        for chapter in self.raw_chapters:
            for img_tag in chapter['soup'].find_all('img'):
                src = img_tag.get('src', '')
                if not src or src.startswith(('http://', 'https://', 'data:')):
                    continue
                
                abs_path = os.path.join(md_dir, src)
                if os.path.exists(abs_path):
                    try:
                        with open(abs_path, 'rb') as img_file:
                            img_data = base64.b64encode(img_file.read()).decode('utf-8')
                            ext = os.path.splitext(abs_path)[1].lower()
                            mime_type = mime_types.get(ext, 'image/png')
                            img_tag['src'] = f'data:{mime_type};base64,{img_data}'
                    except Exception as e:
                        print(f'Warning: Failed to embed image {abs_path}: {e}')
    
    def render_chapters(self, selected_indices: List[int], config: RenderConfig) -> bool:
        """Render selected chapters as paginated fitz documents."""
        self.config = config
        
        # Clean up any existing documents
        for doc in self.fitz_docs:
            doc.close()
        
        self.fitz_docs, self.page_map, self.chapter_info = [], [], []
        
        custom_css = CSSGenerator.generate_page_css(config, bool(config.font_path))
        selected_set = set(selected_indices)
        running_page_count = 0
        
        for idx, chapter in enumerate(self.raw_chapters):
            soup = hyphenate_html_text(chapter['soup'], 'en')
            
            # NEW: Detect header level for this chapter
            header_level = detect_header_level(soup)
            
            body_content = ''.join([str(x) for x in soup.body.contents]) if soup.body else str(soup)
            final_html = f"<html lang='en'><head>{custom_css}</head><body>{body_content}</body></html>"
            
            # Render HTML directly in memory (no temp files)
            try:
                doc = fitz.open("html-", final_html.encode('utf-8'))
                rect = fitz.Rect(0, 0, config.screen_width, config.screen_height)
                doc.layout(rect=rect)
                self.fitz_docs.append(doc)
            except Exception as e:
                print(f'Error rendering chapter {idx} ({chapter["title"]}): {e}')
                return False
            
            chapter_page_count = len(doc)
            # NEW: Store PageInfo objects with header level
            for i in range(chapter_page_count):
                page_info = PageInfo(
                    doc_index=len(self.fitz_docs) - 1,
                    page_index=i,
                    header_level=header_level if i == 0 else 0  # Only first page gets header level
                )
                self.page_map.append(page_info)
            
            if idx in selected_set:
                self.chapter_info.append(ChapterInfo(
                    name=chapter['title'],
                    start_page=running_page_count + 1,
                    end_page=running_page_count + chapter_page_count
                ))
            
            running_page_count += chapter_page_count
        
        self.total_pages = len(self.page_map)
        self.is_ready = True
        return True
    
    def render_page(self, global_page_index: int) -> Optional[Tuple[Image.Image, int]]:
        """Render a single page as an Image with its header level."""
        if not self.is_ready:
            return None
        
        content_height = max(1, self.config.screen_height - 
                           self.config.bottom_padding - self.config.top_padding)
        
        page_info = self.page_map[global_page_index]
        page = self.fitz_docs[page_info.doc_index][page_info.page_index]
        
        sx = self.config.screen_width / page.rect.width
        sy = content_height / page.rect.height
        mat = fitz.Matrix(sx, sy)
        pix = page.get_pixmap(matrix=mat, alpha=False)
        
        img_content = Image.frombytes('RGB', [pix.width, pix.height], 
                                     pix.samples).convert('L')
        
        full_page = Image.new('L', (self.config.screen_width, self.config.screen_height), 255)
        paste_y = self.config.top_padding
        paste_x = (self.config.screen_width - img_content.width) // 2
        full_page.paste(img_content, (paste_x, paste_y))
        
        img_final = full_page.convert('RGB')
        draw = ImageDraw.Draw(img_final)
        
        if self.config.top_padding > 0:
            draw.rectangle([0, 0, self.config.screen_width, self.config.top_padding], 
                         fill=(255, 255, 255))
        if self.config.bottom_padding > 0:
            draw.rectangle([0, self.config.screen_height - self.config.bottom_padding, 
                          self.config.screen_width, self.config.screen_height], 
                         fill=(255, 255, 255))
        
        return (img_final, page_info.header_level)
    
    def _page_generator(self) -> Generator[Tuple[Image.Image, int], None, None]:
        """Generate pages one at a time for memory-efficient streaming."""
        for i in range(self.total_pages):
            yield self.render_page(i)
    
    def save_xtc(self, out_name: str) -> None:
        """Save rendered pages as an XTC file."""
        if not self.is_ready:
            return
        
        XTCWriter.write_xtc(out_name, self._page_generator(), self.total_pages, self.chapter_info)
        
        # Close fitz documents after export to free memory
        for doc in self.fitz_docs:
            doc.close()
        self.fitz_docs = []
        self.is_ready = False

class App(Tk):
    """Main application window for Markdown to XTC conversion."""
    
    def __init__(self):
        super().__init__()
        self.processor = MarkdownProcessor()
        self.current_page_index: int = 0
        self.debounce_timer: Optional[threading.Timer] = None
        self.is_processing: bool = False
        self.selected_chapter_indices: List[int] = []
        
        self.title('MD2XTC - Markdown to XTC Converter (with Header Level Support)')
        self.geometry('1200x800')
        
        self._build_ui()
        self._bind_keys()
    
    def _build_ui(self) -> None:
        """Build the user interface."""
        top_frame = Frame(self)
        top_frame.pack(side='top', fill='x', padx=10, pady=10)
        
        self.btn_select = Button(top_frame, text='Select Markdown', command=self.select_file)
        self.btn_select.pack(side='left', padx=5)
        
        self.lbl_file = Label(top_frame, text='No file selected', fg='gray')
        self.lbl_file.pack(side='left', padx=5)
        
        self.btn_export = Button(top_frame, text='Export XTC', state='disabled', 
                                command=self.export_file)
        self.btn_export.pack(side='left', padx=5)
        
        self.canvas_frame = Frame(self)
        self.canvas_frame.pack(side='top', fill='both', expand=True)
        
        self.canvas = Canvas(self.canvas_frame)
        self.scrollbar = Scrollbar(self.canvas_frame, orient='vertical', 
                                   command=self.canvas.yview)
        self.scrollable_frame = Frame(self.canvas)
        
        self.scrollable_frame.bind(
            "<Configure>",
            lambda e: self.canvas.configure(scrollregion=self.canvas.bbox("all"))
        )
        
        self.canvas.create_window((0, 0), window=self.scrollable_frame, anchor='nw')
        self.canvas.configure(yscrollcommand=self.scrollbar.set)
        
        self.canvas.pack(side='left', fill='both', expand=True)
        self.scrollbar.pack(side='right', fill='y')
        
        self.img_label = Label(self.scrollable_frame, text='')
        self.img_label.pack(padx=10, pady=10)
        
        nav_frame = Frame(self)
        nav_frame.pack(side='bottom', pady=10)
        self.lbl_page = Label(nav_frame, text='')
        self.lbl_page.pack(side='left', padx=5)
    
    def _bind_keys(self) -> None:
        """Bind keyboard shortcuts."""
        self.bind('<Left>', lambda e: self.prev_page())
        self.bind('<Right>', lambda e: self.next_page())
    
    def select_file(self) -> None:
        """Open file dialog to select a Markdown file."""
        path = filedialog.askopenfilename(
            filetypes=[('Markdown', '*.md'), ('All Files', '*.*')]
        )
        if path:
            self.processor.input_file = path
            self.current_page_index = 0
            self.lbl_file.configure(text=os.path.basename(path))
            threading.Thread(target=self._task_parse_structure, daemon=True).start()
    
    def _task_parse_structure(self) -> None:
        """Background task to parse Markdown structure."""
        success = self.processor.parse_book_structure(self.processor.input_file)
        self.after(0, lambda: self._on_structure_parsed(success))
    
    def _on_structure_parsed(self, success: bool) -> None:
        """Handle completion of structure parsing."""
        if not success:
            messagebox.showerror('Error', 'Failed to parse Markdown file.')
            return
        
        self.selected_chapter_indices = list(range(len(self.processor.raw_chapters)))
        self.run_processing()
    
    def run_processing(self) -> None:
        """Start the rendering process."""
        if not self.processor.input_file or self.is_processing:
            return
        
        if self.selected_chapter_indices is None:
            self.selected_chapter_indices = list(range(len(self.processor.raw_chapters)))
        
        self.is_processing = True
        threading.Thread(target=self._task_render, daemon=True).start()
    
    def _task_render(self) -> None:
        """Background task to render chapters."""
        config = RenderConfig(
            font_path=self.processor.config.font_path,
            font_size=30,
            margin=8,
            line_height=1.1,
            font_weight=600,
            bottom_padding=0,
            top_padding=20,
            text_align="left",
            # Custom header sizes - adjust these as needed
            h1_size=44,  # Largest
            h2_size=38,
            h3_size=35,
            h4_size=32,
            h5_size=30,
            h6_size=28   # Smallest (same as body text)
        )
        success = self.processor.render_chapters(self.selected_chapter_indices, config)
        self.after(0, lambda: self._done(success))
    
    def _done(self, success: bool) -> None:
        """Handle completion of rendering."""
        self.is_processing = False
        if success:
            self.btn_export.configure(state='normal')
            new_idx = min(self.current_page_index, self.processor.total_pages - 1)
            self.show_page(new_idx)
        else:
            messagebox.showerror('Error', 'Processing failed.')
    
    def show_page(self, idx: int) -> None:
        """Display a specific page in the preview."""
        if not hasattr(self, 'img_label') or not self.processor.is_ready:
            return
        
        self.current_page_index = idx
        result = self.processor.render_page(idx)
        
        if result is None:
            return
        
        img, header_level = result
        
        base_size = 250
        if img.width > img.height:
            target_h = base_size
            target_w = int(target_h * img.width / img.height)
        else:
            target_w = base_size
            target_h = int(target_w * img.height / img.width)
        
        img_resized = img.resize((target_w, target_h), Image.LANCZOS)
        tk_img = ImageTk.PhotoImage(img_resized)
        self.img_label.configure(image=tk_img)
        self.img_label.image = tk_img
        
        # NEW: Display header level in status
        header_text = f' [H{header_level}]' if header_level > 0 else ''
        self.lbl_page.configure(
            text=f'Page {idx + 1} / {self.processor.total_pages}{header_text}'
        )
    
    def prev_page(self) -> None:
        """Navigate to the previous page."""
        self.show_page(max(0, self.current_page_index - 1))
    
    def next_page(self) -> None:
        """Navigate to the next page."""
        self.show_page(min(self.processor.total_pages - 1, self.current_page_index + 1))
    
    def export_file(self) -> None:
        """Open save dialog and export to XTC file."""
        path = filedialog.asksaveasfilename(defaultextension='.xtc')
        if path:
            threading.Thread(target=lambda: self._run_export(path), daemon=True).start()
    
    def _run_export(self, path: str) -> None:
        """Background task to export XTC file."""
        try:
            self.processor.save_xtc(path)
            self.after(0, lambda: messagebox.showinfo('Success', 'XTC file saved'))
        except Exception as e:
            self.after(0, lambda: messagebox.showerror('Error', f'Export failed: {e}'))

if __name__ == '__main__':
    app = App()
    app.mainloop()