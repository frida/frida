import os
import r2pipe

def scan_files_for_pattern(directory, hex_pattern, file_extension=None):
    search_command = "/xj " + hex_pattern.strip()

    for root, _, files in os.walk(directory):
        for filename in files:
            if file_extension and not filename.endswith(file_extension):
                continue

            file_path = os.path.join(root, filename)
            try:
                r2 = r2pipe.open(file_path, flags=["-n"])
                r2.cmd("e io.cache=true")

                result = r2.cmdj(search_command)

                if result:
                    print(f"Found in {file_path}:")
                    for match in result:
                        print(f"  - Offset: 0x{match['offset']:x}")

                r2.quit()
            except Exception as e:
                print(f"    Error processing {file_path}: {e}")

if __name__ == "__main__":
    import argparse

    parser = argparse.ArgumentParser(description="Scan dump files for a hex pattern using r2pipe.")
    parser.add_argument("directory", help="Directory containing dump files")
    parser.add_argument("pattern", help="Hex pattern to search (e.g. 'ff ff ff ff')")
    parser.add_argument("--ext", help="Only scan files with this extension", default=None)

    args = parser.parse_args()
    scan_files_for_pattern(args.directory, args.pattern, args.ext)
