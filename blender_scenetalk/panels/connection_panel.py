import bpy
import asyncio
import threading
from bpy.props import StringProperty, BoolProperty

# Import the SceneTalk client
from ..scenetalk_client import SceneTalkClient
from ..scenetalk_connection import get_client, set_connection_state, get_connection_state, connect_to_server, disconnect_from_server


# Connection properties
class SceneTalkConnectionProperties(bpy.types.PropertyGroup):
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
            connect_op = row.operator("scenetalk.connect", text="Connect", icon='PLAY')
            
        
        # Reconnect button - always visible but only enabled if not connected
        row = box.row()
        reconnect_op = row.operator("scenetalk.reconnect", text="Reconnect", icon='FILE_REFRESH')
        reconnect_op.enabled = (state != "connecting" and state != "disconnecting")
        
        # Auto-connect toggle
        row = box.row()
        row.prop(connection_props, "auto_connect")


# Auto-connect on startup
@bpy.app.handlers.persistent
def connect_on_startup(dummy):
    # Check if auto-connect is enabled
    if bpy.context.scene.scenetalk_connection.auto_connect:
        endpoint = bpy.context.scene.scenetalk_connection.endpoint
        connect_to_server(endpoint)

# Registration
classes = (
    SceneTalkConnectionProperties,
    SCENETALK_PT_ConnectionPanel,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    
    # Register connection properties
    bpy.types.Scene.scenetalk_props = bpy.props.PointerProperty(type=SceneTalkConnectionProperties)
    
    # Register startup handler
    bpy.app.handlers.load_post.append(connect_on_startup)

def unregister():
    # Ensure disconnection
    if get_connection_state() == "connected":
        disconnect_from_server()
    
    # Remove startup handler
    if connect_on_startup in bpy.app.handlers.load_post:
        bpy.app.handlers.load_post.remove(connect_on_startup)
    
    # Unregister classes
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
    
    # Remove properties
    del bpy.types.Scene.scenetalk_props

if __name__ == "__main__":
    register()