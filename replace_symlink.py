#!/usr/bin/env python3
"""
Utility to replace a symlink directory with a real directory containing symlinks to files.

Usage:
    python replace_symlink.py <directory_path>
"""
import argparse
import os
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Options:
    """Options for processing symlink directory"""
    target_path: str
    verbose: bool = False
    replace: bool = False
    force: bool = False


def process_symlink_directory(options: Options) -> None:
    """
    Process symlink directory:
    1. Verify it's a symlink
    2. Create a real directory with the same name
    3. Recreate structure with symlinks to files
    
    Args:
        options: Processing options
    """
    target_path = options.target_path
    target = Path(target_path).resolve()
    
    # Check 1: Must be an existing directory
    if not target.exists():
        print(f"Error: Path '{target_path}' does not exist", file=sys.stderr)
        sys.exit(1)
    
    # Check 2: Must be a symlink
    if not Path(target_path).is_symlink():
        print(f"Error: '{target_path}' is not a symlink", file=sys.stderr)
        sys.exit(1)
    
    # Check 3: Symlink must point to a directory
    if not target.is_dir():
        print(f"Error: Symlink '{target_path}' does not point to a directory", file=sys.stderr)
        sys.exit(1)
    
    # Get resolved path (where symlink points to)
    resolved_path = target
    original_symlink = Path(target_path)
    
    print(f"Processing symlink: {original_symlink}")
    print(f"Resolved path: {resolved_path}")
    
    # Create new directory next to target
    new_dir_name = f"{original_symlink.name}.file_symlinks"
    new_dir = original_symlink.parent / new_dir_name
    
    if new_dir.exists():
        if options.force:
            print(f"Removing existing directory: {new_dir}")
            shutil.rmtree(new_dir)
        else:
            print(f"Error: Directory '{new_dir}' already exists", file=sys.stderr)
            sys.exit(1)
    
    print(f"Creating new directory: {new_dir}")
    new_dir.mkdir(parents=True, exist_ok=False)
    
    # Recursively traverse resolved directory
    def create_symlink_structure(src_dir: Path, dst_dir: Path) -> None:
        """Recursively create structure with symlinks to files"""
        for item in src_dir.iterdir():
            src_item = item
            dst_item = dst_dir / item.name
            
            if src_item.is_dir() and not src_item.is_symlink():
                # For directories create real directories
                if options.verbose:
                    print(f"  Creating directory: {dst_item.relative_to(new_dir)}")
                dst_item.mkdir(exist_ok=True)
                # Recursively process contents
                create_symlink_structure(src_item, dst_item)
            else:
                # For files create symlinks
                if options.verbose:
                    print(f"  Creating symlink: {dst_item.relative_to(new_dir)} -> {src_item}")
                dst_item.symlink_to(src_item)
    
    # Create structure
    print(f"Creating symlink structure...")
    create_symlink_structure(resolved_path, new_dir)
    print(f"New directory created: {new_dir}")
    
    if options.replace:
        print(f"Performing replacement:")
        if options.force:
            # If force flag is set, delete original symlink
            print(f"  Removing {original_symlink}")
            original_symlink.unlink()
            
            print(f"  Renaming {new_dir} -> {original_symlink}")
            new_dir.rename(original_symlink)
            
            print(f"Replacement completed!")
        else:
            # Otherwise, rename to backup
            backup_name = f"{original_symlink.name}.dir_symlink"
            backup_path = original_symlink.parent / backup_name
            print(f"  Renaming {original_symlink} -> {backup_path}")
            original_symlink.rename(backup_path)
            
            print(f"  Renaming {new_dir} -> {original_symlink}")
            new_dir.rename(original_symlink)
            
            print(f"Replacement completed!")
            print(f"Original symlink saved as: {backup_path}")
    else:
        print(f"To replace the original symlink, run:")
        print(f"  mv '{original_symlink}' '{original_symlink}.dir_symlink'")
        print(f"  mv '{new_dir}' '{original_symlink}'")


def main():
    parser = argparse.ArgumentParser(
        description="Replace symlink directory with a real directory containing symlinks to files"
    )
    parser.add_argument(
        "target_path",
        type=str,
        help="Path to the symlink directory to process"
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="Print detailed information about each element creation"
    )
    parser.add_argument(
        "-r", "--replace",
        action="store_true",
        help="Automatically replace the original symlink (saving it as .dir_symlink)"
    )
    parser.add_argument(
        "-f", "--force",
        action="store_true",
        help="Remove existing directory if it already exists"
    )
    
    args = parser.parse_args()
    options = Options(
        target_path=args.target_path,
        verbose=args.verbose,
        replace=args.replace,
        force=args.force
    )
    process_symlink_directory(options)


if __name__ == "__main__":
    main()
