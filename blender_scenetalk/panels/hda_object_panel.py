import json
import bpy
import logging
from bpy.props import (
    FloatProperty, IntProperty, StringProperty, BoolProperty,
    PointerProperty, EnumProperty, CollectionProperty
)
from bpy.types import PropertyGroup
from ..hda_db import find_by_name
from ..scenetalk_connection import get_connection_state
from . import models
from .models import get_model_props, CrystalsProperties, RocksProperties, AgaveProperties, SaguaroProperties, RockifyProperties
from .update_hooks import recook, update_float_value, update_int_value, update_bool_value, update_string_value

logger = logging.getLogger("hda_object_panel")


def update_model_type(self, context):
    """Update the model type and refresh the UI"""
    obj = context.object
    obj['hda_type'] = self.model_type
    hda = getattr(obj, "hda", None)
    if not hda:
        return

    schema = find_by_name(self.model_type)
    if not schema:
        self.report({'ERROR'}, f"Model type {self.model_type} not found")
        return
    
    # Refresh the UI
    hda.set_from_schema(schema)
    context.area.tag_redraw()

    recook(context.object, hda.get_params())

class HDAInputRef(PropertyGroup):
    input_ref: PointerProperty(
        type=bpy.types.Object,
        name="Input",
        description="Object to use as input for HDA")
    
    # Input type/role in the PCG system
    input_type: EnumProperty(
        name="Input Type",
        description="How this object will be processed in the PCG system",
        items=[
            ('MESH', "Mesh", "Source of geometry data"),
            ('PATH', "Path", "Path for geometry placement"),
            ('VOLUME', "Volume", "Volume defining regions for PCG")
        ],
        default='MESH'
    )
    enabled: BoolProperty(
        name="Enabled",
        description="Control if input is actively used",
        default=True
    )

def validate_inputs(inputs):
    """Validate all inputs and remove any with invalid references"""
    to_remove = []
    
    # First identify indexes to remove (can't remove while iterating)
    for i, item in enumerate(inputs):
        if item.input_ref is None:
            to_remove.append(i)
        
        if item.input_ref not in bpy.data.objects:
            to_remove.append(i)
    
    # Remove in reverse order to maintain correct indices
    for i in sorted(to_remove, reverse=True):
        inputs.remove(i)
      
    # Return statistics about cleanup
    return len(to_remove)

def inputs_updated(inputs):
    validate_inputs(inputs)
    


# HDA Property Group - this will be attached to Blender objects
class HDAProperties(PropertyGroup):
    """HDA properties that can be attached to Blender objects."""

    enabled: BoolProperty(
        name="Enabled",
        description="Whether this HDA is enabled",
        default=True
    )

    hda_id: StringProperty(
        name="HDA ID",
        description="ID of the HDA to use",
        default=""
    )

    hda_path: StringProperty(
        name="HDA Path",
        description="Path to the HDA file",
        default="",
        subtype='FILE_PATH'
    )

    hda_name: StringProperty(
        name="HDA Name",
        description="Name of the HDA",
        default=""
    )

    material_name: StringProperty(
        name="Material Name",
        description="Name of the material to use",
        default=""
    )

    status: StringProperty(
        name="Status",
        description="Processing status",
        default="Not processed"
    )

    # Blender 4.2 supports asset preview - add this for future expansion
    asset_preview: StringProperty(
        name="Asset Preview",
        description="Path to the preview image for this HDA",
        default="",
        subtype='FILE_PATH'
    )

    # Store parameters as a JSON string for simplicity
    # This is a limitation of Blender's property system with nested collections
    hda_params_json: StringProperty(name="Parameters JSON")

    model_type: EnumProperty(
        name="Model Type",
        description="Select model type to generate",
        items=models.get_model_item_enum(),
        default='CRYSTALS',
        update=update_model_type
    )
    
    crystals_props: PointerProperty(type=CrystalsProperties)
    rocks_props: PointerProperty(type=RocksProperties)
    agave_props: PointerProperty(type=AgaveProperties)
    saguaro_props: PointerProperty(type=SaguaroProperties)
    rockify_props: PointerProperty(type=RockifyProperties)

    inputs: CollectionProperty(type=HDAInputRef)
    
    def set_from_schema(self, schema):

        """Initialize from schema data"""
        self.hda_id = "file_xxx"
        self.hda_name = "file_xxx".capitalize()
        self.hda_path = schema.get("file_path", "")
        self.material_name = schema.get("material_name", "")
        
        # Store parameter data as JSON
        params = schema.get("parameters", {})
        self.hda_params_json = json.dumps(params)

            
class HDA_PT_Panel(bpy.types.Panel):
    """HDA Panel in Object Properties"""
    bl_label = "SceneTalk PCG Asset"
    bl_idname = "HDA_PT_Panel"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}  # Collapsed by default

    @classmethod
    def poll(cls, context):
        show = context.object and context.object.get('hda_type', None)
        return show
    
    def draw_header(self, context):
        layout = self.layout
        obj = context.object
        if hasattr(obj, "hda"):
            layout.prop(obj.hda, "enabled", text="")

    def draw(self, context):
        obj = context.object
        hda_type = obj.get('hda_type', None)
        if not hda_type:
            return

        layout = self.layout
        layout.use_property_split = True  # Blender 4.2 style
        layout.use_property_decorate = False  # No animation

        # Connection status and controls
        box = layout.box()

        row = box.row()

        
        


classes = (
    HDAInputRef,
    HDAProperties,
    HDA_PT_Panel,
)

def register():
    models.register_all_models()
    for cls in classes:
        bpy.utils.register_class(cls)
    bpy.types.Object.hda = PointerProperty(type=HDAProperties)
    

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    models.unregister_all_models()

    # Remove the property group
    del bpy.types.Object.hda