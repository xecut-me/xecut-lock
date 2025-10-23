#!/usr/bin/env python3

import os
import sys
import argparse

def main():
    parser = argparse.ArgumentParser(description='Generate cryptographically random KDF key')
    parser.add_argument('key_path', help='Path to save the KDF key file')
    parser.add_argument('length', type=int, help='Length of the key in bytes')
    
    args = parser.parse_args()
    
    if args.length <= 0:
        print("Invalid args: Key length must be positive", file=sys.stderr)
        return 1
    
    try:
        random_bytes = os.urandom(args.length)
        
        with open(args.key_path, 'wb') as f:
            f.write(random_bytes)
        
        print(f"Generated {args.length} byte KDF key and saved to {args.key_path}")

    except Exception as e:
        print(f"Failed to generate KDF key: {e}", file=sys.stderr)
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
