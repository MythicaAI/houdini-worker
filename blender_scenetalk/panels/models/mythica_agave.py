import bpy
from bpy.props import FloatProperty, IntProperty, BoolProperty
from ..update_hooks import update_float_value, update_int_value, update_bool_value, update_string_value

class AgaveProperties(bpy.types.PropertyGroup):
    leafcount: IntProperty(
        name="Leaf Count",
        description="Number of agave leaves",
        min=15,
        max=30,
        step=1,
        default=25,
        update=update_int_value,
    )
    angle: IntProperty(
        name="Angle",
        description="Leaf angle",
        min=30,
        max=60,
        step=1,
        default=50,
        update=update_int_value,
    )

    def draw(self, layout):
        col = layout.column(align=True)
        col.prop(self, "leafcount")
        col.prop(self, "angle")

    def get_params(self):
        return {
            "leafcount": self.leafcount,
            "angle": self.angle,
        }

classes = (AgaveProperties,)