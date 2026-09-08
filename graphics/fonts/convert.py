#!/usr/bin/env python3
"""
Advanced file to header converter with compression support

Usage:
    python3 file_to_header_advanced.py input.ttf output.h [options]

Options:
    -n NAME, --name NAME     Custom variable name
    -b NUM, --bytes NUM      Bytes per line (default: 12)
    -c, --compress           Compress data with zlib
    --inc                    Generate .inc file (data only, no header guards)
"""

import sys
import os
import argparse
import zlib

def file_to_header(input_path, output_path, var_name=None, bytes_per_line=12, 
                   compress=False, inc_file=False):
    """Convert any file to a C header with embedded byte array"""
    
    # Read input file
    try:
        with open(input_path, 'rb') as f:
            original_data = f.read()
    except FileNotFoundError:
        print(f"Error: File '{input_path}' not found")
        return False
    except Exception as e:
        print(f"Error reading file: {e}")
        return False
    
    if len(original_data) == 0:
        print(f"Error: File '{input_path}' is empty")
        return False
    
    # Compress if requested
    data = original_data
    original_size = len(original_data)
    
    if compress:
        data = zlib.compress(original_data, level=9)
        compressed_size = len(data)
        compression_ratio = (1 - compressed_size / original_size) * 100
        print(f"Compressed: {original_size:,} → {compressed_size:,} bytes "
              f"({compression_ratio:.1f}% reduction)")
    
    # Generate variable name
    if var_name is None:
        base_name = os.path.splitext(os.path.basename(input_path))[0]
        var_name = ''.join(c if c.isalnum() else '_' for c in base_name)
        var_name = var_name.lower()
    
    # Write output file
    try:
        with open(output_path, 'w') as f:
            if not inc_file:
                # Full header file with guards
                header_guard = f"{var_name.upper()}_H"
                f.write(f"#ifndef {header_guard}\n")
                f.write(f"#define {header_guard}\n\n")
                f.write(f"/* Embedded data from: {os.path.basename(input_path)} */\n")
                f.write(f"/* Original size: {original_size} bytes */\n")
                
                if compress:
                    f.write(f"/* Compressed size: {len(data)} bytes */\n")
                    f.write(f"/* Compression: zlib level 9 */\n")
                
                f.write(f"\n")
                
                if compress:
                    f.write("#include <zlib.h>\n\n")
                
                f.write(f"static const unsigned char {var_name}_data")
                if compress:
                    f.write("_compressed")
                f.write("[] = {\n")
            
            # Write data bytes
            for i in range(0, len(data), bytes_per_line):
                chunk = data[i:i+bytes_per_line]
                hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
                f.write(f'    {hex_values}')
                
                if i + bytes_per_line < len(data):
                    f.write(',')
                
                f.write('\n')
            
            if not inc_file:
                f.write('};\n\n')
                
                # Size constants
                f.write(f"static const unsigned int {var_name}_size")
                if compress:
                    f.write("_compressed")
                f.write(f" = {len(data)};\n")
                
                if compress:
                    f.write(f"static const unsigned int {var_name}_size_original = {original_size};\n\n")
                    
                    # Add decompression helper
                    f.write(f"/* Helper function to decompress data */\n")
                    f.write(f"static unsigned char* {var_name}_decompress() {{\n")
                    f.write(f"    unsigned char* output = malloc({original_size});\n")
                    f.write(f"    if (!output) return NULL;\n")
                    f.write(f"    \n")
                    f.write(f"    uLongf dest_len = {original_size};\n")
                    f.write(f"    int result = uncompress(output, &dest_len, \n")
                    f.write(f"                           {var_name}_data_compressed, \n")
                    f.write(f"                           {var_name}_size_compressed);\n")
                    f.write(f"    \n")
                    f.write(f"    if (result != Z_OK) {{\n")
                    f.write(f"        free(output);\n")
                    f.write(f"        return NULL;\n")
                    f.write(f"    }}\n")
                    f.write(f"    \n")
                    f.write(f"    return output;\n")
                    f.write(f"}}\n\n")
                else:
                    f.write('\n')
                
                # Close header guard
                f.write(f"#endif /* {header_guard} */\n")
        
        print(f"Successfully created '{output_path}'")
        print(f"  Variable: {var_name}_data")
        print(f"  Size: {len(data):,} bytes")
        if compress:
            print(f"  Use {var_name}_decompress() to get original data")
        
        return True
        
    except Exception as e:
        print(f"Error writing output file: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description='Convert any file to C header with embedded data',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog='''
Examples:
  %(prog)s myfont.ttf myfont.h
  %(prog)s font.ttf font.h -n custom_font
  %(prog)s large_font.ttf font.h --compress
  %(prog)s font.ttf font.inc --inc
        '''
    )
    
    parser.add_argument('input', help='Input file (TTF, PNG, or any binary file)')
    parser.add_argument('output', help='Output header file (.h or .inc)')
    parser.add_argument('-n', '--name', help='Custom variable name')
    parser.add_argument('-b', '--bytes', type=int, default=12, 
                       help='Bytes per line (default: 12)')
    parser.add_argument('-c', '--compress', action='store_true',
                       help='Compress data with zlib')
    parser.add_argument('--inc', action='store_true',
                       help='Generate .inc file (data only)')
    
    args = parser.parse_args()
    
    # Ensure proper extension
    if args.inc and not args.output.endswith('.inc'):
        args.output += '.inc'
    elif not args.inc and not args.output.endswith('.h'):
        args.output += '.h'
    
    success = file_to_header(
        args.input,
        args.output,
        var_name=args.name,
        bytes_per_line=args.bytes,
        compress=args.compress,
        inc_file=args.inc
    )
    
    sys.exit(0 if success else 1)

if __name__ == '__main__':
    main()
