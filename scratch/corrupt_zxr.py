import sys

data = open("record.zxr").read()
if len(sys.argv) > 1 and sys.argv[1] == "corrupt":
    data = data.replace('"topology_root": "b', '"topology_root": "c')
    print("Corrupted topology_root in record.zxr")
else:
    print("No corruption applied")
open("record.zxr", "w").write(data)
