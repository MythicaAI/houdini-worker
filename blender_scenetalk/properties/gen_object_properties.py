import json
import bpy
from bpy.types import PropertyGroup
from bpy.props import (PointerProperty, CollectionProperty, StringProperty, EnumProperty, BoolProperty)
from .models import get_model_props
from .models import CrystalsProperties, RocksProperties, AgaveProperties, SaguaroProperties, RockifyProperties
from ..cook import cook
from ..model_db import find_by_name
from . import models
from ..cook import cook

def update_model_type(self, context):
    """Update the model type and refresh the UI"""
    obj = context.object
    obj['model_type'] = self.model_type
    gen = getattr(obj, "gen", None)
    if not gen:
        return

    schema = find_by_name(self.model_type)
    if not schema:
        self.report({'ERROR'}, f"Model type {self.model_type} not found")
        return
    
    # Refresh the UI
    gen.set_from_schema(schema)
    context.area.tag_redraw()
    props = get_model_props(gen, self.model_type)
    cook(context.object, props.get_params())

class GenInputRef(PropertyGroup):
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


# Model generation properties
class GenProperties(PropertyGroup):
    """HDA properties that can be attached to Blender objects."""

    enabled: BoolProperty(
        name="Enabled",
        description="Whether generator is enabled",
        default=True
    )

    name: StringProperty(
        name="Name",
        description="Name of the generator",
        default=""
    )

    package_id: StringProperty(
        name="Package ID",
        description="Package ID to reference",
        default=""
    )

    file_name: StringProperty(
        name="File in package to use",
        description="File to reference",
        default=""
    )

    entry_point: StringProperty(
        name="Entry point",
        description="Entry point in file to use for generation",
        default="",
    )

    status: StringProperty(
        name="Status",
        description="Processing status",
        default="intialized"
    )

    material_name: StringProperty(
        name="Material Name",
        description="Name of the material to use",
        default=""
    )

    # Blender 4.2 supports asset preview - add this for future expansion
    asset_preview: StringProperty(
        name="Asset Preview",
        description="Path to the preview image",
        default="",
        subtype='FILE_PATH'
    )

    # Store parameters as a JSON string for simplicity
    # This is a limitation of Blender's property system with nested collections
    params_json: StringProperty(name="Parameters JSON")

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

    inputs: CollectionProperty(type=GenInputRef)
    
    def set_from_schema(self, schema):

        """Initialize from schema data"""
        self.name = 'internal'
        self.package_id = "file_package_internal"
        self.entry_point = "default"
        self.file_name = schema.get("file_path", "")
        self.material_name = schema.get("material_name", "")
        
        # Store parameter data as JSON
        params = schema.get("parameters", {})
        self.params_json = json.dumps(params)

classes = (
    GenInputRef,
    GenProperties,
)

def register():
        # Register all model properties
    models.register_all_models()
    for cls in classes:
        bpy.utils.register_class(cls)

    bpy.types.Object.gen = PointerProperty(type=GenProperties)

def unregister():
    for cls in classes:
        bpy.utils.unregister_class(cls)

    models.unregister_all_models()

    # Remove the property group
    del bpy.types.Object.gen