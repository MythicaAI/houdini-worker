import bpy
import logging
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.app.handlers import persistent


# Import the SceneTalk client
from ..scenetalk_client import SceneTalkClient
from ..scenetalk_connection import get_connection_state, connect_to_server, disconnect_from_server
from ..hda_db import find_by_name

from . import models

logger = logging.getLogger("connection_panel")

# Connection properties
class SceneTalkProperties(bpy.types.PropertyGroup):
    """Connection properties for SceneTalk integration."""
    endpoint: StringProperty(
        name="Server Endpoint",
        description="SceneTalk server endpoint in the format ws://hostname:port",
        default="ws://localhost:8765"
    )
    
    auto_connect: BoolProperty(
        name="Auto Connect",
        description="Automatically connect on startup",
        default=False
    )

    show_connection_settings: BoolProperty(
        name="Show Connection Settings",
        description="Expand/collapse connection settings",
        default=False
    )

    # for creating models
    model_type: EnumProperty(
        name="Type",
        description="Select model type to generate",
        items=models.get_model_item_enum(),
        default='CRYSTALS',
    )

# Connection Panel
class SCENETALK_PT_ConnectionPanel(bpy.types.Panel):
    """SceneTalk Connection Panel"""
    bl_label = "SceneTalk Connection"
    bl_idname = "SCENETALK_PT_ConnectionPanel"
    bl_space_type = 'VIEW_3D'
    bl_region_type = 'UI'
    bl_category = 'SceneTalk'
    
    def draw(self, context):
        layout = self.layout
        props = context.scene.scenetalk_props
        
        # Connection endpoint
        box = layout.box()
        
        # Status display
        row = box.row()
        row.prop(props, "show_connection_settings", 
                 icon="TRIA_DOWN" if props.show_connection_settings else "TRIA_RIGHT",
                 icon_only=True, emboss=False)
       
        state = get_connection_state()
        if state == "connected":
            row.label(text="Connected", icon='CHECKMARK')
        elif state == "connecting":
            row.label(text="Connecting...", icon='SORTTIME')
        elif state == "disconnecting":
            row.label(text="Disconnecting...", icon='SORTTIME')
        elif state.startswith("error"):
            error_msg = state.split(":", 1)[1] if ":" in state else "Unknown error"
            row.label(text=f"Error: {error_msg}", icon='ERROR')
        else:
            row.label(text="Disconnected", icon='X')
        
        # Endpoint input
        if props.show_connection_settings:
            row = box.row()
            row.label(text="SceneTalk Endpoint:")
            row = box.row()
            row.prop(props, "endpoint")
            
            # Button row
            row = box.row(align=True)
            
            if state == "connected":
                row.operator("scenetalk.disconnect", text="Disconnect", icon='CANCEL')
            else:
                row.operator("scenetalk.connect", text="Connect", icon='PLAY')
                
            
            # Reconnect button - always visible but only enabled if not connected
            row = box.row()
            row.operator("scenetalk.reconnect", text="Reconnect", icon='FILE_REFRESH')
            
            # Auto-connect toggle
            row = box.row()
            row.prop(props, "auto_connect")

        # Show connected controls
        if state != "connected":
            return
        
        box = layout.box()
        row = box.row()
        row.prop(props, "model_type")

        row = box.row()
        row.operator("hda.create_model", text="Create Model")
        row.operator("hda.init_params", text="Set Model")

    
        # Get the active object
        active_object = context.active_object
        if not active_object.select_get():
            active_object = None
    
        # Count the number of selected objects
        is_multi_select = len(context.selected_objects) > 1
        first_obj = context.selected_objects[0] if is_multi_select else None
        selected = active_object or first_obj

        box = layout.box()
        row = box.row()
        row.label(text="Selection:")
        
        if is_multi_select:
            row.label(text=f"{len(context.selected_objects)} objects selected")
        else:
            obj_name = selected.name if selected else "None"
            row.label(text=obj_name)

        if not active_object and not is_multi_select:
            return
        
        hda_type = selected.get('hda_type', None)
        all_matching_type = all(obj.get("hda_type", None) == hda_type for obj in context.selected_objects)

        if all_matching_type:
            row = box.row()
            row.label(text=hda_type)

        else:
            row = box.row()
            row.label(text="various model types")
            return

        # Show properties based on selected model type
        hda = selected.hda
        props = models.get_model_props(hda, hda_type)
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
        if hda.status == "cooking":
            status_row.label(
                text="Cooking", icon='SORTTIME')
        elif hda.status == "cooked":
            status_row.label(text="Cooked", icon='CHECKMARK')
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
        row.operator("hda.init_params", text="Reset", icon='FILE_REFRESH')

        # Process button
        row.scale_y = 1.5  # Make button bigger
        row.operator("hda.process", text="Cook", icon='MOD_HUE_SATURATION')
        
        

# Auto-connect on startup
def connect_on_startup():
    # Check if auto-connect is enabled
    if bpy.context.scene.scenetalk_props.auto_connect:
        logger.info("Auto-connecting to SceneTalk server...")
        endpoint = bpy.context.scene.scenetalk_props.endpoint
        connect_to_server(endpoint)

# Registration
classes = (
    SceneTalkProperties,
    SCENETALK_PT_ConnectionPanel,
)

# Define update callback for selection changes
@persistent
def selection_change_callback(_scene):
    refresh_connection_panel()

def refresh_connection_panel():
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            area.tag_redraw()

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    
    # Register connection properties
    bpy.types.Scene.scenetalk_props = bpy.props.PointerProperty(type=SceneTalkProperties)

    # Register selection change handler
    bpy.app.handlers.depsgraph_update_post.append(selection_change_callback)

    # This avoids issues with trying to connect before the UI is ready
    bpy.app.timers.register(connect_on_startup, first_interval=.5)


def unregister():
    # Ensure disconnection
    if get_connection_state() == "connected":
        disconnect_from_server()

    # Remove selection change handler
    if selection_change_callback in bpy.app.handlers.depsgraph_update_post:
        bpy.app.handlers.depsgraph_update_post.remove(selection_change_callback)

    
    # Unregister classes
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    
    # Remove properties
    del bpy.types.Scene.scenetalk_props

if __name__ == "__main__":
    register()