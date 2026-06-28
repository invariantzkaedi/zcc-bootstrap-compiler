#!/usr/bin/env python3
import json
import subprocess
import os
import sys

class ASTFeatureExtractor:
    def __init__(self):
        self.reset_counts()

    def reset_counts(self):
        # Raw counts of AST nodes
        self.raw_counts = {
            "node_count": 0.0,
            "call_count": 0.0,
            "assign_count": 0.0,
            "compound_assign_count": 0.0,
            "add_sub_count": 0.0,
            "mul_div_mod_count": 0.0,
            "cmp_count": 0.0,
            "logic_count": 0.0,
            "bitwise_count": 0.0,
            "deref_count": 0.0,
            "addr_count": 0.0,
            "member_count": 0.0,
            "cast_count": 0.0,
            "if_count": 0.0,
            "loop_count": 0.0,
            "switch_case_count": 0.0,
            "jump_count": 0.0,
            "num_lit_count": 0.0,
            "float_lit_count": 0.0,
            "str_char_lit_count": 0.0,
            "var_count": 0.0,
            "inc_dec_count": 0.0,
            "unsigned_type_count": 0.0,
            "long_int_type_count": 0.0,
            "double_type_count": 0.0,
            "ptr_to_ptr_type_count": 0.0,
            "statement_count": 0.0,
            "ptr_vars": 0.0,
            "unsigned_vars": 0.0,
            "pointer_arith_count": 0.0,
            
            # Semantic Vulnerability Features
            "memcpy_size_gt_dest": 0.0,
            "unsafe_str_call": 0.0,          # any strcpy/strcat/gets/sprintf/vsprintf call
            "stack_buf_with_unsafe_copy": 0.0, # local array dest + unsafe copy call
            "integer_arith_without_bounds_check": 0.0,
            "sizeof_pointer_used_as_buffer": 0.0,
            "array_index_without_bounds_check": 0.0,
            "malloc_size_less_than_copy_size": 0.0,
            "signed_to_unsigned_compare": 0.0,
            "null_checked_before_deref": 0.0
        }
        self.max_depth = 0
        self.num_params = 0
        self.var_malloc_sizes = {}   # heap: malloc/calloc → variable name → byte size
        self.stack_alloc_sizes = {}  # stack: alloca/local-array → variable name → byte size
        self.enclosing_ifs = []      # Tracks conditional nodes we are currently inside

    def extract_functions_from_file(self, zcc_path, c_file, extra_args=None):
        """Runs zcc to get the AST JSON and parses function subtrees"""
        try:
            if extra_args is None:
                extra_args = []
            if os.name == 'nt':
                wsl_zcc = zcc_path.replace('\\', '/').replace('H:', '/mnt/h').replace('h:', '/mnt/h')
                wsl_c = c_file.replace('\\', '/').replace('H:', '/mnt/h').replace('h:', '/mnt/h')
                wsl_extra = [a.replace('\\', '/').replace('H:', '/mnt/h').replace('h:', '/mnt/h') for a in extra_args]
                cmd = ["wsl", wsl_zcc, wsl_c] + wsl_extra + ["--dump-ast-json"]
            else:
                cmd = [zcc_path, c_file] + extra_args + ["--dump-ast-json"]
            
            res = subprocess.run(cmd, capture_output=True, text=True, check=True)
            
            stdout = res.stdout
            json_start = stdout.find('{"schema":')
            if json_start == -1:
                return {}
            
            ast_json = json.loads(stdout[json_start:])
            return self.extract_functions_from_json(ast_json)
        except Exception as e:
            import traceback
            print(f"[Extractor Error] Failed to parse {c_file}: {e}", file=sys.stderr)
            traceback.print_exc(file=sys.stderr)
            if hasattr(e, 'stdout') and e.stdout:
                print("STDOUT:\n", e.stdout, file=sys.stderr)
            if hasattr(e, 'stderr') and e.stderr:
                print("STDERR:\n", e.stderr, file=sys.stderr)
            return {}

    def extract_functions_from_json(self, ast_json):
        if not ast_json or "nodes" not in ast_json:
            return {}
        
        functions = {}
        for node in ast_json["nodes"]:
            if node.get("kind") == "ND_FUNC_DEF":
                fn_name = node.get("function", "anonymous")
                self.reset_counts()
                self.var_malloc_sizes = {}
                self.stack_alloc_sizes = {}
                self.enclosing_ifs = []
                
                # Setup parameters count
                params = node.get("params", [])
                self.num_params = len(params)
                
                # Recursively walk the function body
                body = node.get("children", {}).get("body")
                if body:
                    self._walk(body, 1)
                
                self.raw_counts["node_count"] = max(self.raw_counts["node_count"], 1.0)
                
                # Compute 32-dimensional layout with embedded semantic features
                features = [0.0] * 32
                raw = self.raw_counts
                n_count = raw["node_count"]
                v_count = max(raw["var_count"], 1.0)
                
                # 0: node_count (ratio of size to 100.0)
                features[0] = n_count / 100.0
                # 1: memcpy_size_gt_dest (SEMANTIC)
                features[1] = raw["memcpy_size_gt_dest"]
                # 2: call_count / max(statement_count, 1)
                features[2] = raw["call_count"] / max(raw["statement_count"], 1.0)
                # 3: unsafe_str_call — any inherently unbounded string call (CWE-121 primary signal)
                features[3] = raw["unsafe_str_call"]
                # 4: stack_buf_with_unsafe_copy — local array as dest of unsafe copy
                features[4] = raw["stack_buf_with_unsafe_copy"]
                # 5: arith_ops / max(comparison_ops, 1)
                arith_ops = raw["add_sub_count"] + raw["mul_div_mod_count"]
                features[5] = arith_ops / max(raw["cmp_count"], 1.0)
                # 6: integer_arith_without_bounds_check (SEMANTIC)
                features[6] = raw["integer_arith_without_bounds_check"]
                # 7: cmp_ratio
                features[7] = raw["cmp_count"] / n_count
                # 8: sizeof_pointer_used_as_buffer (SEMANTIC)
                features[8] = raw["sizeof_pointer_used_as_buffer"]
                # 9: array_index_without_bounds_check (SEMANTIC)
                features[9] = raw["array_index_without_bounds_check"]
                # 10: deref_count / max(guard_count, 1)
                features[10] = raw["deref_count"] / max(raw["if_count"], 1.0)
                # 11: malloc_size_less_than_copy_size (SEMANTIC)
                features[11] = raw["malloc_size_less_than_copy_size"]
                # 12: cast_count / max(node_count, 1)
                features[12] = raw["cast_count"] / n_count
                # 13: ptr_vars / max(var_count, 1)
                features[13] = raw["ptr_vars"] / v_count
                # 14: if_ratio
                features[14] = raw["if_count"] / n_count
                # 15: signed_to_unsigned_compare (SEMANTIC)
                features[15] = raw["signed_to_unsigned_compare"]
                # 16: null_checked_before_deref (SEMANTIC)
                features[16] = raw["null_checked_before_deref"]
                # 17: num_lit_ratio
                features[17] = raw["num_lit_count"] / n_count
                # 18: pointer_arith_ratio
                features[18] = raw["pointer_arith_count"] / n_count
                # 19: float_lit_ratio
                features[19] = raw["float_lit_count"] / n_count
                # 20: max_depth / 10.0
                features[20] = float(self.max_depth) / 10.0
                # 21: num_params / 5.0
                features[21] = float(self.num_params) / 5.0
                # 22: var_ratio
                features[22] = raw["var_count"] / n_count
                # 23: unsigned_type_ratio
                features[23] = raw["unsigned_type_count"] / n_count
                # 24: long_int_type_ratio
                features[24] = raw["long_int_type_count"] / n_count
                # 25: ptr_to_ptr_type_ratio
                features[25] = raw["ptr_to_ptr_type_count"] / n_count
                # 26: unsigned_vars / max(var_count, 1)
                features[26] = raw["unsigned_vars"] / v_count
                # 27: statement_ratio
                features[27] = raw["statement_count"] / 50.0
                
                # Standard counts for the rest to keep 32 dimensions
                # 28: loop_ratio
                features[28] = raw["loop_count"] / n_count
                # 29: switch_case_ratio
                features[29] = raw["switch_case_count"] / n_count
                # 30: bitwise_ratio
                features[30] = raw["bitwise_count"] / n_count
                # 31: member_ratio
                features[31] = raw["member_count"] / n_count
                
                functions[fn_name] = features
                
        return functions

    def _peel_casts(self, node):
        while node and isinstance(node, dict) and node.get("kind") == "ND_CAST":
            node = node.get("children", {}).get("lhs")
        return node if isinstance(node, dict) else None

    def _peel_addr_and_casts(self, node):
        while node and isinstance(node, dict) and node.get("kind") in ("ND_CAST", "ND_ADDR"):
            node = node.get("children", {}).get("lhs")
        return node if isinstance(node, dict) else None

    # Canonical byte sizes for scalar ZCC types
    _TYPE_SIZES = {
        "TY_CHAR": 1, "TY_UCHAR": 1,
        "TY_SHORT": 2, "TY_USHORT": 2,
        "TY_INT": 4, "TY_UINT": 4,
        "TY_LONG": 8, "TY_ULONG": 8,
        "TY_LONGLONG": 8, "TY_ULONGLONG": 8,
        "TY_FLOAT": 4, "TY_DOUBLE": 8, "TY_PTR": 8,
    }

    def _type_size(self, t):
        """Return byte size for a type node or kind string; 0 if unknown."""
        if not t:
            return 0
        if isinstance(t, str):
            return self._TYPE_SIZES.get(t, 0)
        if isinstance(t, dict):
            k = t.get("kind", "")
            if k == "TY_ARRAY":
                # array size is stored as the total byte size
                return float(t.get("size", 0))
            explicit = t.get("size")
            if explicit:
                return float(explicit)
            return self._TYPE_SIZES.get(k, 0)
        return 0

    def _eval_const_expr(self, node):
        """Statically evaluate a constant integer/sizeof expression.
        Handles: ND_NUM, ND_CAST, ND_SIZEOF_EXPR, ND_SIZEOF_TYPE,
                 ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_VAR (for local arrays).
        Returns 0.0 when the expression cannot be evaluated statically.
        """
        if not node or not isinstance(node, dict):
            return 0.0
        kind = node.get("kind")
        if kind == "ND_NUM":
            return float(node.get("int_val", 0))
        elif kind == "ND_CAST":
            return self._eval_const_expr(node.get("children", {}).get("lhs"))
        # sizeof(expr) and sizeof(type) — ZCC may emit ND_SIZEOF_EXPR or ND_SIZEOF
        elif kind in ("ND_SIZEOF_EXPR", "ND_SIZEOF"):
            inner = node.get("children", {}).get("lhs") or node.get("children", {}).get("expr")
            if inner and isinstance(inner, dict):
                t = inner.get("type") or inner.get("cast_type")
                sz = self._type_size(t)
                if sz:
                    return float(sz)
                # fallback: if lhs itself has a type.size field
                t2 = inner.get("type", {})
                if isinstance(t2, dict) and t2.get("size"):
                    return float(t2["size"])
            # sizeof(TypeName) — type node may be directly on the sizeof node
            t_direct = node.get("type") or node.get("sizeof_type")
            sz = self._type_size(t_direct)
            return float(sz) if sz else 0.0
        elif kind == "ND_ADD":
            lhs = self._eval_const_expr(node.get("children", {}).get("lhs"))
            rhs = self._eval_const_expr(node.get("children", {}).get("rhs"))
            return lhs + rhs
        elif kind == "ND_SUB":
            lhs = self._eval_const_expr(node.get("children", {}).get("lhs"))
            rhs = self._eval_const_expr(node.get("children", {}).get("rhs"))
            return lhs - rhs
        elif kind == "ND_MUL":
            lhs = self._eval_const_expr(node.get("children", {}).get("lhs"))
            rhs = self._eval_const_expr(node.get("children", {}).get("rhs"))
            return lhs * rhs
        elif kind == "ND_DIV":
            lhs = self._eval_const_expr(node.get("children", {}).get("lhs"))
            rhs = self._eval_const_expr(node.get("children", {}).get("rhs"))
            return lhs / rhs if rhs != 0 else 0.0
        elif kind == "ND_VAR":
            # Local array variable: use its declared type size as a constant
            v_name = node.get("name", "")
            t = node.get("type", {})
            if isinstance(t, dict) and t.get("kind") == "TY_ARRAY":
                return float(t.get("size", 0))
            # Check stack tracking
            if v_name in self.stack_alloc_sizes:
                return float(self.stack_alloc_sizes[v_name])
        return 0.0

    def _walk(self, node, depth):
        if not node or not isinstance(node, dict):
            return
        
        self.max_depth = max(self.max_depth, depth)
        self.raw_counts["node_count"] += 1.0
        
        kind = node.get("kind", "")
        ntype = node.get("type", {})
        tkind = ntype.get("kind", "") if ntype and isinstance(ntype, dict) else ""
        
        pushed_if = False
        # Track IF scopes to check bounds checks
        if kind == "ND_IF":
            self.raw_counts["if_count"] += 1.0
            cond = node.get("children", {}).get("cond")
            if cond:
                self.enclosing_ifs.append(cond)
                pushed_if = True
                
        elif kind == "ND_CALL":
            self.raw_counts["call_count"] += 1.0
            callee = node.get("callee", "")
            args = node.get("children", {}).get("args", [])
            
            # Track allocation sizes — heap and stack separated
            if callee in ("malloc", "calloc") and len(args) > 0:
                m_size = self._eval_const_expr(args[0])
                if callee == "calloc" and len(args) > 1:
                    m_size *= self._eval_const_expr(args[1])
                self.var_malloc_sizes["last_malloc"] = m_size
            elif callee == "alloca" and len(args) > 0:
                a_size = self._eval_const_expr(args[0])
                # Record in STACK map so downstream copy checks use it correctly
                self.stack_alloc_sizes["last_alloca"] = a_size
            
            # ── CWE-121 primary signals ─────────────────────────────────────
            # Inherently unbounded string functions (strcpy/strcat/gets/sprintf family)
            UNSAFE_STR_CALLS = (
                "strcpy", "strcat", "strncat",
                "gets", "gets_s",
                "sprintf", "vsprintf",
            )
            # Risk-qualified: scanf/fscanf only when format uses bare %s (no width)
            SCANF_CALLS = ("scanf", "fscanf", "sscanf", "vscanf", "vfscanf")

            is_risky_call = callee in UNSAFE_STR_CALLS

            # snprintf/vsnprintf: risky when size arg > dest buffer size
            if callee in ("snprintf", "vsnprintf") and len(args) >= 2:
                dest_sz_arg = args[1]  # second arg is the size limit
                limit = self._eval_const_expr(dest_sz_arg)
                dest_p = self._peel_addr_and_casts(args[0]) if args else None
                dest_buf_sz = 0.0
                if dest_p:
                    dt = dest_p.get("type", {})
                    if isinstance(dt, dict):
                        if dt.get("kind") == "TY_ARRAY":
                            dest_buf_sz = float(dt.get("size", 0))
                        elif dt.get("kind") == "TY_PTR":
                            v = dest_p.get("name", "")
                            dest_buf_sz = self.stack_alloc_sizes.get(v,
                                          self.var_malloc_sizes.get(v, 0.0))
                if limit > 0.0 and dest_buf_sz > 0.0 and limit > dest_buf_sz:
                    is_risky_call = True

            # scanf family: risky only if format string contains bare %s
            if callee in SCANF_CALLS:
                fmt_node = None
                # fscanf: fmt is arg[1]; others: arg[0]
                if callee in ("fscanf", "vfscanf") and len(args) >= 2:
                    fmt_node = args[1]
                elif len(args) >= 1:
                    fmt_node = args[0]
                if fmt_node and isinstance(fmt_node, dict):
                    if fmt_node.get("kind") in ("ND_STR", "ND_CHAR_LIT"):
                        fmt_val = fmt_node.get("str_val", fmt_node.get("val", ""))
                        import re as _re
                        # Bare %s with no width specifier → risky
                        if _re.search(r"%s", str(fmt_val)) and \
                           not _re.search(r"%[0-9]+s", str(fmt_val)):
                            is_risky_call = True

            if is_risky_call:
                self.raw_counts["unsafe_str_call"] = 1.0
                # Check if dest is a stack buffer (array or alloca pointer)
                dest_arg = args[0] if args else None
                if dest_arg:
                    dest_peeled = self._peel_addr_and_casts(dest_arg)
                    if dest_peeled:
                        dest_type = dest_peeled.get("type", {})
                        if dest_type and isinstance(dest_type, dict):
                            # Declared array → definitely stack
                            if dest_type.get("kind") == "TY_ARRAY":
                                self.raw_counts["stack_buf_with_unsafe_copy"] = 1.0
                            elif dest_type.get("kind") == "TY_PTR":
                                v_name = dest_peeled.get("name", "")
                                base = dest_type.get("base", {})
                                is_char_ptr = isinstance(base, dict) and \
                                              base.get("kind") in ("TY_CHAR", "TY_UCHAR")
                                # alloca pointer → stack; not in heap map → treat as stack
                                is_stack = (v_name in self.stack_alloc_sizes or
                                            v_name not in self.var_malloc_sizes)
                                if is_char_ptr and is_stack:
                                    self.raw_counts["stack_buf_with_unsafe_copy"] = 1.0

            # ── Copy-family bounds analysis ──────────────────────────────────
            COPY_CALLS = ("memcpy", "memmove", "strncpy", "strcpy", "strcat", "strncat")
            if callee in COPY_CALLS and len(args) >= 2:
                dest = args[0]
                dest_peeled = self._peel_addr_and_casts(dest)

                dest_size = 0.0
                if dest_peeled:
                    dest_type = dest_peeled.get("type", {})
                    if dest_type and isinstance(dest_type, dict):
                        if dest_type.get("kind") == "TY_ARRAY":
                            dest_size = float(dest_type.get("size", 0))
                        elif dest_type.get("kind") == "TY_PTR":
                            v_name = dest_peeled.get("name", "")
                            # Check stack first (alloca), then heap (malloc)
                            dest_size = self.stack_alloc_sizes.get(v_name,
                                        self.var_malloc_sizes.get(v_name, 0.0))
                        else:
                            dest_size = float(dest_type.get("size", 0))

                # Determine copy size
                copy_size = 0.0
                size_arg = None
                if callee in ("memcpy", "memmove", "strncpy", "strncat") and len(args) >= 3:
                    size_arg = args[2]
                    copy_size = self._eval_const_expr(size_arg)
                elif callee in ("strcpy", "strcat") and len(args) >= 2:
                    src_peeled = self._peel_addr_and_casts(args[1])
                    if src_peeled:
                        src_type = src_peeled.get("type", {})
                        if src_type and isinstance(src_type, dict):
                            copy_size = float(src_type.get("size", 0))

                # Fire memcpy_size_gt_dest
                if copy_size > 0.0 and dest_size > 0.0:
                    if copy_size > dest_size:
                        self.raw_counts["memcpy_size_gt_dest"] = 1.0
                elif dest_size > 0.0 and size_arg is not None and copy_size == 0.0:
                    # Dynamic size: flag if not bounds-checked
                    if not self._has_bounds_check_for_node(size_arg):
                        self.raw_counts["memcpy_size_gt_dest"] = 1.0

                    # sizeof(whole struct) used as copy size into a struct field
                    if dest_peeled and dest_peeled.get("kind") == "ND_MEMBER":
                        parent = dest_peeled.get("children", {}).get("lhs", {})
                        parent_peeled = self._peel_addr_and_casts(parent)
                        if parent_peeled and parent_peeled.get("kind") == "ND_DEREF":
                            parent_peeled = self._peel_addr_and_casts(
                                parent_peeled.get("children", {}).get("lhs", {}))
                        if parent_peeled:
                            parent_type = parent_peeled.get("type", {})
                            if parent_type and parent_type.get("kind") in ("TY_STRUCT", "TY_UNION"):
                                parent_size = float(parent_type.get("size", 0))
                                if copy_size == parent_size and parent_size > dest_size:
                                    self.raw_counts["integer_arith_without_bounds_check"] = 1.0

                # Heap malloc size mismatch
                if dest_peeled and dest_peeled.get("kind") == "ND_VAR":
                    v_name = dest_peeled.get("name", "")
                    m_size = self.var_malloc_sizes.get(v_name,
                             self.var_malloc_sizes.get("last_malloc", 0.0))
                    if m_size > 0.0 and copy_size > m_size:
                        self.raw_counts["malloc_size_less_than_copy_size"] = 1.0

            
        elif kind == "ND_ASSIGN":
            self.raw_counts["assign_count"] += 1.0
            lhs = node.get("children", {}).get("lhs", {})
            rhs = node.get("children", {}).get("rhs", {})
            
            # Map malloc size to assigned variable
            lhs_peeled = self._peel_addr_and_casts(lhs)
            rhs_peeled = self._peel_addr_and_casts(rhs)
            if lhs_peeled.get("kind") == "ND_VAR" and rhs_peeled:
                v_name = lhs_peeled.get("name", "")
                
                # Case 1: allocation call — separate stack (alloca) from heap (malloc/calloc)
                rhs_callee = rhs_peeled.get("callee") if rhs_peeled.get("kind") == "ND_CALL" else None
                if rhs_callee in ("malloc", "calloc", "alloca"):
                    # Evaluate size args DIRECTLY from rhs_peeled — do NOT use last_alloca/last_malloc
                    # sentinels, because ND_CALL children are walked AFTER the ASSIGN handler runs.
                    rhs_args = rhs_peeled.get("children", {}).get("args", [])
                    if rhs_callee == "alloca":
                        a_size = self._eval_const_expr(rhs_args[0]) if rhs_args else 0.0
                        if a_size > 0.0:
                            prev = self.stack_alloc_sizes.get(v_name, float('inf'))
                            self.stack_alloc_sizes[v_name] = min(prev, a_size)
                    else:
                        m_size = self._eval_const_expr(rhs_args[0]) if rhs_args else 0.0
                        if rhs_callee == "calloc" and len(rhs_args) > 1:
                            m_size *= self._eval_const_expr(rhs_args[1])
                        if m_size > 0.0:
                            prev_size = self.var_malloc_sizes.get(v_name, float('inf'))
                            self.var_malloc_sizes[v_name] = min(prev_size, m_size)

                            # Check if malloc size is misaligned with pointer base type
                            v_type = lhs_peeled.get("type", {})
                            if v_type and isinstance(v_type, dict) and v_type.get("kind") == "TY_PTR":
                                base_type = v_type.get("base", "")
                                base_size = float(self._type_size(base_type)) or 1.0
                                if base_size > 0.0:
                                    if m_size < base_size:
                                        self.raw_counts["sizeof_pointer_used_as_buffer"] = 1.0
                                    elif m_size % base_size != 0:
                                        self.raw_counts["sizeof_pointer_used_as_buffer"] = 1.0


                # Case 2: assigning another variable (size propagation)
                elif rhs_peeled.get("kind") == "ND_VAR":
                    r_name = rhs_peeled.get("name", "")
                    r_type = rhs_peeled.get("type", {})
                    if r_type and isinstance(r_type, dict):
                        if r_type.get("kind") == "TY_PTR":
                            # Check stack map first (alloca), then heap map (malloc)
                            if r_name in self.stack_alloc_sizes:
                                r_size = self.stack_alloc_sizes[r_name]
                                if r_size > 0.0:
                                    prev = self.stack_alloc_sizes.get(v_name, float('inf'))
                                    self.stack_alloc_sizes[v_name] = min(prev, r_size)
                            elif r_name in self.var_malloc_sizes:
                                r_size = self.var_malloc_sizes[r_name]
                                if r_size > 0.0:
                                    prev = self.var_malloc_sizes.get(v_name, float('inf'))
                                    self.var_malloc_sizes[v_name] = min(prev, r_size)
                        else:
                            r_size = float(r_type.get("size", 0.0))
                            if r_size > 0.0:
                                prev = self.var_malloc_sizes.get(v_name, float('inf'))
                                self.var_malloc_sizes[v_name] = min(prev, r_size)

            
        elif kind == "ND_COMPOUND_ASSIGN":
            self.raw_counts["compound_assign_count"] += 1.0
            # Check integer arithmetic without bounds checks
            op = node.get("compound_op", "")
            if op in ("ND_ADD", "ND_MUL"):
                if not self._has_bounds_check_for_node(node, check_type="upper"):
                    self.raw_counts["integer_arith_without_bounds_check"] = 1.0
            elif op == "ND_SUB":
                if not self._has_bounds_check_for_node(node, check_type="lower"):
                    self.raw_counts["integer_arith_without_bounds_check"] = 1.0
            else:
                if not self._has_bounds_check_for_node(node, check_type="both"):
                    self.raw_counts["integer_arith_without_bounds_check"] = 1.0
        elif kind in ("ND_ADD", "ND_FADD"):
            self.raw_counts["add_sub_count"] += 1.0
            if tkind == "TY_PTR":
                self.raw_counts["pointer_arith_count"] += 1.0
                if not self._has_bounds_check_for_node(node, check_type="upper"):
                    self.raw_counts["array_index_without_bounds_check"] = 1.0
            else:
                if not self._has_bounds_check_for_node(node, check_type="upper"):
                    self.raw_counts["integer_arith_without_bounds_check"] = 1.0
        elif kind in ("ND_SUB", "ND_FSUB"):
            self.raw_counts["add_sub_count"] += 1.0
            if tkind == "TY_PTR":
                self.raw_counts["pointer_arith_count"] += 1.0
                if not self._has_bounds_check_for_node(node, check_type="lower"):
                    self.raw_counts["array_index_without_bounds_check"] = 1.0
            else:
                if not self._has_bounds_check_for_node(node, check_type="lower"):
                    self.raw_counts["integer_arith_without_bounds_check"] = 1.0
        elif kind in ("ND_MUL", "ND_FMUL"):
            self.raw_counts["mul_div_mod_count"] += 1.0
            if not self._has_bounds_check_for_node(node, check_type="upper"):
                self.raw_counts["integer_arith_without_bounds_check"] = 1.0
        elif kind in ("ND_DIV", "ND_MOD", "ND_FDIV"):
            self.raw_counts["mul_div_mod_count"] += 1.0
            if not self._has_bounds_check_for_node(node, check_type="both"):
                self.raw_counts["integer_arith_without_bounds_check"] = 1.0
            
        elif kind in ("ND_EQ", "ND_NE", "ND_LT", "ND_LE", "ND_GT", "ND_GE"):
            self.raw_counts["cmp_count"] += 1.0
            
            # Signed vs Unsigned comparisons (CWE-682)
            lhs_peeled = self._peel_casts(node.get("children", {}).get("lhs", {}))
            rhs_peeled = self._peel_casts(node.get("children", {}).get("rhs", {}))
            if lhs_peeled and rhs_peeled:
                l_type = lhs_peeled.get("type", {}).get("kind", "")
                r_type = rhs_peeled.get("type", {}).get("kind", "")
                unsigned_types = ("TY_UCHAR", "TY_USHORT", "TY_UINT", "TY_ULONG", "TY_ULONGLONG")
                signed_types = ("TY_CHAR", "TY_SHORT", "TY_INT", "TY_LONG", "TY_LONGLONG")
                
                l_unsigned = l_type in unsigned_types
                r_unsigned = r_type in unsigned_types
                l_signed = l_type in signed_types
                r_signed = r_type in signed_types
                
                if (l_unsigned and r_signed) or (l_signed and r_unsigned):
                    self.raw_counts["signed_to_unsigned_compare"] = 1.0
                    
        elif kind in ("ND_LAND", "ND_LOR", "ND_LNOT"):
            self.raw_counts["logic_count"] += 1.0
        elif kind in ("ND_BAND", "ND_BOR", "ND_BXOR", "ND_BNOT", "ND_SHL", "ND_SHR"):
            self.raw_counts["bitwise_count"] += 1.0
            
        elif kind == "ND_DEREF":
            self.raw_counts["deref_count"] += 1.0
            
            # Check null checks before dereferencing (CWE-476)
            var_node = self._find_inner_var(node)
            if var_node:
                var_name = var_node.get("name", "")
                if var_name and self._has_null_check_for_var(var_name):
                    self.raw_counts["null_checked_before_deref"] = 1.0
                    
        elif kind == "ND_ADDR":
            self.raw_counts["addr_count"] += 1.0
        elif kind == "ND_MEMBER":
            self.raw_counts["member_count"] += 1.0
        elif kind == "ND_CAST":
            self.raw_counts["cast_count"] += 1.0
            # CWE-704: Incorrect pointer casts
            lhs_peeled = self._peel_casts(node.get("children", {}).get("lhs", {}))
            if lhs_peeled:
                l_type = lhs_peeled.get("type", {})
                c_type = node.get("cast_type", node.get("type", {}))
                if l_type and c_type and l_type.get("kind") == "TY_PTR" and c_type.get("kind") == "TY_PTR":
                    l_base = l_type.get("base", "")
                    c_base = c_type.get("base", "")
                    if l_base and c_base:
                        TYPE_SIZES = {
                            "TY_CHAR": 1.0, "TY_UCHAR": 1.0,
                            "TY_SHORT": 2.0, "TY_USHORT": 2.0,
                            "TY_INT": 4.0, "TY_UINT": 4.0,
                            "TY_LONG": 8.0, "TY_ULONG": 8.0,
                            "TY_LONGLONG": 8.0, "TY_ULONGLONG": 8.0,
                            "TY_PTR": 8.0, "TY_FLOAT": 4.0, "TY_DOUBLE": 8.0
                        }
                        l_size = TYPE_SIZES.get(l_base, 0.0) if isinstance(l_base, str) else float(l_base.get("size", 0)) if isinstance(l_base, dict) else 0.0
                        c_size = TYPE_SIZES.get(c_base, 0.0) if isinstance(c_base, str) else float(c_base.get("size", 0)) if isinstance(c_base, dict) else 0.0
                        
                        if l_size > 0.0 and c_size > 0.0 and c_size > l_size:
                            self.raw_counts["integer_arith_without_bounds_check"] = 1.0
                            
        elif kind in ("ND_WHILE", "ND_FOR", "ND_DO_WHILE"):
            self.raw_counts["loop_count"] += 1.0
        elif kind in ("ND_SWITCH", "ND_CASE", "ND_DEFAULT"):
            self.raw_counts["switch_case_count"] += 1.0
        elif kind in ("ND_BREAK", "ND_CONTINUE", "ND_GOTO", "ND_GOTO_COMPUTED", "ND_RETURN"):
            self.raw_counts["jump_count"] += 1.0
        elif kind == "ND_NUM":
            self.raw_counts["num_lit_count"] += 1.0
        elif kind == "ND_FLIT":
            self.raw_counts["float_lit_count"] += 1.0
        elif kind in ("ND_STR", "ND_CHAR_LIT"):
            self.raw_counts["str_char_lit_count"] += 1.0
        elif kind == "ND_VAR":
            self.raw_counts["var_count"] += 1.0
            if tkind == "TY_PTR":
                self.raw_counts["ptr_vars"] += 1.0
            if tkind in ("TY_UCHAR", "TY_USHORT", "TY_UINT", "TY_ULONG", "TY_ULONGLONG"):
                self.raw_counts["unsigned_vars"] += 1.0
        elif kind in ("ND_PRE_INC", "ND_POST_INC"):
            self.raw_counts["inc_dec_count"] += 1.0
            if not self._has_bounds_check_for_node(node, check_type="upper"):
                self.raw_counts["integer_arith_without_bounds_check"] = 1.0
        elif kind in ("ND_PRE_DEC", "ND_POST_DEC"):
            self.raw_counts["inc_dec_count"] += 1.0
            if not self._has_bounds_check_for_node(node, check_type="lower"):
                self.raw_counts["integer_arith_without_bounds_check"] = 1.0

        if ntype:
            if tkind in ("TY_UCHAR", "TY_USHORT", "TY_UINT", "TY_ULONG", "TY_ULONGLONG"):
                self.raw_counts["unsigned_type_count"] += 1.0
            if tkind in ("TY_LONG", "TY_ULONG", "TY_LONGLONG", "TY_ULONGLONG"):
                self.raw_counts["long_int_type_count"] += 1.0
            if tkind == "TY_DOUBLE":
                self.raw_counts["double_type_count"] += 1.0
            if tkind == "TY_PTR":
                base = ntype.get("base")
                if base == "TY_PTR":
                    self.raw_counts["ptr_to_ptr_type_count"] += 1.0

        # Walk children recursively
        children = node.get("children", {})
        if isinstance(children, dict):
            for child_key, child in children.items():
                if child_key == "stmts" and isinstance(child, list):
                    self.raw_counts["statement_count"] += len(child)
                    for item in child:
                        self._walk(item, depth + 1)
                elif isinstance(child, list):
                    for item in child:
                        self._walk(item, depth + 1)
                else:
                    self._walk(child, depth + 1)

        # Pop IF scopes when exiting
        if pushed_if:
            self.enclosing_ifs.pop()

    def _find_inner_var(self, node):
        """Recursively search for the first ND_VAR node in a subtree"""
        if not node or not isinstance(node, dict):
            return None
        if node.get("kind") == "ND_VAR":
            return node
        
        # Check children
        children = node.get("children", {})
        if isinstance(children, dict):
            for child in children.values():
                if isinstance(child, list):
                    for item in child:
                        v = self._find_inner_var(item)
                        if v: return v
                elif isinstance(child, dict):
                    v = self._find_inner_var(child)
                    if v: return v
        return None

    def _has_null_check_for_var(self, var_name):
        """Checks if var_name is involved in any active enclosing IF conditions"""
        for cond in self.enclosing_ifs:
            if self._var_in_condition(cond, var_name):
                return True
        return False

    def _var_in_condition(self, cond_node, var_name):
        if not cond_node or not isinstance(cond_node, dict):
            return False
        if cond_node.get("kind") == "ND_VAR" and cond_node.get("name") == var_name:
            return True
        
        children = cond_node.get("children", {})
        if isinstance(children, dict):
            for child in children.values():
                if isinstance(child, list):
                    for item in child:
                        if self._var_in_condition(item, var_name): return True
                elif isinstance(child, dict):
                    if self._var_in_condition(child, var_name): return True
        return False

    def _has_bounds_check_for_node(self, node, check_type="both"):
        """Checks if a pointer arithmetic addition node is bounded by active conditions"""
        idx_var = self._find_inner_var(node)
        if not idx_var:
            return True # No variable index to check bounds on (e.g. constant offsets)
            
        var_name = idx_var.get("name", "")
        var_type = idx_var.get("type", {})
        is_signed = var_type.get("kind", "") in ("TY_CHAR", "TY_SHORT", "TY_INT", "TY_LONG", "TY_LONGLONG")
        
        has_upper = False
        has_lower = False
        
        for cond in self.enclosing_ifs:
            if self._var_in_condition(cond, var_name):
                kind = cond.get("kind", "")
                if kind in ("ND_LT", "ND_LE", "ND_GT", "ND_GE", "ND_NE", "ND_EQ"):
                    lhs = cond.get("children", {}).get("lhs", {})
                    rhs = cond.get("children", {}).get("rhs", {})
                    
                    val = None
                    if lhs and isinstance(lhs, dict) and lhs.get("kind") == "ND_NUM":
                        val = float(lhs.get("int_val", 0))
                    elif rhs and isinstance(rhs, dict) and rhs.get("kind") == "ND_NUM":
                        val = float(rhs.get("int_val", 0))
                        
                    if val is not None and val <= 0.0:
                        has_lower = True
                    else:
                        has_upper = True
                        
        if check_type == "upper":
            return has_upper
        elif check_type == "lower":
            return has_lower
        else:
            if is_signed:
                return has_upper and has_lower
            else:
                return has_upper

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python extractor.py <zcc_path> <c_file> [extra_compiler_args...]")
        sys.exit(1)
    
    extractor = ASTFeatureExtractor()
    funcs = extractor.extract_functions_from_file(sys.argv[1], sys.argv[2], sys.argv[3:])
    for fn, feat in funcs.items():
        print(f"Function {fn}: {feat}")
