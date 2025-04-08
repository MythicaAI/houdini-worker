import logging
import json
import threading
import time
import bpy

from ..async_wrap import run_async
from ..scenetalk_connection import get_client
from ..hda_db import find_by_name

# Global variables to handle debouncing
_timer = None
_last_update_time = 0
_debounce_delay = 0.1  # Delay in seconds
_pending_value = None
_update_lock = threading.Lock()

logger = logging.getLogger("update_hooks")

def update_float_value(self, context):
    recook(context.object, self.get_params())

def update_int_value(self, context):
    recook(context.object, self.get_params())

def update_bool_value(self, context):
    recook(context.object, self.get_params())

def update_string_value(self, context):
    recook(context.object, self.get_params())

def recook(obj, params):
    global _last_update_time, _pending_value, _timer
    
    # Store the current value
    with _update_lock:
        _pending_value = (obj, params,)
        _last_update_time = time.time()
    
    # If no timer is running, start one
    if _timer is None:
        _timer = threading.Timer(_debounce_delay, recook_timer)
        _timer.start()


def recook_timer():
    global _timer, _pending_value, _last_update_time
    
    current_time = time.time()
    
    with _update_lock:
        # Check if enough time has passed since the last update
        if current_time - _last_update_time >= _debounce_delay:
            # Time has passed, send the request with the latest value
            obj, params = _pending_value
            _pending_value = None
            _timer = None
            
            # Execute in the main thread to avoid threading issues
            bpy.app.timers.register(
                lambda: recook_internal(obj, params),
                first_interval=0.0)
        else:
            # Not enough time has passed, restart the timer
            _timer = threading.Timer(_debounce_delay, recook_timer)
            _timer.start()


def recook_internal(obj, params):
    client = get_client()
    assert client is not None
    assert obj is not None
    
    hda_type = obj.get("hda_type", None)
    if hda_type is None:
        logger.error("Object has no HDA type")

    obj.hda.hda_params_json = json.dumps(params)
    obj.hda.status = "cooking"

    schema = find_by_name(hda_type)
    if not schema:
        logger.error("Schema not found for %s", hda_type)
        return
    
    default_params = schema["parameters"]

    # Extract parameters into a dictionary mapping label -> default value
    all_params = {param_id: param["default"]
                for param_id, param in default_params.items()}
    all_params.update(params)

    run_async(
        client.send_cook(
            hda_type,
            obj.name,
            all_params))