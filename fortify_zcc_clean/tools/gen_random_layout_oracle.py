def gen_nested_type(rng, index):
    nested_name = f"Nested{index}"
    decl = f"""
struct {nested_name} {{
    char n0;
    int n1;
}};
"""
    return decl, f"struct {nested_name}"

def gen_struct(rng, name, depth=0):
    count = rng.randint(1, 8)
    prelude = []
    lines = [f"struct {name} {{"]
    fields = []

    for i in range(count):
        field = f"f{i}"

        if depth < 2 and rng.random() < 0.20:
            nested_decl, nested_type = gen_nested_type(rng, f"{name}_{i}")
            prelude.append(nested_decl)
            ty = nested_type
        else:
            ty = field_type(rng, depth)

        lines.append(f"    {ty} {field};")
        fields.append(field)

    lines.append("};")

    return "\n".join(prelude + ["\n".join(lines)]), f"struct {name}", fields