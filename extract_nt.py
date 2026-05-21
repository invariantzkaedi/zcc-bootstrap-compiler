import sys

in_func = False
with open("zcc2.s") as f, open("next_token_extracted.s", "w") as out:
    for line in f:
        if line.startswith("next_token:"):
            in_func = True
        elif in_func and line.startswith(".size next_token"):
            out.write(line)
            break
        elif in_func and not line.startswith(" ") and not line.startswith("\t") and not line.startswith("."):
            if not line.startswith("next_token:"):
                break
        if in_func:
            out.write(line)
