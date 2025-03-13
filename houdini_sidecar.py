#!/usr/bin/env python3
import asyncio
import json
import websockets

# Modify this path as necessary to point to your Houdini-Worker executable.
HOUDINI_WORKER_CMD = [
    "/run/houdini_worker",
    "--client", "0.0.0.0:8765",
    "--privileged", "127.0.0.1:9876"
]

# This function launches the Houdini-Worker process.
async def launch_worker():
    print("Launching Houdini-Worker...")
    # Start the Houdini-Worker as an asynchronous subprocess.
    process = await asyncio.create_subprocess_exec(*HOUDINI_WORKER_CMD)
    # Optionally, you might want to monitor the process or log its output.
    await process.wait()  # This will wait until the process terminates.
    print("Houdini-Worker process has terminated.")
    return process

# This is the websocket handler that listens for op:package messages.
async def handle_websocket(websocket, path):
    print(f"New websocket connection from {websocket.remote_address}")
    try:
        async for message in websocket:
            print(f"Received message: {message}")
            try:
                data = json.loads(message)
            except json.JSONDecodeError:
                error_response = json.dumps({"status": "error", "message": "Invalid JSON"})
                await websocket.send(error_response)
                continue

            # Check for the op:package command.
            if data.get("op") == "package":
                # For now, we respond with a fixed directory path.
                package_path = "/local/package/directory"
                response = json.dumps({
                    "status": "ok",
                    "package_path": package_path
                })
                await websocket.send(response)
                print("Responded to op:package request.")
            else:
                response = json.dumps({
                    "status": "error",
                    "message": "Unknown op"
                })
                await websocket.send(response)
                print("Received unknown op.")
    except websockets.exceptions.ConnectionClosed:
        print("Websocket connection closed.")

# The main coroutine launches both the Houdini-Worker and the websocket server.
async def main():
    # Launch the Houdini-Worker process in the background.
    worker_task = asyncio.create_task(launch_worker())

    # Start the websocket server to listen for package operations.
    # Here we use port 8888; adjust as needed.
    ws_server = await websockets.serve(handle_websocket, "0.0.0.0", 8888)
    print("Houdini-sidecar started. Listening for package ops on ws://0.0.0.0:8888")

    # Run until the worker process terminates.
    await worker_task

    # Once the worker stops, close the websocket server.
    ws_server.close()
    await ws_server.wait_closed()

if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Houdini-sidecar terminated via keyboard interrupt.")
