import bpy
from bpy.props import FloatProperty, IntProperty, BoolProperty
from ...cook import update_float_value, update_int_value, update_bool_value, update_string_value

class CrystalsProperties(bpy.types.PropertyGroup):
    length: FloatProperty(
        name="Length",
        description="Crystal length",
        min=0.5,
        max=5.0,
        step=0.1,
        default=2.5,
        update=update_float_value
    )
    radius: FloatProperty(
        name="Radius",
        description="Crystal radius",
        min=0.1,
        max=2.0,
        step=0.1,
        default=0.6,
        update=update_float_value
    )
    numsides: IntProperty(
        name="Sides",
        description="Number of sides",
        min=3,
        max=12,
        step=1,
        default=6,
        update=update_int_value
    )

    def draw(self, layout):
        col = layout.column(align=True)
        col.prop(self, "length")
        col.prop(self, "radius")
        col.prop(self, "numsides")

    def get_params(self):
        return {
            "length": self.length,
            "radius": self.radius,
            "numsides": self.numsides,
        }


classes = (CrystalsProperties,)