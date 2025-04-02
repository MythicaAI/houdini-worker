import asyncio
import base64
import json
import logging
from random import random
import secrets
import string
import time
from typing import Dict, Any, Optional, Tuple, Callable

import httpx
from httpx_ws import aconnect_ws

from .build_object import build_object

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("SceneTalk")


def process_response(response: Any) -> bool:
    """Generic SceneTalk response processor"""
    if response["op"] == "geometry":
        rand_str = ''.join(secrets.choice(string.ascii_lowercase) for i in range(10))
        obj_name = f"obj_{rand_str}"
        build_object(response, obj_name)
        return True
    completed = response["op"] == "automation" and \
        response["data"] == "end"
    return completed


class SceneTalkClient:
    """Minimal client for communicating with Houdini via WebSocket."""

    def __init__(self, host: str = "localhost", port: int = 8765):
        self.ws_url = f"ws://{host}:{port}"
        self.client = None
        self.websocket = None
        self.connected = False
        self.callbacks = {}
        self._task = None

    async def connect(self) -> bool:
        """Connect to the SceneTalk WebSocket server."""
        logger.info(f"Connecting to {self.ws_url}")
        try:
            self.client = httpx.AsyncClient(
                base_url=self.ws_url,
                timeout=5,
                limits=httpx.Limits(max_connections=10, max_keepalive_connections=5),
                http2=False,
            )

            # Start message listener
            self._task = asyncio.create_task(self._message_handler())
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
        self.connected = False

        if self._task:
            self._task.cancel()
            self._task = None

        if self.websocket:
            await self.websocket.close()
            self.websocket = None

        if self.client:
            await self.client.aclose()
            self.client = None

        logger.info("Disconnected")

        if "on_disconnect" in self.callbacks:
            self.callbacks["on_disconnect"]()

    def set_callbacks(self, callbacks: Dict[str, Callable]):
        """Set event callbacks."""
        self.callbacks = callbacks
    
    async def _test_crytals(self):
        crystals_param_schema = {
            "file_path": "assets/mythica.crystal.1.0.hda",
            "material_name": "crystal",
            "parameters": {
            "length": {
                "type": "slider",
                "label": "Length",
                "min": 0.5,
                "max": 5,
                "step": 0.1,
                "default": 2.5,
            },
            "radius": {
                "type": "slider",
                "label": "Radius",
                "min": 0.1,
                "max": 2,
                "step": 0.1,
                "default": 0.6,
            },
            "numsides": {
                "type": "slider",
                "label": "Sides",
                "min": 3,
                "max": 12,
                "step": 1,
                "default": 6,
            },
            },
        }

        # Extract parameters into a dictionary mapping label -> default value
        params = {param["label"]: param["default"] for param in crystals_param_schema["parameters"].values()}

        cook_message = {
            "op": "cook",
            "data": {
            "hda_path": {
                "file_id": "file_xxx",
                "file_path": crystals_param_schema['file_path']
            },
            "definition_index": 0,
            "format": "raw",
            **params  # Unpack all parameters
            }
        }

        
        await self.send_message(cook_message, process_response)
        logger.info("HDA cooked")

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
        success = await self.send_message(upload_message, process_response)
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
        success = await self.send_message(upload_message, process_response)
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
        await self.send_message(test_message, process_response)
        logger.info("HDA cooked")
        
    async def _message_handler(self):
        """Handle incoming WebSocket messages."""
        try:    
            async with aconnect_ws(self.ws_url, self.client) as ws:
                self.websocket = ws
                self.connected = True
                logger.info("Connected to %s", self.ws_url)
                if "on_connect" in self.callbacks:
                    self.callbacks["on_connect"]()
                await self._test_crytals()
                # await self._test_cube()

        except httpx.ReadError as e:
            logger.error(f"WebSocket read error: {e}")
            if "on_error" in self.callbacks:
                self.callbacks["on_error"](str(e))
        except asyncio.CancelledError:
            logger.info("Message handler cancelled")
            pass
        except Exception as e:
            logger.error(f"WebSocket error: {e}")
            if "on_error" in self.callbacks:
                self.callbacks["on_error"](str(e))
        finally:
            logger.info("Message handler disconnected")
            self.connected = False
            if "on_disconnect" in self.callbacks:
                self.callbacks["on_disconnect"]()

    def _process_message(self, data: Dict[str, Any]):
        """Process a message based on its operation type."""
        op_type = data.get("op")

        # Log the message type
        logger.debug(f"Received {op_type} message")

        # Dispatch to specific handler if available
        handler_name = f"on_{op_type}"
        if handler_name in self.callbacks:
            self.callbacks[handler_name](data.get("data"))

        # Also dispatch to the general message handler if available
        if "on_message" in self.callbacks:
            self.callbacks["on_message"](data)

    async def send_message(self, 
                           message: Dict[str, Any],
                           process_response: Callable[[Any], bool]) -> bool:
        """Send a message to the server."""
        if not self.websocket or not self.connected:
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
                    logger.info("Received response: %s", response_data)
                    completed = process_response(response_data)
                    if completed:
                        return True
                except asyncio.TimeoutError:
                    logger.error("Read timeout while waiting for data")
            return False
        except httpx.WriteError as e:
            logger.error(f"Error sending message: {e}")
            return False
        except Exception as e:
            logger.error(f"Send failed: {e}")
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
