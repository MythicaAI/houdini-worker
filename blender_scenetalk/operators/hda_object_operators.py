import json

import bpy
import logging
from bpy.props import PointerProperty, StringProperty
from ..panels.update_hooks import recook
from ..async_wrap import run_async
from ..scenetalk_client import random_obj_name
from ..scenetalk_connection import get_connection_state, get_client
from ..hda_db import find_by_name
from ..panels.models import get_model_props

logger = logging.getLogger("hda_object_operators")

# Operator to add HDA to object
class HDA_OT_AddToObject(bpy.types.Operator):
    """Add HDA to Object"""
    bl_idname = "hda.add_to_object"
    bl_label = "Add HDA"
    bl_options = {'REGISTER', 'UNDO'}

    def execute(self, context):
        obj = context.object

        if not obj:
            self.report({'ERROR'}, "No object selected")
            return {'CANCELLED'}

        # Add HDA property group to object
        obj.hda.enabled = True
        obj.hda.status = "Not processed"

        self.report({'INFO'}, "Added HDA to object")
        return {'FINISHED'}

# Operator to create the selected model
class HDA_OT_CreateModel(bpy.types.Operator):
    bl_idname = "hda.create_model"
    bl_label = "Create Model"
    bl_description = "Create the selected model type at the default location"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        model_type = context.scene.scenetalk_props.model_type
        schema = find_by_name(model_type)
        if not schema:
            self.report({'ERROR'}, f"Schema not found for {model_type}")
            return {'CANCELLED'}
        
        obj_name = random_obj_name()
        client = get_client()
        if not client:
            self.report({'ERROR'}, "No client initialized")
            return {'CANCELLED'}
        
        run_async(client.create_model(model_type, obj_name))
        
        return {'FINISHED'}
    
    
class HDA_OT_InitParams(bpy.types.Operator):
    bl_idname = "hda.init_params"
    bl_label = "Initialize HDA Parameters"
    bl_description = "Set up default HDA parameter values"
    bl_options = {'REGISTER'}

    def execute(self, context):
        obj = context.object
        if not obj:
            self.report({'ERROR'}, "No object selected")
            return {'CANCELLED'}
        
        hda_type = obj.get('hda_type', None)
        if not hda_type:
            hda_type = context.scene.scenetalk_props.model_type
            if not hda_type: 
                self.report({'WARNING'}, "Object has no HDA or parameters")
                return {'CANCELLED'}
            obj['hda_type'] = hda_type
        
        schema = find_by_name(hda_type)
        if not schema:
            self.report({'ERROR'}, f"Schema not found for {hda_type}")
            return {'CANCELLED'}

        hda = obj.hda
        props = get_model_props(hda, hda_type)
        if not props:
            return {'CANCELLED'}
        
        # TODO: pause updates while initializing
        # HACK: pass object data to inputX parameters
        
        # default values already handled in property creation
        for param_id, param_def in schema['parameters'].items():
            # Initialize the property with the default value
            if "default" in param_def:
                default = param_def["default"]
                setattr(props, param_id, default)
            else:
                continue

        obj.update_tag(refresh={'DATA'})
        return {'FINISHED'}
    
# Operator to process HDA
class HDA_OT_Process(bpy.types.Operator):
    """Process HDA"""
    bl_idname = "hda.process"
    bl_label = "Process HDA"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        # Only enable if connected and object has HDA
        obj = context.object
        is_connected = get_connection_state() == "connected"
        return obj.get("hda_type", None) and is_connected

    def execute(self, context):
        obj = context.object
        hda = obj.hda
        hda_type = obj.get("hda_type", None)
        assert hda_type is not None
        props = get_model_props(hda, hda_type)
        assert props is not None
        params = props.get_params()
        recook(obj, params)
        return {'FINISHED'}
    
classes = (
    HDA_OT_AddToObject,
    HDA_OT_CreateModel,
    HDA_OT_Process,
    HDA_OT_InitParams,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)  # Unregister in reverse order