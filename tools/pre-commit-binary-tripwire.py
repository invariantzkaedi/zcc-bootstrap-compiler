#!/usr/bin/env python3
import sys
import os
import subprocess

# Root-level files that are explicitly allowed to have no extension or custom extension
ALLOWED_ROOT_NAMES = {
    "Makefile",
    "Makefile.dbg",
    "Makefile.ir",
    "Makefile.release",
    "Makefile.zld",
    "CODEOWNERS",
    "Dockerfile",
    "LICENSE",
    "char"  # wait, is 'char' tracked? Let's see
}

ALLOWED_EXTENSIONS = {
    ".c", ".h", ".py", ".md", ".txt", ".json", ".yaml", ".yml",
    ".sh", ".bat", ".patch", ".gitignore", ".gitattributes", ".xml",
    ".html", ".js"
}

def is_binary(filepath):
    """Check if a file is binary by searching for null bytes."""
    try:
        with open(filepath, 'rb') as f:
            chunk = f.read(1024)
            return b'\x00' in chunk
    except Exception:
        return False

def get_staged_files():
    """Get list of staged files from git diff --cached --diff-filter=d --name-only."""
    try:
        res = subprocess.run(
            ["git", "diff", "--cached", "--diff-filter=d", "--name-only"],
            capture_output=True, text=True, check=True
        )
        return [f.strip() for f in res.stdout.splitlines() if f.strip()]
    except Exception as e:
        print(f"Error fetching staged files: {e}", file=sys.stderr)
        return []

def main():
    staged_files = get_staged_files()
    failed = False

    for rel_path in staged_files:
        if not os.path.exists(rel_path):
            continue  # deleted files are fine
        
        # Check size (e.g. limit to 1MB = 1,048,576 bytes)
        size_bytes = os.path.getsize(rel_path)
        if size_bytes > 1024 * 1024:
            print(f"❌ REJECTED: File '{rel_path}' is too large ({size_bytes / 1024 / 1024:.2f} MB). Maximum size is 1.00 MB.", file=sys.stderr)
            failed = True
            continue

        # Check files staged at the root
        dirname = os.path.dirname(rel_path)
        if dirname == "":
            filename = os.path.basename(rel_path)
            _, ext = os.path.splitext(filename)
            
            # Allow .gitignore, .gitattributes, etc.
            if filename.startswith('.'):
                continue
                
            # If not an allowed root name and not an allowed extension
            if filename not in ALLOWED_ROOT_NAMES and ext not in ALLOWED_EXTENSIONS:
                print(f"❌ REJECTED: Staging binary or non-whitelisted file '{rel_path}' at repository root is blocked.", file=sys.stderr)
                failed = True
                continue

            # Check if it contains binary signatures (null bytes)
            if is_binary(rel_path):
                print(f"❌ REJECTED: Staged file '{rel_path}' contains binary data.", file=sys.stderr)
                failed = True
                continue

    if failed:
        print("\nUse subdirectories (e.g., tests/, experiments/, tools/) for temporary files and binaries.", file=sys.stderr)
        print("Ensure temporary binaries are ignored in .gitignore.", file=sys.stderr)
        sys.exit(1)

    sys.exit(0)

if __name__ == "__main__":
    main()
