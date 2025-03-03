#!/bin/bash

# Setup license server configuration
echo "ClientID=${SFX_CLIENT_ID}" > /root/houdini20.5/hserver.ini
echo "ClientSecret=${SFX_CLIENT_SECRET}" >> /root/houdini20.5/hserver.ini

# Start the license server
hserver -S https://www.sidefx.com/license/sesinetd

PORT=8765
/run/houdini_worker $PORT &
WORKER_PID=$!

# Cleanup function to release licenses
cleanup() {
    echo "Received shutdown signal, cleaning up..."
    kill $WORKER_PID
    hserver -Q
    echo "Finished cleanup."
}

trap cleanup TERM INT

# Wait for the worker process instead of sleeping indefinitely
wait $WORKER_PID