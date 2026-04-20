import os

def create_large_arg_test():
    content = "#define BIG(x) x\nint test(void) {\n    return BIG(\n"
    content += " + ".join([str(i) for i in range(1200)]) + " + 0\n    );\n}\n"
    with open("/tmp/large_arg_test.c", "w") as f:
        f.write(content)

def create_large_body_test():
    content = "#define LARGE_BODY "
    content += " + ".join(["var_" + str(i) for i in range(2500)]) + " + 0\n"
    content += "int test(void) { return LARGE_BODY; }\n"
    with open("tests/pp/large_macro_body.c", "w") as f:
        f.write(content)

if __name__ == "__main__":
    create_large_arg_test()
    create_large_body_test()
