import bpy
from bpy.props import StringProperty, BoolProperty, PointerProperty
from ..scenetalk_connection import get_connection_state

# HDA Property Group - this will be attached to Blender objects
class HDAProperties(bpy.types.PropertyGroup):
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


class HDA_PT_Panel(bpy.types.Panel):
    """HDA Panel in Object Properties"""
    bl_label = "Houdini Digital Asset"
    bl_idname = "HDA_PT_Panel"
    bl_space_type = 'PROPERTIES'
    bl_region_type = 'WINDOW'
    bl_context = "object"
    bl_options = {'DEFAULT_CLOSED'}  # Collapsed by default

    @classmethod
    def poll(cls, context):
        return context.object is not None

    def draw_header(self, context):
        layout = self.layout
        obj = context.object
        if hasattr(obj, "hda"):
            layout.prop(obj.hda, "enabled", text="")

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True  # Blender 4.2 style
        layout.use_property_decorate = False  # No animation

        obj = context.object

        # Connection status and controls
        box = layout.box()
        row = box.row()

        # Use icons for connection status
        conn_state = get_connection_state()
        if conn_state == "connected":
            row.label(text="Server Connected", icon='CHECKMARK')
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

        # HDA controls - only show if we have hda properties and the panel is enabled
        if hasattr(obj, "hda"):
            hda = obj.hda

            # Only show HDA controls if panel is enabled
            if hda.enabled:
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

                # Process button
                row = layout.row()
                row.scale_y = 1.5  # Make button bigger

                if get_connection_state() == "connected":
                    row.operator("hda.process", text="Process HDA",
                                 icon='MOD_HUE_SATURATION')
                else:
                    row.operator("hda.process", text="Process HDA",
                                 icon='MOD_HUE_SATURATION').enabled = False
        else:
            # Add HDA button
            row = layout.row()
            row.scale_y = 1.5
            row.operator("hda.add_to_object",
                         text="Add HDA to Object", icon='ADD')


classes = (
    HDAProperties,
    HDA_PT_Panel,
)

def register():
    for cls in classes:
        bpy.utils.register_class(cls)

    # Register the property group
    bpy.types.Object.hda = PointerProperty(type=HDAProperties)

def unregister():
    for cls in reversed(classes):
        bpy.utils.unregister_class(cls)

    # Remove the property group
    del bpy.types.Object.hda