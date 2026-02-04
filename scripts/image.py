import tkinter as tk
from tkinter import filedialog, messagebox
import base64
import pyperclip
import os
from PIL import Image
import io

# Supported extensions
IMAGE_EXTS = ["png", "jpg", "jpeg", "gif", "bmp"]
VIDEO_EXTS = ["mp4", "webm", "mov", "avi", "mkv"]

# Compression settings
MAX_WIDTH = 600 # Maximum width in pixels
MAX_HEIGHT = 800  # Maximum height in pixels
JPEG_QUALITY = 85  # Quality for JPEG compression (1-100)

def compress_image(file_path):
    """Compress and resize image while maintaining aspect ratio"""
    img = Image.open(file_path)
    
    # Convert RGBA to RGB if necessary (for JPEG compatibility)
    if img.mode == 'RGBA':
        background = Image.new('RGB', img.size, (255, 255, 255))
        background.paste(img, mask=img.split()[3])
        img = background
    elif img.mode != 'RGB':
        img = img.convert('RGB')
    
    # Resize if image is larger than max dimensions
    if img.width > MAX_WIDTH or img.height > MAX_HEIGHT:
        img.thumbnail((MAX_WIDTH, MAX_HEIGHT), Image.Resampling.LANCZOS)
    
    # Save to bytes buffer with compression
    buffer = io.BytesIO()
    img.save(buffer, format='JPEG', quality=JPEG_QUALITY, optimize=True)
    buffer.seek(0)
    
    return buffer.read(), 'jpeg'

def select_file():
    file_path = filedialog.askopenfilename(
        title="Select an Image or Video",
        filetypes=[("Media Files", "*.png *.jpg *.jpeg *.gif *.bmp *.mp4 *.webm *.mov *.avi *.mkv")]
    )
    if not file_path:
        return

    file_name = os.path.splitext(os.path.basename(file_path))[0]
    ext = file_path.split('.')[-1].lower()

    try:
        if ext in IMAGE_EXTS:
            # Compress image
            compressed_data, output_ext = compress_image(file_path)
            encoded_string = base64.b64encode(compressed_data).decode('utf-8')
            markdown_embed = f"![{file_name}](data:image/{output_ext};base64,{encoded_string})\n"
            
        elif ext in VIDEO_EXTS:
            # Videos are not compressed (would require ffmpeg)
            with open(file_path, "rb") as f:
                encoded_string = base64.b64encode(f.read()).decode('utf-8')
            markdown_embed = f"[{file_name}](data:video/{ext};base64,{encoded_string})\n"
            
        else:
            messagebox.showerror("Error", "Unsupported file type!")
            return

        pyperclip.copy(markdown_embed)
        messagebox.showinfo("Success", "Markdown embed copied to clipboard!")

    except Exception as e:
        messagebox.showerror("Error", f"Failed to process file:\n{e}")

# GUI
root = tk.Tk()
root.title("Media to Markdown Base64")
root.geometry("300x100")

button = tk.Button(root, text="Select Image or Video", command=select_file)
button.pack(expand=True, pady=20)

root.mainloop()