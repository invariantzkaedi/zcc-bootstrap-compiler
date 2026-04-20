#!/bin/sh
ZCC=/mnt/h/__DOWNLOADS/zcc_github_upload/zcc

# Pattern 1
echo 'typedef struct S S; extern S *(func1)(int x);' > /tmp/p1.c
echo "=== Pattern 1 ==="
$ZCC /tmp/p1.c -o /tmp/p1.s 2>&1 | grep "error:" || echo "OK"

# Pattern 2
echo 'typedef struct S S; extern S *func2(int x);' > /tmp/p2.c
echo "=== Pattern 2 ==="
$ZCC /tmp/p2.c -o /tmp/p2.s 2>&1 | grep "error:" || echo "OK"

# Pattern 3
echo 'typedef struct S S; extern int (func3)(int x);' > /tmp/p3.c
echo "=== Pattern 3 ==="
$ZCC /tmp/p3.c -o /tmp/p3.s 2>&1 | grep "error:" || echo "OK"

# Pattern 4
echo 'typedef struct S S; extern S **(func4)(int x);' > /tmp/p4.c
echo "=== Pattern 4 ==="
$ZCC /tmp/p4.c -o /tmp/p4.s 2>&1 | grep "error:" || echo "OK"

# Pattern 5
echo 'typedef struct S S; extern int (*func5(int x))(int);' > /tmp/p5.c
echo "=== Pattern 5 ==="
$ZCC /tmp/p5.c -o /tmp/p5.s 2>&1 | grep "error:" || echo "OK"
