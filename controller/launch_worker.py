import asyncio
import logging

from websocket_client import websocket_client

log = logging.getLogger(__name__)

# Modify this path as necessary to point to your Houdini-Worker executable.
def get_worker_cmd(exe_path, client_endpoint, admin_endpoint):
    return (
        exe_path,
        "--client", client_endpoint,
        "--admin", admin_endpoint
    )


async def launch_worker(worker_cmd, resolve_queue, shutdown_event):
    """This function launches the Houdini-Worker process."""
    log.info("launching %s...", worker_cmd)

    # Start the Houdini-Worker as an asynchronous subprocess.
    process = await asyncio.create_subprocess_exec(*worker_cmd)

    # Connect to the process
    ws_client_task = asyncio.create_task(websocket_client(resolve_queue)),

    # Optionally, you might want to monitor the process or log its output.
    await asyncio.gather(*[ws_client_task, process.wait()])

    log.info("process has terminated.")
    shutdown_event.set()
