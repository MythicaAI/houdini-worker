import asyncio
import base64
from functools import partial
import json
import logging
from random import random
import secrets
import string
import sys
import contextlib
import os
from typing import Dict, Any, Optional, Tuple, Callable
from asyncio import Queue
from .hda_db import find_by_name

# Add the 'libs' folder to the Python path
libs_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "lib")
if libs_path not in sys.path:
    sys.path.append(libs_path)

import httpx
from httpx_ws import WebSocketNetworkError, aconnect_ws

# Configure logging
logging.basicConfig(level=logging.INFO)

logger = logging.getLogger("scenetalk_client")


def random_obj_name(model_type) -> str:
    """Generate a random object name."""
    rand_str = ''.join(secrets.choice(string.ascii_lowercase) for i in range(10))
    obj_name = f"{model_type.upper()}_{rand_str}"
    return obj_name


class SceneTalkClient:
    """Minimal client for communicating with Houdini via WebSocket."""

    def __init__(self, event_queue: Queue, host: str = "localhost", port: int = 8765):
        self.event_queue = event_queue
        self.ws_url = f"ws://{host}:{port}"
        self.client = None
        self.websocket = None
        self.async_stack = None
        self.connection_state = "disconnected"
        self._task = None
        # TODO: migrate to request context
        self.current_object_schema = None
        self.current_object_name = None
        self.current_model_type = None
        self.current_inputs = []

    async def process_response(self, response: Any) -> bool:
        """Generic SceneTalk response processor"""
        op_type = response['op']
        # logger.info(f"process_response: {op_type}")
        if op_type == "error":
            await self.event_queue.put(("error", response["data"]))
        elif op_type == "geometry":
            # TODO - decide on model or schema or job def language
            await self.event_queue.put(("geometry",
                                        self.current_model_type,
                                        self.current_object_name or random_obj_name(),
                                        self.current_inputs,
                                        response,
                                        self.current_object_schema))
        completed = op_type == "automation" and \
            response["data"] == "end"
        return completed

    async def connect(self, endpoint) -> bool:
        """Connect to the SceneTalk WebSocket server."""
        logger.info(f"Connecting to {endpoint}")
        try:
            self.client = httpx.AsyncClient(
                base_url=self.ws_url,
                timeout=5,
                limits=httpx.Limits(max_connections=10, max_keepalive_connections=5),
                http2=False,
            )

            # create the websocket connection object, it has to be wrapped
            # in an async stack to handle the async generator
            self.async_stack = contextlib.AsyncExitStack()
            self.websocket = await self.async_stack.enter_async_context(
                aconnect_ws(self.ws_url, self.client))   
            self.connection_state = "connected"
            self.ws_url = endpoint
            await self.event_queue.put(["connected", self.ws_url])
            return True
        
        except httpx.ConnectError as e:
            logger.error(f"Connection error: {e}")
            await self.disconnect()
            return False
        except Exception as e:
            logger.error(f"Failed to connect: {e}")
            await self.disconnect()
            return False

    async def disconnect(self):
        """Disconnect from the WebSocket server."""
        self.connection_state = "diconnecting"

        if self._task:
            self._task.cancel()
            self._task = None

        if self.websocket:
            await self.websocket.close()
            self.websocket = None

        if self.async_stack:
            await self.async_stack.aclose()
            self.async_stack = None

        if self.client:
            await self.client.aclose()
            self.client = None

        self.connection_state = "disconnected"
        await self.event_queue.put(["disconnected"])

    async def send_cook(self, model_type, obj_name, object_inputs, params):
        """Send a cook message to the server."""
        schema = find_by_name(model_type)
        assert schema is not None
        self.current_model_type = model_type
        self.current_object_schema = schema
        self.current_inputs = object_inputs
        
        if not self.websocket or self.connection_state != "connected":
            logger.error("Cannot send message: not connected")
            return False
        try:
            self.current_object_name = obj_name
            msg = {   
                "op": "cook",
                "data": {
                    "hda_path": {
                        "file_id": "file_local_hda",
                        "file_path": self.current_object_schema['file_path']
                    },
                    "definition_index": 0,
                    "format": "raw",
                    **params  # Unpack all parameters
                }
            }
            json_message = json.dumps(msg)
            await self.websocket.send_text(json_message)
            while True:
                try:
                    message = await asyncio.wait_for(
                        self.websocket.receive_text(),
                        timeout=30
                    )
                    response_data = json.loads(message)
                    completed = await self.process_response(response_data)
                    if completed:
                        logger.info("Cook completed")
                        return True
                except asyncio.TimeoutError:
                    logger.error("Read timeout while waiting for data")
                except WebSocketNetworkError:
                    logger.exception(f"WebSocket error")
                    await self.disconnect()
                    return False
            return False
        except httpx.WriteError as e:
            logger.error(f"Error sending message: {e}")
            return False 

    async def _test_cube(self):
        # Upload HDA file
        hda_file_id = "file_test_hda"
        hda_path = "../assets/test_cube.hda"
        base64_content = None
        with open(hda_path, "rb") as f:
            file_content = f.read()
            base64_content = base64.b64encode(file_content).decode('utf-8')

        upload_message = {
            "op": "file_upload",
            "data": {
                "file_id": hda_file_id,
                "content_type": "application/hda",
                "content_base64": base64_content
            }
        }
        logger.info("Uploading HDA file")
        success = await self.send_message(upload_message)
        assert success
        logger.info("HDA file uploaded")

        # Upload USDZ file
        input_file_id = "file_test_input0"
        input_path = "../assets/cube.usdz"
        base64_content = None
        with open(input_path, "rb") as f:
            file_content = f.read()
            base64_content = base64.b64encode(file_content).decode('utf-8')

        upload_message = {
            "op": "file_upload",
            "data": {
                "file_id": input_file_id,
                "content_type": "application/usdz",
                "content_base64": base64_content
            }
        }
        logger.info("Uploading USDZ file")
        success = await self.send_message(upload_message)
        assert success
        logger.info("USDZ file uploaded")


        # Cook HDA
        test_message = {
            "op": "cook",
            "data": {
                "hda_path": {
                    "file_id": hda_file_id,
                },
                "definition_index": 0,
                "format": "raw",
                "input0": {
                    "file_id": input_file_id,
                },
                "test_int": 5,
                "test_float": 2.0,
                "test_string": "test",
                "test_bool": True,
            }
        }
        await self.send_message(test_message)
        logger.info("HDA cooked")

    async def send_message(self, 
                           message: Dict[str, Any]) -> bool:
        """Send a message to the server."""
        if not self.websocket or self.connection_state != "connected":
            logger.error("Cannot send message: not connected")
            return False
        try:
            await self.websocket.send_text(json.dumps(message))
            while True:
                try:
                    message = await asyncio.wait_for(
                        self.websocket.receive_text(),
                        timeout=30
                    )
                    response_data = json.loads(message)
                    completed = await self.process_response(response_data)
                    if completed:
                        return True
                except asyncio.TimeoutError:
                    logger.error("Read timeout while waiting for data")
            return False
        except (httpx.WriteError, httpx.LocalProtocolError) as e:
            logger.error(f"Error sending message: {e}")
            return False

    async def upload_file(self, file_id: str, file_path: str, content_type: str = "application/octet-stream") -> bool:
        """Upload a file to the server."""
        try:
            with open(file_path, "rb") as f:
                file_content = f.read()
                base64_content = base64.b64encode(file_content).decode('utf-8')

            upload_message = {
                "op": "file_upload",
                "data": {
                    "file_id": file_id,
                    "content_type": content_type,
                    "content_base64": base64_content
                }
            }

            return await self.send_message(upload_message)

        except FileNotFoundError:
            logger.error(f"File not found: {file_path}")
            return False
        except Exception as e:
            logger.error(f"Error uploading file: {e}")
            return False

    async def cook_hda(self, hda_file_id: str, params: Dict[str, Any], input_files: Dict[str, str] = None) -> bool:
        """Cook an HDA with parameters and input files."""
        if input_files is None:
            input_files = {}

        # Format input files
        files_dict = {}
        for param_name, file_id in input_files.items():
            files_dict[param_name] = {"file_id": file_id}

        # Create cook message
        cook_message = {
            "op": "cook",
            "data": {
                "hda_path": {
                    "file_id": hda_file_id,
                },
                "definition_index": 0,
                "format": "raw",
                **files_dict,
                **params
            }
        }

        # Send cook request
        return await self.send_message(cook_message)
