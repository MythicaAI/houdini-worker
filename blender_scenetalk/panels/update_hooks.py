import logging
import json

from ..async_wrap import run_async
from ..scenetalk_connection import get_client
from ..hda_db import find_by_name

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
    client = get_client()
    assert client is not None
    assert obj is not None
    
    hda_type = obj.get("hda_type", None)
    if hda_type is None:
        logger.error("Object has no HDA type")

    obj.hda.hda_params_json = json.dumps(params)

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