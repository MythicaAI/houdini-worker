import json
import logging
import httpx
from httpx_ws import aconnect_ws
from pydantic import ValidationError

from models import ResolveForCookRequest

log = logging.getLogger(__name__)
# This function connects to the WebSocket server at 127.0.0.1:9876, sends an op:hello, and listens for resolve-for-cook requests
async def websocket_client(admin_ws_endpoint, resolve_queue):
    async def hello(websocket):
        await websocket.send(json.dumps({"op": "hello"}))  # Send op:hello
        log.info("Hello message sent. Waiting for messages...")

    async with httpx.AsyncClient() as client:
        async with aconnect_ws(admin_ws_endpoint, client) as websocket:
            log.info("Connected to Houdini worker at %s", admin_ws_endpoint)
            await hello(websocket)
            async for message in websocket.iter_text():
                try:
                    data = json.loads(message)
                    op = data.get("op")
                    if not op:
                        log.error("no 'op' in message")

                    if data.get("op") == "resolve-for-cook":
                        validated_request = ResolveForCookRequest(**data)
                        await resolve_queue.put(validated_request)
                        log.info("enqueued resolve-for-cook request: %s", validated_request)
                    else:
                        log.error("Unknown op: %s", op)
                except (ValidationError, json.JSONDecodeError) as e:
                    log.error(f"Invalid message received: {message}. Error: {e}")
