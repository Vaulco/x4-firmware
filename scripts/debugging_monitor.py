import sys
import argparse
import re
from datetime import datetime
import time

# Try to import potentially missing packages
try:
    import serial
    from colorama import init, Fore, Style
except ImportError as e:
    missing_package = e.name
    print("\n" + "!" * 50)
    print(f" Error: The required package '{missing_package}' is not installed.")
    print("!" * 50)

    print(f"\nTo fix this, please run the following command in your terminal:\n")

    install_cmd = "pip install "
    packages = []
    if 'serial' in str(e): packages.append("pyserial")
    if 'colorama' in str(e): packages.append("colorama")

    print(f"    {install_cmd}{' '.join(packages)}")

    print("\nExiting...")
    sys.exit(1)

# Initialize colors
init(autoreset=True)

# Global statistics
stats = {
    'total_lines': 0,
    'errors': 0,
    'warnings': 0,
    'mem_reports': 0,
    'min_free_heap': float('inf'),
    'max_free_heap': 0,
    'start_time': None,
    'activity_changes': 0,
    'web_requests': 0,
    'display_refreshes': 0,
    'page_turns': 0
}

def get_color_for_line(line):
    """
    Classify log lines by type and assign appropriate colors.
    """
    line_upper = line.upper()

    # Critical errors
    if "ERROR" in line_upper or "FAILED" in line_upper:
        return Fore.RED
    
    # Warnings
    if "WARNING" in line_upper:
        return Fore.YELLOW
    
    # Memory reports
    if "[MEM]" in line_upper:
        return Fore.CYAN
    
    # Web/Network activity
    if "[WEB]" in line_upper or "[WEBACT]" in line_upper:
        return Fore.LIGHTBLUE_EX
    
    if "[WIFI]" in line_upper or "[WCS]" in line_upper:
        return Fore.LIGHTCYAN_EX
    
    # Display operations
    if "[GFX]" in line_upper or "DISPLAY" in line_upper or "REFRESH" in line_upper:
        return Fore.MAGENTA
    
    # File operations (XTC/SD Card)
    if "[XTC]" in line_upper or "[XTR]" in line_upper:
        return Fore.GREEN
    
    if "[SD]" in line_upper:
        return Fore.LIGHTGREEN_EX
    
    # Activity changes
    if "[ACT]" in line_upper:
        return Fore.YELLOW
    
    # Loop performance
    if "[LOOP]" in line_upper:
        return Fore.BLUE
    
    # Sleep/Power
    if "[SLP]" in line_upper:
        return Fore.LIGHTMAGENTA_EX
    
    # Boot messages
    if any(keyword in line_upper for keyword in ["ESP-ROM", "BUILD:", "RST:", "BOOT:", "ENTRY"]):
        return Fore.LIGHTBLACK_EX
    
    # Display initialization
    if "EINKDISPLAY:" in line_upper or "SSD1677" in line_upper:
        return Fore.LIGHTMAGENTA_EX

    return Fore.WHITE

def parse_memory_line(line):
    """
    Extracts Free, Total, and Min Free bytes from memory log lines.
    Format: [MEM] Free: 196344 bytes, Total: 226412 bytes, Min Free: 112620 bytes
    """
    match = re.search(r"Free:\s*(\d+).*?Total:\s*(\d+).*?Min Free:\s*(\d+)", line)
    if match:
        try:
            free_bytes = int(match.group(1))
            total_bytes = int(match.group(2))
            min_free_bytes = int(match.group(3))
            return free_bytes, total_bytes, min_free_bytes
        except ValueError:
            return None, None, None
    return None, None, None

def update_statistics(line):
    """Update global statistics based on log content"""
    stats['total_lines'] += 1
    
    if stats['start_time'] is None:
        stats['start_time'] = time.time()
    
    line_upper = line.upper()
    
    # Count errors and warnings
    if "ERROR" in line_upper or "FAILED" in line_upper:
        stats['errors'] += 1
    
    if "WARNING" in line_upper:
        stats['warnings'] += 1
    
    # Track memory
    if "[MEM]" in line_upper:
        free_bytes, total_bytes, min_free_bytes = parse_memory_line(line)
        if free_bytes is not None:
            stats['mem_reports'] += 1
            stats['min_free_heap'] = min(stats['min_free_heap'], free_bytes)
            stats['max_free_heap'] = max(stats['max_free_heap'], free_bytes)
    
    # Track activity changes
    if "ENTERING ACTIVITY" in line_upper or "EXITING ACTIVITY" in line_upper:
        stats['activity_changes'] += 1
    
    # Track web requests
    if "[WEB]" in line_upper and "SERVED" in line_upper:
        stats['web_requests'] += 1
    
    # Track display refreshes
    if "REFRESH" in line_upper and ("HALF" in line_upper or "FULL" in line_upper or "FAST" in line_upper):
        stats['display_refreshes'] += 1
    
    # Track page turns
    if "[XTR]" in line_upper and "RENDERED PAGE" in line_upper:
        stats['page_turns'] += 1

def print_statistics():
    """Print accumulated statistics"""
    if stats['start_time'] is None:
        return
    
    runtime = time.time() - stats['start_time']
    print(f"\n{Fore.CYAN}{'='*70}{Style.RESET_ALL}")
    print(f"{Fore.CYAN}Session Statistics:{Style.RESET_ALL}")
    print(f"  Runtime: {int(runtime // 60)}m {int(runtime % 60)}s")
    print(f"  Total lines: {stats['total_lines']}")
    print(f"  Errors: {Fore.RED}{stats['errors']}{Style.RESET_ALL}, "
          f"Warnings: {Fore.YELLOW}{stats['warnings']}{Style.RESET_ALL}")
    
    if stats['mem_reports'] > 0:
        min_kb = stats['min_free_heap'] / 1024
        max_kb = stats['max_free_heap'] / 1024
        print(f"  Memory reports: {stats['mem_reports']}")
        print(f"  Heap range: {Fore.GREEN}{min_kb:.1f} KB{Style.RESET_ALL} - "
              f"{Fore.GREEN}{max_kb:.1f} KB{Style.RESET_ALL}")
    
    if stats['activity_changes'] > 0:
        print(f"  Activity changes: {stats['activity_changes']}")
    
    if stats['web_requests'] > 0:
        print(f"  Web requests served: {stats['web_requests']}")
    
    if stats['display_refreshes'] > 0:
        print(f"  Display refreshes: {stats['display_refreshes']}")
    
    if stats['page_turns'] > 0:
        print(f"  Pages rendered: {stats['page_turns']}")
    
    print(f"{Fore.CYAN}{'='*70}{Style.RESET_ALL}\n")

def serial_worker(port, baud, filter_keywords, show_stats, hide_timestamps):
    """
    Handles reading serial and printing to console.
    """
    print(f"{Fore.CYAN}--- Opening {port} at {baud} baud ---{Style.RESET_ALL}")

    try:
        ser = serial.Serial(port, baud, timeout=0.1)
        ser.dtr = False
        ser.rts = False
    except serial.SerialException as e:
        print(f"{Fore.RED}Error opening port: {e}{Style.RESET_ALL}")
        return

    last_stats_time = time.time()
    
    try:
        while True:
            try:
                raw_data = ser.readline().decode('utf-8', errors='replace')

                if not raw_data:
                    # Print periodic stats if enabled
                    if show_stats and time.time() - last_stats_time >= 30:
                        print_statistics()
                        last_stats_time = time.time()
                    continue

                clean_line = raw_data.strip()
                if not clean_line:
                    continue

                # Apply filter if specified
                if filter_keywords:
                    if not any(keyword.upper() in clean_line.upper() for keyword in filter_keywords):
                        continue

                # Add PC timestamp (or remove device timestamp if hide_timestamps)
                if hide_timestamps:
                    formatted_line = re.sub(r"^\[\d+\]\s*", "", clean_line)
                else:
                    pc_time = datetime.now().strftime("%H:%M:%S.%f")[:-3]  # Include milliseconds
                    formatted_line = re.sub(r"^\[\d+\]", f"[{pc_time}]", clean_line)

                # Update statistics
                update_statistics(formatted_line)

                # Print to console with color
                line_color = get_color_for_line(formatted_line)
                print(f"{line_color}{formatted_line}{Style.RESET_ALL}")

            except OSError:
                print(f"{Fore.RED}Device disconnected.{Style.RESET_ALL}")
                break
            except UnicodeDecodeError:
                # Skip malformed data
                continue
                
    except KeyboardInterrupt:
        pass
    finally:
        if 'ser' in locals() and ser.is_open:
            ser.close()
        
        # Print final statistics
        if show_stats:
            print_statistics()

def main():
    parser = argparse.ArgumentParser(
        description="ESP32 Serial Monitor with color coding and statistics",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s COM5                                # Monitor COM5 at 115200 baud
  %(prog)s /dev/ttyUSB0 --baud 9600            # Custom baud rate
  %(prog)s COM5 --filter MEM WEB               # Only show lines with MEM or WEB
  %(prog)s COM5 --stats                        # Show statistics every 30s
  %(prog)s COM5 --filter ERROR WARNING         # Only show errors/warnings
  %(prog)s COM5 --no-timestamps                # Hide timestamps for cleaner output
        """
    )
    parser.add_argument("port", nargs="?", default="COM5", help="Serial port (default: COM5)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--filter", nargs="+", metavar="KEYWORD", 
                       help="Only show lines containing these keywords (case-insensitive)")
    parser.add_argument("--stats", action="store_true",
                       help="Show statistics every 30 seconds and on exit")
    parser.add_argument("--no-timestamps", action="store_true",
                       help="Hide timestamps for cleaner output")
    args = parser.parse_args()

    print(f"{Fore.CYAN}ESP32 Serial Monitor{Style.RESET_ALL}")
    print(f"Port: {args.port}, Baud: {args.baud}")
    
    if args.filter:
        print(f"Filter: {', '.join(args.filter)}")
    
    if args.stats:
        print("Statistics: Enabled")
    
    if args.no_timestamps:
        print("Timestamps: Hidden")
    
    print(f"{Fore.CYAN}{'='*70}{Style.RESET_ALL}\n")

    try:
        serial_worker(args.port, args.baud, args.filter, args.stats, args.no_timestamps)
    except KeyboardInterrupt:
        print(f"\n{Fore.YELLOW}Exiting...{Style.RESET_ALL}")

if __name__ == "__main__":
    main()