import json
import bpy
import logging
from bpy.props import (
    FloatProperty, IntProperty, StringProperty, BoolProperty,
    PointerProperty, EnumProperty
)
from bpy.types import PropertyGroup
from ..hda_db import find_by_name
from ..scenetalk_connection import get_connection_state
from . import models
from .models import get_model_props, CrystalsProperties, RocksProperties, AgaveProperties, SaguaroProperties, RockifyProperties
from .update_hooks import recook, update_float_value, update_int_value, update_bool_value, update_string_value

logger = logging.getLogger("hda_object_panel")


# Base property group for a single parameter
class HDAParameter(bpy.types.PropertyGroup):
    """A single HDA parameter"""
    param_id: StringProperty(name="ID")
    label: StringProperty(name="Label")
    
    # Value properties - one for each supported type
    float_value: FloatProperty(name="Value", update=update_float_value)
    int_value: IntProperty(name="Value", update=update_int_value)
    bool_value: BoolProperty(name="Value", update=update_bool_value)
    
    # Value type
    value_type: StringProperty(
        name="Value Type",
        default="float",
        description="Type of value this parameter holds",
        update=update_string_value
    )
    
    # Range values
    min_value: FloatProperty(name="Min", default=0.0)
    max_value: FloatProperty(name="Max", default=1.0)
    step_value: FloatProperty(name="Step", default=0.1)
    
    # UI properties
    is_hidden: BoolProperty(name="Hidden", default=False)
    ui_type: StringProperty(name="UI Type", default="slider")
    
    def draw(self, layout):
        """Draw this parameter in the UI"""
        if self.is_hidden:
            return
            
        if self.ui_type == "slider":
            row = layout.row()
            
            if self.value_type == "float":
                row.prop(
                    self,
                    "float_value", 
                    text=self.label,
                    slider=True
                )
            elif self.value_type == "int":
                row.prop(
                    self, "int_value", 
                    text=self.label,
                    slider=True
                )
        elif self.ui_type == "checkbox":
            row = layout.row()
            row.prop(
                self, "bool_value",
                text=self.label,
            )
        
    def get_value(self):
        """Get this parameter's value"""
        if self.value_type == "float":
            return self.float_value
        elif self.value_type == "int":
            return self.int_value
        elif self.value_type == "bool":
            return self.bool_value
        return None

    def set_value(self, value):
        """Set this parameter's value based on type"""
        if isinstance(value, float):
            self.value_type = "float"
            self.float_value = value
            # self.float_value.set_range(self.min_value, self.max_value)
            # self.float_value.set_step(self.step_value)
        elif isinstance(value, int):
            self.value_type = "int"
            self.int_value = value
            # self.int_value.set_range(self.min_value, self.max_value)
            # self.int_value.set_step(self.step_value)
        elif isinstance(value, bool):
            self.value_type = "bool"
            self.bool_value = value

    def init_from_schema(self, param_id, param_def):
        """Initialize this parameter from schema definition"""
        # self.param_id = param_id
        
        # # Set label
        # if "label" in param_def:
        #     self.label = param_def["label"]
        # else:
        #     self.label = param_id.capitalize()
        
        # # Set UI type and hidden flag
        # self.ui_type = param_def.get("type", "slider")
        # self.is_hidden = (self.ui_type == "hidden")
        
        # # Set range for sliders
        # if self.ui_type == "slider":
        #     if "min" in param_def:
        #         self.min_value = float(param_def["min"])
        #     if "max" in param_def:
        #         self.max_value = float(param_def["max"])
        #     if "step" in param_def:
        #         self.step_value = float(param_def["step"])
        
        # Set default value
        if "default" in param_def:
            default = param_def["default"]
            self.set_value(default)

def update_model_type(self, context):
    """Update the model type and refresh the UI"""
    obj = context.object
    setattr(obj, 'hda_type', self.model_type)
    hda = getattr(obj, "hda", None)
    if not hda:
        return

    schema = find_by_name(self.model_type)
    if not schema:
        self.report({'ERROR'}, f"Model type {self.model_type} not found")
        return
    
    # Refresh the UI
    hda.set_from_schema(hda.hda_id, hda.hda_data)
    context.area.tag_redraw()

    recook(context.object, hda.get_params())

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
    
    def set_from_schema(self, hda_data):
        """Initialize from schema data"""
        self.hda_id = "file_xxx"
        self.hda_name = "file_xxx".capitalize()
        self.hda_path = hda_data.get("file_path", "")
        self.material_name = hda_data.get("material_name", "")
        
        # Store parameter data as JSON
        params = hda_data.get("parameters", {})
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

        # Use icons for connection status
        conn_state = get_connection_state()
        if conn_state == "connected":
            row.label(text="SceneTalk Status", icon='CHECKMARK')
            row = box.row()
            row.operator("scenetalk.disconnect", text="Disconnect", icon='CANCEL')
        elif conn_state == "connecting":
            row.label(text="Connecting...", icon='SORTTIME')
        elif conn_state.startswith("error"):
            row.label(text=get_connection_state(), icon='ERROR')
            row = box.row()
            row.operator("scenetalk.connect", text="Reconnect", icon='FILE_REFRESH')
        else:
            row.label(text="Server Disconnected", icon='X')
            row = box.row()
            row.operator("scenetalk.connect", text="Connect", icon='PLAY')

        box = layout.box()

        # Get the HDA property group
        hda = getattr(obj, "hda", None)
        if not hda:
            return
        
        # Draw model selector
        row = box.row()
        row.label(text="Model", icon='OBJECT_DATA')
        layout.prop(hda, "model_type")

        row = box.row()
        
        # Show properties based on selected model type
        props = get_model_props(hda, hda_type)
        box.label(text=hda_type + " Parameters:")   
        if props:
            props.draw(box)

        # File selection
        box = layout.box()
        box.label(text="HDA File", icon='FILE_BLEND')
        row = box.row()
        row.prop(hda, "hda_path", text="")

        # Status information
        box = layout.box()
        status_row = box.row()

        # Status with icon
        if hda.status == "Processing...":
            status_row.label(
                text="Status: Processing...", icon='SORTTIME')
        elif hda.status == "Processed":
            status_row.label(text="Status: Processed",
                                icon='CHECKMARK')
        elif hda.status.startswith("Error"):
            status_row.label(
                text=f"Status: {hda.status}", icon='ERROR')
        else:
            status_row.label(text=f"Status: {hda.status}", icon='INFO')
    
        schema = find_by_name(hda_type)
        if schema:
            pass

        # Parameters
        layout.separator()
        layout.label(text="Parameters:")

        # Here's where the dynamic UI happens
        # TODO: code gen the models and dynamically register/unregister them
        # call the draw method, this is required because, step size, min, max
        # and update can only be set during registration
        # param: HDAParameter = None
        # for param in list(hda.hda_params):
        #     param.draw(layout)

        # Init button
        row = layout.row()
        row.scale_y = 1.5  # Make button bigger
        row.operator("hda.init_params", text="Initialize",
                        icon='FILE_REFRESH')

        # Process button
        row = layout.row()
        row.scale_y = 1.5  # Make button bigger

        if get_connection_state() == "connected":
            row.operator("hda.process", text="Process HDA",
                            icon='MOD_HUE_SATURATION')
        else:
            row.operator("hda.process", text="Process HDA",
                            icon='MOD_HUE_SATURATION')


classes = (
    HDAParameter,
    HDAProperties,
    HDA_PT_Panel,
)

def register():
    models.register_all_models()

    bpy.utils.register_class(HDAParameter)
    bpy.utils.register_class(HDAProperties)
    bpy.utils.register_class(HDA_PT_Panel)

    bpy.types.Object.hda = PointerProperty(type=HDAProperties)
    

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

    models.unregister_all_models()

    # Remove the property group
    del bpy.types.Object.hda