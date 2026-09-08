import os
import re

# Regex to catch various ways __ANDROID__ is checked
# Matches #ifdef __ANDROID__, #if defined(__ANDROID__), and #if defined __ANDROID__
pattern = re.compile(r'#\s*(if\s+defined\(__ANDROID__\)|if\s+defined\s+__ANDROID__|ifdef\s+__ANDROID__)')

def audit_file(filepath):
    found_count = 0
    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"Could not read {filepath}: {e}")
        return 0

    file_header_printed = False
    
    for i, line in enumerate(lines):
        if pattern.search(line):
            if not file_header_printed:
                print(f"\n{'='*80}\nFILE: {filepath}\n{'='*80}")
                file_header_printed = True
            
            found_count += 1
            print(f"[Block {found_count} | Line {i+1}]: {line.strip()}")
            
            # Tracking nesting levels to find the matching #endif
            stack = 1
            cursor = i + 1
            while stack > 0 and cursor < len(lines):
                curr_line = lines[cursor].strip()
                
                # If we hit another #if / #ifdef / #ifndef, increase stack
                if re.match(r'#\s*if', curr_line):
                    stack += 1
                # If we hit an #endif, decrease stack
                elif re.match(r'#\s*endif', curr_line):
                    stack -= 1
                
                print(f"  {lines[cursor].rstrip()}")
                cursor += 1
            print("-" * 40)
    
    return found_count

def main():
    # Define which extensions to scan
    extensions = ('.cpp', '.h', '.hpp', '.c', '.cc', '.cxx')
    total_files_scanned = 0
    total_blocks_found = 0
    
    print("Starting Recursive Audit of __ANDROID__ blocks...")
    
    # os.walk scans the current directory '.' and all subdirectories
    for root, dirs, files in os.walk('.'):
        # Optional: Skip specific directories (like .git or build folders)
        if '.git' in dirs:
            dirs.remove('.git')
        if 'build' in dirs:
            dirs.remove('build')

        for file in files:
            if file.lower().endswith(extensions):
                total_files_scanned += 1
                filepath = os.path.join(root, file)
                total_blocks_found += audit_file(filepath)

    print(f"\nAudit Complete.")
    print(f"Total Files Scanned: {total_files_scanned}")
    print(f"Total __ANDROID__ Blocks Found: {total_blocks_found}")

if __name__ == "__main__":
    main()
