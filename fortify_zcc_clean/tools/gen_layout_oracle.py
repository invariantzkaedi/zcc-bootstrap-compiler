#!/usr/bin/env python3

from pathlib import Path

CASES = [
    {
        "name": "basic",
        "kind": "struct",
        "type": "struct Basic",
        "decl": """
struct Basic {
    char c;
    int i;
    char tail;
};
""",
        "fields": ["c", "i", "tail"],
    },
    {
        "name": "double_middle",
        "kind": "struct",
        "type": "struct DoubleMiddle",
        "decl": """
struct DoubleMiddle {
    char c;
    double d;
    int i;
};
""",
        "fields": ["c", "d", "i"],
    },
    {
        "name": "union_basic",
        "kind": "union",
        "type": "union U",
        "decl": """
union U {
    char c;
    int i;
    double d;
};
""",
        "fields": ["c", "i", "d"],
    },
    {
        "name": "nested",
        "kind": "struct",
        "type": "struct Nested",
        "decl": """
struct Nested {
    char c;
    struct {
        int x;
        char y;
    } inner;
    double d;
};
""",
        "fields": ["c", "inner", "d"],
    },
    {
        "name": "array_case",
        "kind": "struct",
        "type": "struct ArrayCase",
        "decl": """
struct ArrayCase {
    char a[3];
    int b[2];
};
""",
        "fields": ["a", "b"],
    },
]

TEMPLATE = r'''
#include <stddef.h>
#include <stdio.h>

{decl}

int main(void) {{
    printf("sizeof=%zu\n", sizeof({type_name}));
    printf("alignof=%zu\n", _Alignof({type_name}));
{offsets}
    return 0;
}}
'''

def offset_lines(case):
    if case["kind"] == "union":
        return "".join(
            f'    printf("offsetof.{field}=%zu\\n", offsetof({case["type"]}, {field}));\n'
            for field in case["fields"]
        )

    return "".join(
        f'    printf("offsetof.{field}=%zu\\n", offsetof({case["type"]}, {field}));\n'
        for field in case["fields"]
    )

def main():
    out_dir = Path("tests/layout")
    out_dir.mkdir(parents=True, exist_ok=True)

    for case in CASES:
        source = TEMPLATE.format(
            decl=case["decl"],
            type_name=case["type"],
            offsets=offset_lines(case),
        )

        path = out_dir / f'oracle_{case["name"]}.c'
        path.write_text(source, encoding="utf-8")

if __name__ == "__main__":
    main()