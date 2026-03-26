#!/usr/bin/env python3
"""
ax-rtm-verify.py — Requirements Traceability Matrix Verification
axioma-agent Layer 5

DVEC: v1.3 | SRS-002 v0.3

Verifies that all public functions trace to SRS requirements.

Copyright (c) 2026 The Murray Family Innovation Trust
SPDX-License-Identifier: GPL-3.0-or-later
Patent: UK GB2521625.0
"""

import os
import re
import sys
import argparse

# Required SRS-002 mappings for each public function
REQUIRED_MAPPINGS = {
    'ax_agent_init': ['SRS-002-SHALL-026'],
    'ax_agent_bind': ['SRS-002-SHALL-026'],
    'ax_agent_step': [
        'SRS-002-SHALL-006',
        'SRS-002-SHALL-007',
        'SRS-002-SHALL-013'
    ],
    'ax_health_state_to_str': ['SRS-002-SHALL-027'],
    'ax_input_class_to_str': ['SRS-002-SHALL-027'],
    'ax_violation_to_str': ['SRS-002-SHALL-027'],
    'ax_extract_timestamp': ['SRS-002-SHALL-009'],
    'ax_hash_equal_ct': ['SRS-002-SHALL-026'],
}

def find_source_files(root):
    """Find all .c and .h files"""
    files = []
    for dirpath, _, filenames in os.walk(root):
        for f in filenames:
            if f.endswith('.c') or f.endswith('.h'):
                filepath = os.path.join(dirpath, f)
                # Skip build directories
                if '/build/' not in filepath:
                    files.append(filepath)
    return files

def extract_srs_refs(content):
    """Extract all SRS-002-SHALL-XXX references"""
    pattern = r'SRS-002-SHALL-\d{3}'
    return set(re.findall(pattern, content))

def verify_function_traceability(files):
    """Verify each public function has required SRS traceability"""
    all_content = ""
    for filepath in files:
        with open(filepath, 'r') as f:
            all_content += f.read()
    
    results = {}
    all_refs = set()
    
    for func, required_refs in REQUIRED_MAPPINGS.items():
        # Find function definition/declaration with preceding comments
        # Look for traceability comment near function
        pattern = rf'(@traceability[^\n]*{func}|{func}[^{{]*@traceability)'
        
        # Simpler approach: find SRS refs near the function name
        func_pattern = rf'{func}\s*\([^)]*\)'
        matches = list(re.finditer(func_pattern, all_content))
        
        found_refs = set()
        for match in matches:
            # Get surrounding context (500 chars before)
            start = max(0, match.start() - 500)
            context = all_content[start:match.end()]
            refs = extract_srs_refs(context)
            found_refs.update(refs)
        
        # Check if required refs are present
        missing = set(required_refs) - found_refs
        results[func] = {
            'required': required_refs,
            'found': list(found_refs),
            'missing': list(missing),
            'status': len(missing) == 0
        }
        all_refs.update(found_refs)
    
    return results, all_refs

def main():
    parser = argparse.ArgumentParser(
        description='Verify SRS-002 traceability for axioma-agent'
    )
    parser.add_argument(
        '--root',
        default='.',
        help='Root directory of axioma-agent'
    )
    parser.add_argument(
        '--ci',
        action='store_true',
        help='CI mode: exit non-zero on failure'
    )
    args = parser.parse_args()
    
    print("=" * 60)
    print("axioma-agent RTM Verification")
    print("DVEC: v1.3 | SRS-002 v0.3")
    print("=" * 60)
    print()
    
    files = find_source_files(args.root)
    if not files:
        print("ERROR: No source files found")
        sys.exit(1)
    
    results, all_refs = verify_function_traceability(files)
    
    print("Function Traceability Check:")
    print("-" * 40)
    
    all_passed = True
    for func, data in results.items():
        status = "✓" if data['status'] else "✗"
        refs_str = ", ".join(data['found'][:3])  # Show first 3
        print(f"  {status} {func}: {refs_str}")
        if not data['status']:
            all_passed = False
            print(f"      MISSING: {', '.join(data['missing'])}")
    
    print()
    print("=" * 60)
    print("VERIFICATION RESULTS")
    print("=" * 60)
    print()
    print("-" * 60)
    
    if all_passed:
        print("RESULT: CONFORMANT")
        print("All SRS traceability requirements satisfied.")
    else:
        print("RESULT: NON-CONFORMANT")
        print("Missing SRS traceability anchors.")
    
    print()
    print(f"SRS references found: {len(all_refs)}")
    print(f"Functions checked: {len(results)}")
    print("-" * 60)
    
    if args.ci and not all_passed:
        sys.exit(1)
    
    return 0 if all_passed else 1

if __name__ == '__main__':
    sys.exit(main())
