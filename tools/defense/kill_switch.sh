#!/bin/bash
# ZKAEDI God's Eye Kill Switch
# Instantly fractures all IPC sockets, WebSocket connections, and kills the execution engines.

echo -e "\033[38;5;199m[CRITICAL OVERRIDE] EXECUTING GOD'S EYE KILL SWITCH\033[0m"

# 1. Sever the Python Orchestrators and Edge Brainstems
pkill -9 -f apex_orchestrator.py
pkill -9 -f ingestion_socket.py
pkill -9 -f flashbots_injector.py

# 2. Obliterate the IPC Pipes and UNIX Sockets
echo -e "\033[38;5;51m[FRACTURE]\033[0m Destroying local IPC matrices..."
rm -f /tmp/warden_syphon.sock
rm -f /tmp/sculptor_ipc.sock
rm -f /tmp/warden_tx.pipe

# 3. Flush the Local Network Cache
sudo iptables -A OUTPUT -p tcp --dport 8545 -j REJECT
echo -e "\033[38;5;51m[FRACTURE]\033[0m Mainnet RPC routing severed."

echo -e "\033[38;5;199m[SYSTEM SAFED] All MEV operations terminated. Funds are locked.\033[0m"
