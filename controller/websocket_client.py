import asyncio
import json
import logging
import httpx
from httpx_ws import aconnect_ws
from pydantic import ValidationError

from models import ResolveForCookRequest, ResolveFileRequest, AsyncWorkRequest

log = logging.getLogger(__name__)
# This function connects to the WebSocket server at 127.0.0.1:9876, sends an op:hello, and listens for resolve-for-cook requests
async def websocket_client(admin_ws_endpoint, resolve_queue, response_queue):
    async def hello(websocket):
        await websocket.send_text(json.dumps({"op": "hello"}))
        log.info("Hello message sent. Waiting for messages...")

    async def send_responses(websocket):
        while True:
            try:
                response = await response_queue.get()
                await websocket.send_text(json.dumps(response.model_dump()))
                log.info("Sent response: %s", response)
            except Exception as e:
                log.error(f"Error sending response: {e}")

    async def receive_messages(websocket):
        while True:
            try:
                message = await websocket.receive_text()
                data = json.loads(message)
                op = data.get("op")
                if not op:
                    log.error("no 'op' in message")
                    continue

                if op == "resolve-for-cook":
                    validated_request = ResolveForCookRequest(**data)
                    await resolve_queue.put(validated_request)
                    log.info("enqueued resolve-for-cook request: %s", validated_request)
                if op == "file_resolve":
                    validated_request = ResolveFileRequest(**data)
                    await resolve_queue.put(AsyncWorkRequest(validated_request))
                    log.info("enqueued file_resolve request: %s", validated_request)
                else:
                    log.error("Unknown op: %s %s", op, str(data))
            except (ValidationError, json.JSONDecodeError) as e:
                log.error(f"Invalid message received: {message}. Error: {e}")

    async with httpx.AsyncClient() as client:
        async with aconnect_ws(admin_ws_endpoint, client) as websocket:
            log.info("Connected to Houdini worker at %s", admin_ws_endpoint)
            await hello(websocket)

            send_task = asyncio.create_task(send_responses(websocket))
            receive_task = asyncio.create_task(receive_messages(websocket))

            await asyncio.gather(send_task, receive_task)
