import bpy
from bpy.props import StringProperty, BoolProperty

from ..scenetalk_connection import connect_to_server, disconnect_from_server, get_connection_state


# Connect operator
class SCENETALK_OT_Connect(bpy.types.Operator):
    """Connect to Houdini Server"""
    bl_idname = "scenetalk.connect"
    bl_label = "Connect to SceneTalk Endpoint"
    bl_options = {'REGISTER'}
    
    def execute(self, context):
        endpoint = context.scene.scenetalk_props.endpoint
        if connect_to_server(endpoint):
            self.report({'INFO'}, f"Connecting to SceneTalk server at {endpoint}")
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Connection already in progress")
            return {'CANCELLED'}

# Disconnect operator
class SCENETALK_OT_Disconnect(bpy.types.Operator):
    """Disconnect from SceneTalk Server"""
    bl_idname = "scenetalk.disconnect"
    bl_label = "Disconnect from SceneTalk Endpoint"
    bl_options = {'REGISTER'}
    
    def execute(self, context):
        if disconnect_from_server():
            self.report({'INFO'}, "Disconnecting from SceneTalk server")
            return {'FINISHED'}
        else:
            self.report({'WARNING'}, "Disconnection failed")
            return {'CANCELLED'}

# Reconnect operator
class SCENETALK_OT_Reconnect(bpy.types.Operator):
    """Reconnect to SceneTalk Server"""
    bl_idname = "scenetalk.reconnect"
    bl_label = "Reconnect to SceneTalk Endpoint"
    bl_options = {'REGISTER'}
    
    enabled: BoolProperty(default=False)

    def execute(self, context):
        endpoint = context.scene.scenetalk_props.endpoint

        # First disconnect if connected
        if get_connection_state() == "connected":
            disconnect_from_server()
            
            # Wait a short time before reconnecting
            def delayed_connect():
                connect_to_server(endpoint)
                return None  # Don't repeat
                
            bpy.app.timers.register(delayed_connect, first_interval=1.0)
            
            self.report({'INFO'}, f"Reconnecting to SceneTalk server at {endpoint}")
            return {'FINISHED'}
        else:
            # Just connect directly
            if connect_to_server(endpoint):
                self.report({'INFO'}, f"Connecting to SceneTalk server at {endpoint}")
                return {'FINISHED'}
            else:
                self.report({'WARNING'}, "Connection already in progress")
                return {'CANCELLED'}
            
classes = (
    SCENETALK_OT_Connect,
    SCENETALK_OT_Disconnect,
    SCENETALK_OT_Reconnect
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

def unregister():
    # Unregister classes
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)