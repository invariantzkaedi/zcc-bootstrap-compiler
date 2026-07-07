#!/usr/bin/env python3
import os

fixtures = {
    "01_branch_prune_copy_prop": {
        "input": """func @t(i32 %arg0) -> i32 {
bb0:
  %c1 = const i1 1
  br i1 %c1, bb1, bb2
bb1:
  %c0 = const i32 0
  %v1 = add i32 %arg0, %c0
  jmp bb3
bb2:
  %v2 = const i32 999
  jmp bb3
bb3:
  %p = phi i32 [bb1: %v1], [bb2: %v2]
  ret i32 %p
}
""",
        "expected": """func @t(i32 %arg0) -> i32 {
bb0:
  %c1 = const i1 1
  jmp bb1
bb1:
  %c0 = const i32 0
  %v1 = copy i32 %arg0
  jmp bb3
bb2:
bb3:
  ret i32 %v1
}
"""
    },
    "02_sccp_instcombine_chain": {
        "input": """func @t() -> i32 {
bb0:
  %x = const i32 5
  %c0 = const i32 0
  %y = add i32 %x, %c0
  %c2 = const i32 2
  %z = mul i32 %y, %c2
  ret i32 %z
}
""",
        "expected": """func @t() -> i32 {
bb0:
  %x = const i32 5
  %c0 = const i32 0
  %y = const i32 5
  %c2 = const i32 2
  %z = const i32 10
  ret i32 %z
}
"""
    },
    "03_cfg_simplify_phi_collapse": {
        "input": """func @t(i32 %arg0) -> i32 {
bb0:
  %c = const i1 1
  br i1 %c, bb1, bb2
bb1:
  jmp bb3
bb2:
  jmp bb3
bb3:
  %p = phi i32 [bb1: %arg0], [bb2: %arg0]
  ret i32 %p
}
""",
        "expected": """func @t(i32 %arg0) -> i32 {
bb0:
  %c = const i1 1
  jmp bb1
bb1:
  jmp bb3
bb2:
bb3:
  ret i32 %arg0
}
"""
    },
    "04_loop_trip_const_prune": {
        "input": """func @t() -> i32 {
bb0:
  %zero = const i32 0
  jmp bb1
bb1:
  %cond = const i1 0
  br i1 %cond, bb2, bb3
bb2:
  %one = const i32 1
  jmp bb1
bb3:
  ret i32 %zero
}
""",
        "expected": """func @t() -> i32 {
bb0:
  %zero = const i32 0
  jmp bb1
bb1:
  %cond = const i1 0
  jmp bb3
bb2:
bb3:
  ret i32 %zero
}
"""
    },
    "05_sub_self_branch_prune": {
        "input": """func @t(i32 %arg0) -> i32 {
bb0:
  %sub = sub i32 %arg0, %arg0
  %c0 = const i32 0
  %cond = icmp eq i32 %sub, %c0
  br i1 %cond, bb1, bb2
bb1:
  %res1 = const i32 42
  jmp bb3
bb2:
  %res2 = const i32 24
  jmp bb3
bb3:
  %p = phi i32 [bb1: %res1], [bb2: %res2]
  ret i32 %p
}
""",
        "expected": """func @t(i32 %arg0) -> i32 {
bb0:
  %sub = const i32 0
  %c0 = const i32 0
  %cond = const i1 1
  jmp bb1
bb1:
  %res1 = const i32 42
  jmp bb3
bb2:
bb3:
  %p = const i32 42
  ret i32 %p
}
"""
    },
    "06_cascade_binop_fold": {
        "input": """func @t(i32 %arg0) -> i32 {
bb0:
  %c0 = const i32 0
  %a = add i32 %arg0, %c0
  %c1 = const i32 1
  %b = mul i32 %a, %c1
  %c = sub i32 %b, %c0
  ret i32 %c
}
""",
        "expected": """func @t(i32 %arg0) -> i32 {
bb0:
  %c0 = const i32 0
  %a = copy i32 %arg0
  %c1 = const i32 1
  %b = copy i32 %a
  %c = copy i32 %b
  ret i32 %c
}
"""
    },
    "07_unconditional_jump_chain": {
        "input": """func @t(i32 %arg0) -> i32 {
bb0:
  jmp bb1
bb1:
  jmp bb2
bb2:
  ret i32 %arg0
}
""",
        "expected": """func @t(i32 %arg0) -> i32 {
bb0:
  jmp bb1
bb1:
  jmp bb2
bb2:
  ret i32 %arg0
}
"""
    },
    "08_nested_branch_pruning": {
        "input": """func @t() -> i32 {
bb0:
  %c1 = const i1 1
  br i1 %c1, bb1, bb2
bb1:
  %c2 = const i1 0
  br i1 %c2, bb3, bb4
bb2:
  %v2 = const i32 99
  jmp bb5
bb3:
  %v3 = const i32 11
  jmp bb5
bb4:
  %v4 = const i32 22
  jmp bb5
bb5:
  %p = phi i32 [bb2: %v2], [bb3: %v3], [bb4: %v4]
  ret i32 %p
}
""",
        "expected": """func @t() -> i32 {
bb0:
  %c1 = const i1 1
  jmp bb1
bb1:
  %c2 = const i1 0
  jmp bb4
bb2:
bb3:
bb4:
  %v4 = const i32 22
  jmp bb5
bb5:
  %p = const i32 22
  ret i32 %p
}
"""
    },
    "09_and_zero_branch_prune": {
        "input": """func @t(i32 %arg0) -> i32 {
bb0:
  %c0 = const i32 0
  %and = and i32 %arg0, %c0
  %cond = icmp ne i32 %and, %c0
  br i1 %cond, bb1, bb2
bb1:
  %v1 = const i32 100
  jmp bb3
bb2:
  %v2 = const i32 200
  jmp bb3
bb3:
  %p = phi i32 [bb1: %v1], [bb2: %v2]
  ret i32 %p
}
""",
        "expected": """func @t(i32 %arg0) -> i32 {
bb0:
  %c0 = const i32 0
  %and = const i32 0
  %cond = const i1 0
  jmp bb2
bb1:
bb2:
  %v2 = const i32 200
  jmp bb3
bb3:
  %p = const i32 200
  ret i32 %p
}
"""
    },
    "10_or_zero_sccp_prop": {
        "input": """func @t() -> i32 {
bb0:
  %x = const i32 42
  %c0 = const i32 0
  %y = or i32 %x, %c0
  ret i32 %y
}
""",
        "expected": """func @t() -> i32 {
bb0:
  %x = const i32 42
  %c0 = const i32 0
  %y = const i32 42
  ret i32 %y
}
"""
    },
    "11_shift_zero_sccp_prop": {
        "input": """func @t() -> i32 {
bb0:
  %x = const i32 5
  %c0 = const i32 0
  %y = shl i32 %x, %c0
  ret i32 %y
}
""",
        "expected": """func @t() -> i32 {
bb0:
  %x = const i32 5
  %c0 = const i32 0
  %y = const i32 5
  ret i32 %y
}
"""
    },
    "12_multiple_phi_collapsing": {
        "input": """func @t(i32 %arg0, i32 %arg1) -> i32 {
bb0:
  %c = const i1 1
  br i1 %c, bb1, bb2
bb1:
  %v1 = add i32 %arg0, %arg1
  jmp bb3
bb2:
  %v2 = sub i32 %arg0, %arg1
  jmp bb3
bb3:
  %p1 = phi i32 [bb1: %v1], [bb2: %v2]
  %p2 = phi i32 [bb1: %arg0], [bb2: %arg1]
  %res = add i32 %p1, %p2
  ret i32 %res
}
""",
        "expected": """func @t(i32 %arg0, i32 %arg1) -> i32 {
bb0:
  %c = const i1 1
  jmp bb1
bb1:
  %v1 = add i32 %arg0, %arg1
  jmp bb3
bb2:
bb3:
  %res = add i32 %v1, %arg0
  ret i32 %res
}
"""
    },
    "13_sccp_eq_constants_prune": {
        "input": """func @t() -> i32 {
bb0:
  %a = const i32 100
  %b = const i32 100
  %cond = icmp eq i32 %a, %b
  br i1 %cond, bb1, bb2
bb1:
  %v1 = const i32 1
  jmp bb3
bb2:
  %v2 = const i32 2
  jmp bb3
bb3:
  %p = phi i32 [bb1: %v1], [bb2: %v2]
  ret i32 %p
}
""",
        "expected": """func @t() -> i32 {
bb0:
  %a = const i32 100
  %b = const i32 100
  %cond = const i1 1
  jmp bb1
bb1:
  %v1 = const i32 1
  jmp bb3
bb2:
bb3:
  %p = const i32 1
  ret i32 %p
}
"""
    },
    "14_and_one_sccp_prop": {
        "input": """func @t() -> i32 {
bb0:
  %x = const i32 1
  %c1 = const i32 1
  %y = and i32 %x, %c1
  ret i32 %y
}
""",
        "expected": """func @t() -> i32 {
bb0:
  %x = const i32 1
  %c1 = const i32 1
  %y = const i32 1
  ret i32 %y
}
"""
    },
    "15_loop_latch_identity_fold": {
        "input": """func @t(i32 %arg0) -> i32 {
bb0:
  %zero = const i32 0
  jmp bb1
bb1:
  %i = phi i32 [bb0: %zero], [bb2: %i_next]
  %cond = icmp lt i32 %i, %arg0
  br i1 %cond, bb2, bb3
bb2:
  %zero_add = const i32 0
  %i_add = add i32 %i, %zero_add
  %one = const i32 1
  %i_next = add i32 %i_add, %one
  jmp bb1
bb3:
  ret i32 %i
}
""",
        "expected": """func @t(i32 %arg0) -> i32 {
bb0:
  %zero = const i32 0
  jmp bb1
bb1:
  %i = phi i32 [bb0: %zero], [bb2: %i_next]
  %cond = icmp lt i1 %i, %arg0
  br i1 %cond, bb2, bb3
bb2:
  %zero_add = const i32 0
  %i_add = copy i32 %i
  %one = const i32 1
  %i_next = add i32 %i_add, %one
  jmp bb1
bb3:
  ret i32 %i
}
"""
    },
    "16_phi_elimination_dead_block": {
        "input": """func @t(i32 %arg0) -> i32 {
bb0:
  %c = const i1 0
  br i1 %c, bb1, bb2
bb1:
  %v1 = const i32 50
  jmp bb3
bb2:
  %v2 = const i32 60
  jmp bb3
bb3:
  %p = phi i32 [bb1: %v1], [bb2: %v2]
  ret i32 %p
}
""",
        "expected": """func @t(i32 %arg0) -> i32 {
bb0:
  %c = const i1 0
  jmp bb2
bb1:
bb2:
  %v2 = const i32 60
  jmp bb3
bb3:
  %p = const i32 60
  ret i32 %p
}
"""
    },
    "17_loop_unreachable_prune": {
        "input": """func @t() -> i32 {
bb0:
  %c = const i1 0
  br i1 %c, bb1, bb2
bb1:
  %x = const i32 10
  jmp bb1
bb2:
  %y = const i32 20
  ret i32 %y
}
""",
        "expected": """func @t() -> i32 {
bb0:
  %c = const i1 0
  jmp bb2
bb1:
bb2:
  %y = const i32 20
  ret i32 %y
}
"""
    },
    "18_cond_move_phi_fold": {
        "input": """func @t() -> i32 {
bb0:
  %c = const i1 1
  br i1 %c, bb1, bb2
bb1:
  %v1 = const i32 10
  jmp bb3
bb2:
  %v2 = const i32 20
  jmp bb3
bb3:
  %p = phi i32 [bb1: %v1], [bb2: %v2]
  ret i32 %p
}
""",
        "expected": """func @t() -> i32 {
bb0:
  %c = const i1 1
  jmp bb1
bb1:
  %v1 = const i32 10
  jmp bb3
bb2:
bb3:
  %p = const i32 10
  ret i32 %p
}
"""
    },
    "19_xor_self_identity_fold": {
        "input": """func @t(i32 %arg0) -> i32 {
bb0:
  %zero = xor i32 %arg0, %arg0
  %res = add i32 %arg0, %zero
  ret i32 %res
}
""",
        "expected": """func @t(i32 %arg0) -> i32 {
bb0:
  %zero = const i32 0
  %res = copy i32 %arg0
  ret i32 %res
}
"""
    },
    "20_deep_iterative_chain": {
        "input": """func @t(i32 %arg0) -> i32 {
bb0:
  %c0 = const i32 0
  %cond1 = icmp eq i32 %c0, %c0
  br i1 %cond1, bb1, bb2
bb1:
  %v1 = add i32 %arg0, %c0
  %c1 = const i32 1
  %cond2 = icmp eq i32 %c0, %c1
  br i1 %cond2, bb3, bb4
bb2:
  %v2 = const i32 999
  jmp bb5
bb3:
  %v3 = const i32 888
  jmp bb5
bb4:
  %v4 = mul i32 %v1, %c1
  jmp bb5
bb5:
  %p = phi i32 [bb2: %v2], [bb3: %v3], [bb4: %v4]
  ret i32 %p
}
""",
        "expected": """func @t(i32 %arg0) -> i32 {
bb0:
  %c0 = const i32 0
  %cond1 = const i1 1
  jmp bb1
bb1:
  %v1 = copy i32 %arg0
  %c1 = const i32 1
  %cond2 = const i1 0
  jmp bb4
bb2:
bb3:
bb4:
  %v4 = copy i32 %v1
  jmp bb5
bb5:
  ret i32 %v4
}
"""
    }
}

os.makedirs("tests/opt/mixed", exist_ok=True)
for name, data in fixtures.items():
    dirpath = os.path.join("tests/opt/mixed", name)
    os.makedirs(dirpath, exist_ok=True)
    with open(os.path.join(dirpath, "input.ir"), "w", encoding="utf-8") as f:
        f.write(data["input"])
    with open(os.path.join(dirpath, "expected.ir"), "w", encoding="utf-8") as f:
        f.write(data["expected"])
    print(f"Generated {name}")
