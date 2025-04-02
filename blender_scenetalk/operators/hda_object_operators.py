import bpy
from bpy.props import PointerProperty
from ..scenetalk_connection import get_connection_state

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
        return (obj and hasattr(obj, "hda") and
                obj.hda.enabled and
                get_connection_state() == "connected")

    def invoke(self, context, event):
        # Ask for confirmation before processing
        return context.window_manager.invoke_confirm(self, event)

    def execute(self, context):
        obj = context.object
        hda = obj.hda

        if not hda.hda_path or not os.path.exists(hda.hda_path):
            self.report({'ERROR'}, f"HDA file not found: {hda.hda_path}")
            return {'CANCELLED'}

        # Update status
        hda.status = "Processing..."

        # This is a basic implementation - in a real addon we would:
        # 1. Export the current object to a temporary file
        # 2. Upload the HDA and input file
        # 3. Process the HDA
        # 4. Import the result

        # For now, just simulate processing
        def update_status():
            hda.status = "Processed"
            # Force UI refresh
            for window in bpy.context.window_manager.windows:
                for area in window.screen.areas:
                    area.tag_redraw()
            return None  # Don't repeat

        # Schedule status update after a delay
        bpy.app.timers.register(update_status, first_interval=2.0)

        self.report({'INFO'}, "Processing HDA...")
        return {'FINISHED'}
    
classes = (
    HDA_OT_AddToObject,
    HDA_OT_Process,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)  # Unregister in reverse order