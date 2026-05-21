import sys
import os

with open("part2.c", "r") as f:
    lines = f.readlines()

def get_block(start_marker, end_marker, start_line_hint):
    for i in range(start_line_hint - 20, start_line_hint + 20):
        if start_marker in lines[i]:
            start = i
            break
    else:
        raise Exception(f"Could not find start marker {start_marker}")
    
    end = -1
    for i in range(start, len(lines)):
        if end_marker in lines[i]:
            end = i
            break
    if end == -1:
        raise Exception(f"Could not find end marker {end_marker}")
    
    return start, end

# 1. Number literals
n_start, n_end = get_block("/* number literals */", "cc->tk_text[1] = is_lng ? 'L' : 0;", 667)
n_end += 2

# 2. Char literals
c_start, c_end = get_block("/* character literal */", "cc->tk_val = val;", 758)
c_end += 2

# 3. String literals
s_start, s_end = get_block("/* string literal */", "        return;", 774)
s_end += 1

# 4. Identifiers
i_start, i_end = get_block("/* identifiers and keywords", "cc->tk_text[MAX_IDENT - 1] = 0;", 873)
i_end += 2

# 5. Operators
o_start, o_end = get_block("/* operators and delimiters */", "/* unknown character", 986)
o_end -= 1

def make_func(name, sig, lines_list):
    content = f"static void {name}({sig}) {{\n"
    for line in lines_list[1:-1]:
        content += line
    content += "}\n\n"
    return content

func_n = make_func("lex_number", "Compiler *cc, int c", lines[n_start+1:n_end])
func_c = make_func("lex_char", "Compiler *cc", lines[c_start+1:c_end])
func_s = make_func("lex_string", "Compiler *cc", lines[s_start+1:s_end])

func_i = "static int lex_ident(Compiler *cc) {\n    int kw;\n"
for line in lines[i_start+1:i_end][1:-1]:
    func_i += line.replace("goto again;", "return 0;")
func_i += "    return 1;\n}\n\n"

# Replace in next_token
new_lines = lines[:n_start]
new_lines.append("    if (is_digit(c)) { lex_number(cc, c); return; }\n")
new_lines.append("    if (c == '\\'') { lex_char(cc); return; }\n")
new_lines.append("    if (c == '\"') { lex_string(cc); return; }\n")
new_lines.append("    if (is_alpha(c)) { if (lex_ident(cc)) return; else goto again; }\n")
new_lines.append("    if (lex_operator(cc, c)) return;\n    goto again;\n")
new_lines.extend(lines[o_end+2:])

func_o = "static int lex_operator(Compiler *cc, int c) {\n"
for line in lines[o_start:o_end]:
    func_o += line.replace("return;", "return 1;")
func_o += "    return 0;\n}\n\n"

next_token_idx = -1
for i, line in enumerate(lines):
    if "void next_token(Compiler *cc) {" in line:
        next_token_idx = i
        break

final_lines = lines[:next_token_idx] + [func_n, func_c, func_s, func_i, func_o] + new_lines[next_token_idx:]

with open("part2.c", "w") as f:
    f.writelines(final_lines)

print("Split completed.")
