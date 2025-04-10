import json

import bpy
import logging
from bpy.props import PointerProperty, StringProperty, BoolProperty
from ..panels.update_hooks import recook
from ..async_wrap import run_async_bg
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
    
    track_inputs: BoolProperty(
        name="track_inputs",
        description="Track inputs",
        default=False
    )

    def execute(self, context):
        model_type = context.scene.scenetalk_props.model_type
        schema = find_by_name(model_type)
        if not schema:
            self.report({'ERROR'}, f"Schema not found for {model_type}")
            return {'CANCELLED'}
        
        obj_name = random_obj_name(model_type)
        input_objects = []
        if self.track_inputs:
            logger.info("input objects %s", context.selected_objects)
            if len(context.selected_objects) == 0:
                self.report({'ERROR'}, "No input objects selected")
                return {'CANCELLED'}
            placeholder_mesh = bpy.data.meshes.new(f"{obj_name}_mesh")
            obj = bpy.data.objects.new(obj_name, placeholder_mesh)
            bpy.context.collection.objects.link(obj)

            obj["hda_type"] = model_type
            
            for sel in bpy.context.selected_objects:
                input_ref = obj.hda.inputs.add()
                input_ref.input_ref = sel
                input_ref.input_type = "MESH"
                input_ref.enabled = True

            input_objects.extend(bpy.context.selected_objects)

            # TODO: merge with upsert code
            obj.hda.model_type = model_type

            # HACK: Rotate the object so that Z is up, this is supposed to be
            # fixed by the OCS being transmitted as a property of the prim
            obj.rotation_euler = (1.5708, 0, 0)  # Rotate 90 degrees around the X-axis

            # Store the generator schema
            obj.hda.set_from_schema(schema)

            # Object status update
            obj.hda.status = "cooked"

            # Select the object
            bpy.context.view_layer.objects.active = obj
            obj.select_set(True)


        client = get_client()
        if not client:
            self.report({'ERROR'}, "No client initialized")
            return {'CANCELLED'}
        
        # TODO: extract geometry and upload as inputs to the scene

        # Extract parameters into a dictionary mapping label -> default value
        params = {param_id: param["default"]
                  for param_id, param in schema["parameters"].items()}

        run_async_bg(client.send_cook(
            model_type,
              obj_name,
              input_objects,
              params))
        
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


# Operator to add input objects
class HDA_OT_AddInput(bpy.types.Operator):
    bl_idname = "hda.add_input"
    bl_label = "Add Input Object"
    bl_description = "Add selected object as an input"
    bl_options = {'REGISTER', 'UNDO'}
    
    input: PointerProperty(type=bpy.types.Object)

    def execute(self, context):
        if context.object is None:
            self.report({'WARNING'}, "No object selected")
            return {'CANCELLED'}
        
        input = context.object.hda.inputs.add()
        input.input_ref = getattr(self, 'input', None)
        input.input_type = 'MESH'
        input.enabled = input.input_ref is not None
                
        return {'FINISHED'}

class HDA_OT_AddInputInteractive(bpy.types.Operator):
    bl_idname = "hda.add_input_interactive"
    bl_label = "Add Inputs"
    bl_description = "Add Input Objects Interactively"
    bl_options = {'REGISTER', 'UNDO'}

    waiting_for_input: BoolProperty(default=True)
    
    def execute(self, context):
        if not self.waiting_for_input:
            # Process the selected objects
            selected_objects = context.selected_objects
            if not selected_objects:
                self.report({'WARNING'}, "No objects selected")
                return {'CANCELLED'}
            
            # Do something with the selected objects
            self.report({'INFO'}, f"Processing {len(selected_objects)} objects")
            
            # Example: Move selected objects up by 1 unit
            for obj in selected_objects:
                obj.location.z += 1.0
            
            return {'FINISHED'}
        return {'RUNNING_MODAL'}
    
    def modal(self, context, event):
        if event.type == 'SPACE' and event.value == 'PRESS':
            # User pressed space, confirm selection
            self.waiting_for_input = False
            return self.execute(context)
        
        elif event.type == 'ESC':
            # User cancelled
            self.report({'INFO'}, "Cancelled")
            return {'CANCELLED'}
        
        return {'RUNNING_MODAL'}
    
    def invoke(self, context, event):
        self.waiting_for_input = True
        self.report({'INFO'}, "Select objects and press SPACEBAR to confirm")
        context.window_manager.modal_handler_add(self)
        return {'RUNNING_MODAL'}


# Operator to remove input objects
class HDA_OT_RemoveInput(bpy.types.Operator):
    bl_idname = "hda.remove_input"
    bl_label = "Remove PCG Input"
    bl_description = "Remove selected PCG input"
    bl_options = {'REGISTER', 'UNDO'}
    
    def execute(self, context):
        hda_type = context.object.get("hda_type", None)
        if not hda_type:
            self.report({'WARNING'}, "Object has no HDA")
            return {'CANCELLED'}
        
        last_index = len(context.object.hda.inputs) - 1
        context.object.hda.inputs.remove(last_index)
            
        return {'FINISHED'}


classes = (
    HDA_OT_AddToObject,
    HDA_OT_CreateModel,
    HDA_OT_Process,
    HDA_OT_InitParams,
    HDA_OT_AddInput,
    HDA_OT_AddInputInteractive,
    HDA_OT_RemoveInput)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)  # Unregister in reverse order


