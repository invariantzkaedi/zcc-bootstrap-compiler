import sys
import os
import time
import subprocess

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 verify_kernel.py <kernel_elf_path>")
        sys.exit(1)

    elf_path = sys.argv[1]
    if not os.path.exists(elf_path):
        print(f"Error: kernel ELF '{elf_path}' not found.")
        sys.exit(1)

    log_path = "serial.log"
    if os.path.exists(log_path):
        try:
            os.remove(log_path)
        except Exception:
            pass

    print(f"🚀 Booting {elf_path} in QEMU...")

    # Spawn QEMU subprocess redirecting COM1 to a local log file
    cmd = [
        "qemu-system-x86_64",
        "-m", "512M",
        "-smp", "2",
        "-nographic",
        "-kernel", elf_path,
        "-serial", f"file:{log_path}",
        "-monitor", "none"
    ]

    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    except Exception as e:
        print(f"❌ Failed to launch QEMU: {e}")
        print("Please verify that QEMU is installed under WSL via: sudo apt install -y qemu-system-x86")
        sys.exit(1)

    start_time = time.time()
    timeout = 8.0
    success = False

    print("⏳ Waiting for COM1 serial handshake marker...")

    while time.time() - start_time < timeout:
        # Check if subprocess exited prematurely
        exit_code = proc.poll()
        if exit_code is not None:
            # Subprocess terminated unexpectedly
            break

        # Read accumulated serial logs
        if os.path.exists(log_path):
            with open(log_path, "r", errors="ignore") as f:
                content = f.read()
                if "[ZKAEDI_V2_BOOT_SUCCESS]" in content:
                    success = True
                    break

        time.sleep(0.1)

    # Clean up QEMU process
    proc.terminate()
    try:
        proc.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        proc.kill()

    # Read final logs
    logs = ""
    if os.path.exists(log_path):
        with open(log_path, "r", errors="ignore") as f:
            logs = f.read()

    # Delete temp log file
    if os.path.exists(log_path):
        try:
            os.remove(log_path)
        except Exception:
            pass

    if success:
        print("----------------------------------------------------------------")
        print(logs.strip())
        print("----------------------------------------------------------------")
        print(f"✅ SUCCESS: {elf_path} successfully booted and verified COM1 serial handshake!")
        sys.exit(0)
    else:
        print("----------------------------------------------------------------")
        if logs:
            print(logs.strip())
        else:
            print("[No serial output captured]")
        print("----------------------------------------------------------------")
        print(f"❌ FAILURE: {elf_path} failed to boot or match handshake within {timeout}s.")
        sys.exit(1)

if __name__ == "__main__":
    main()
