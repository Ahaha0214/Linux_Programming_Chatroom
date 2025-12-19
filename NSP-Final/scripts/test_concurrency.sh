#!/bin/bash

# Exit on error
set -e

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."

cd "$PROJECT_ROOT"

echo "========================================"
echo "   MMO Snake Game - Concurrency Test    "
echo "========================================"

# 1. Compile
echo "[*] Compiling project..."
make > /dev/null

# 2. Start Server
echo "[*] Starting Server..."
./server > server.log 2>&1 &
SERVER_PID=$!
echo "    Server PID: $SERVER_PID"

# Wait for server to initialize
sleep 1

# 3. Run Stress Test
echo "[*] Launching 100 Concurrent Clients (Stress Mode)..."
echo "    This will spawn 100 threads, each simulating a player."
echo "    Each player will send random moves and measure latency."
echo "----------------------------------------"

# Run client in stress mode
./client -stress

echo "----------------------------------------"
echo "[*] Test Finished."

# 4. Cleanup
echo "[*] Stopping Server..."
kill $SERVER_PID
wait $SERVER_PID 2>/dev/null || true

echo "[*] Done."
echo "    Check server.log for server-side output."
