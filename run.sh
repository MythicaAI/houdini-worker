#!/bin/bash

# Setup license server configuration
echo "ClientID=${SFX_CLIENT_ID}" > /root/houdini20.5/hserver.ini
echo "ClientSecret=${SFX_CLIENT_SECRET}" >> /root/houdini20.5/hserver.ini

# Start the license server
hserver -S https://www.sidefx.com/license/sesinetd

# Number of workers to spawn
NUM_WORKERS=${NUM_WORKERS:-3}
BASE_PORT=8765
WORKER_PIDS=()

# Start multiple workers
for ((i=0; i<$NUM_WORKERS; i++)); do
    PORT=$((BASE_PORT + i))
    # python3.11 /run/controller/main.py --worker /run/houdini_worker &
    /run/houdini_worker $PORT &
    WORKER_PIDS+=($!)
    echo "Started worker on port $PORT with PID ${WORKER_PIDS[-1]}"
done

# Cleanup function to release licenses
cleanup() {
    echo "Received shutdown signal, cleaning up..."
    for pid in "${WORKER_PIDS[@]}"; do
        kill $pid
    done
    hserver -Q
    echo "Finished cleanup."
}

trap cleanup TERM INT

# Wait for all worker processes
wait ${WORKER_PIDS[@]}