import asyncio
import json
import logging
import httpx
from httpx_ws import aconnect_ws
from pydantic import ValidationError

from models import ResolveForCookRequest, ResolveFileRequest, AsyncWorkRequest

log = logging.getLogger(__name__)


async def hello(websocket):
    await websocket.send_text(json.dumps({"op": "hello"}))
    log.info("Hello message sent. Waiting for messages...")


async def send_responses(websocket, response_queue):
    while True:
        try:
            response = await response_queue.get()
            await websocket.send_text(json.dumps(response.model_dump()))
            log.info("Sent response: %s", response)
        except Exception as e:
            log.error(f"Error sending response: {e}")


async def receive_messages(websocket, resolve_queue):
    while True:
        try:
            message = await websocket.receive_text()
            data = json.loads(message)
            op = data.get("op")
            if not op:
                log.error("no 'op' in message")
                continue

            if op == "log":
                log_data = data.get("data")
                level = log_data.get("level")
                text = log_data.get("text")
                if level == "info":
                    log.info(text)
                elif level == "warning":
                    log.warning(text)
                elif level == "error":
                    log.error(text)
                else:
                    log.error("Unknown log level: %s for message: %s", level, text)
            elif op == "resolve-for-cook":
                validated_request = ResolveForCookRequest(**data)
                await resolve_queue.put(validated_request)
                log.info("enqueued resolve-for-cook request: %s", validated_request)
            elif op == "file_resolve":
                validated_request = ResolveFileRequest(**data)
                await resolve_queue.put(AsyncWorkRequest(validated_request))
                log.info("enqueued file_resolve request: %s", validated_request)
            else:
                log.error("Unknown op: %s %s", op, str(data))
        except (ValidationError, json.JSONDecodeError) as e:
            log.error(f"Invalid message received: {message}. Error: {e}")

async def try_connect(admin_ws_endpoint, client, resolve_queue, response_queue):
        async with aconnect_ws(admin_ws_endpoint, client) as websocket:
            log.info("Connected to Houdini worker at %s", admin_ws_endpoint)
            await hello(websocket)

            send_task = asyncio.create_task(send_responses(websocket, response_queue))
            receive_task = asyncio.create_task(receive_messages(websocket, resolve_queue))

            await asyncio.gather(send_task, receive_task)

# This function connects to the WebSocket server at 127.0.0.1:9876, sends an op:hello, and listens for resolve-for-cook requests
async def websocket_client(admin_ws_endpoint, resolve_queue, response_queue):
    retries = 20  # Maximum number of retries
    delay = 3  # Delay in seconds between retries
    for attempt in range(1, retries + 1):
        try:
            async with httpx.AsyncClient() as client:
                await try_connect(admin_ws_endpoint, client, resolve_queue, response_queue)
            break  # Exit the retry loop when successful
        except httpx.ConnectError as e:
            if attempt < retries:
                # Log the retry attempt and wait before retrying
                log.debug(f"Connection attempt {attempt} failed: {e}. Retrying in {delay} seconds...")
                await asyncio.sleep(delay)
            else:
                # Raise the exception after all retries are exhausted
                log.error(f"All {retries} connection attempts failed. Unable to connect.")
                raise