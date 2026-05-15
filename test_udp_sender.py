import socket
import struct
import time
import math

# ZKAEDI UDP Sender Tester
UDP_IP = "127.0.0.1"
UDP_PORT = 5005

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

print(f"[ZKAEDI UDP TEST] Blasting mock 5-float Hamiltonian parameters to {UDP_IP}:{UDP_PORT}")
print("Press Ctrl+C to stop.")

try:
    start_time = time.time()
    while True:
        t = time.time() - start_time
        
        # Mock values mimicking intense audio / Hamiltonian fields
        bass = max(0.0, math.sin(t * 8.0)) * 1.5 # Rhythmic bass drops
        treble = (math.sin(t * 15.0) + 1.0) / 2.0 # Fast treble
        phase = t * 2.0 # Increasing phase to drive structural wave
        chaos = 1.0 + math.sin(t * 0.5) * 0.8 # Slow oscillation for chaos

        # Pack 5 floats (time, bass, treble, phase, chaos) -> 20 bytes
        packet = struct.pack("fffff", t, bass, treble, phase, chaos)
        sock.sendto(packet, (UDP_IP, UDP_PORT))
        
        time.sleep(1/60.0) # 60 Hz simulation

except KeyboardInterrupt:
    print("\n[ZKAEDI UDP TEST] Transmission stopped.")
