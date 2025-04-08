import bpy
import logging
from bpy.props import StringProperty, BoolProperty, EnumProperty
from bpy.app.handlers import persistent

# Import the SceneTalk client
from ..scenetalk_client import SceneTalkClient
from ..scenetalk_connection import get_connection_state, connect_to_server, disconnect_from_server

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
        connection_props = context.scene.scenetalk_props
        
        # Connection endpoint
        box = layout.box()
        
        # Status display
        row = box.row()
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
        row = box.row()
        row.label(text="SceneTalk Endpoint:")
        row = box.row()
        row.prop(connection_props, "endpoint")
        
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
        row.prop(connection_props, "auto_connect")

        # Show connected controls
        if state != "connected":
            return
        
        box = layout.box()
        row = box.row()
        row.prop(connection_props, "model_type")

        row = box.row()
        row.operator("hda.create_model", text="Create Model")
        row.operator("hda.init_params", text="Set Model")
        

# Auto-connect on startup
@bpy.app.handlers.persistent
def connect_on_startup(dummy):
    # Check if auto-connect is enabled
    if bpy.context.scene.scenetalk_connection.auto_connect:
        logger.info("Auto-connecting to SceneTalk server...")
        endpoint = bpy.context.scene.scenetalk_connection.endpoint
        connect_to_server(endpoint)

# Registration
classes = (
    SceneTalkProperties,
    SCENETALK_PT_ConnectionPanel,
)

# Define update callback for selection changes
@persistent
def selection_change_callback(scene):
    # Force redraw of UI
    for area in bpy.context.screen.areas:
        if area.type == 'VIEW_3D':
            area.tag_redraw()


def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    
    # Register connection properties
    bpy.types.Scene.scenetalk_props = bpy.props.PointerProperty(type=SceneTalkProperties)
    
    # Register startup handler
    bpy.app.handlers.load_post.append(connect_on_startup)

    # Register selection change handler
    bpy.app.handlers.depsgraph_update_post.append(selection_change_callback)


def unregister():
    # Ensure disconnection
    if get_connection_state() == "connected":
        disconnect_from_server()
    
    # Remove startup handler
    if connect_on_startup in bpy.app.handlers.load_post:
        bpy.app.handlers.load_post.remove(connect_on_startup)

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