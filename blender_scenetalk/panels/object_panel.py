import bpy
import logging

logger = logging.getLogger("object_panel")

            
class OBJ_PT_Panel(bpy.types.Panel):
    """HDA Panel in Object Properties"""
    bl_label = "SceneTalk PCG Asset"
    bl_idname = "OBJ_PT_Panel"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}  # Collapsed by default

    @classmethod
    def poll(cls, context):
        show = context.object and context.object.get('model_type', None)
        return show
    
    def draw_header(self, context):
        layout = self.layout
        obj = context.object
        if hasattr(obj, "gen"):
            layout.prop(obj.gen, "enabled", text="")

    def draw(self, context):
        obj = context.object
        model_type = obj.get('model_type', None)
        if not model_type:
            return

        layout = self.layout
        layout.use_property_split = True  # Blender 4.2 style
        layout.use_property_decorate = False  # No animation


classes = (
    OBJ_PT_Panel,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)
    

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)
