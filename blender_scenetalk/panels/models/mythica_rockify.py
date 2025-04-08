import bpy
from bpy.props import FloatProperty, IntProperty, BoolProperty
from ..update_hooks import update_float_value, update_int_value, update_bool_value, update_string_value


class RockifyProperties(bpy.types.PropertyGroup):
    stage: IntProperty(
        name="Stage",
        description="Processing stage",
        min=0,
        max=3,
        step=1,
        default=1,
        update=update_int_value,
    )
    base_rangemax: FloatProperty(
        name="Base Noise",
        description="Base noise amount",
        min=0.0,
        max=10.0,
        step=0.5,
        default=6.5,
        update=update_float_value
    )
    mid_rangemax: FloatProperty(
        name="Mid Noise",
        description="Mid-level noise amount",
        min=0.0,
        max=3.0,
        step=0.25,
        default=0.25,
        update=update_float_value
    )
    top_rangemax: FloatProperty(
        name="Top Noise",
        description="Top-level noise amount",
        min=0.0,
        max=5.0,
        step=0.5,
        default=0.5,
        update=update_float_value
    )
    smoothingiterations: IntProperty(
        name="Smoothing Iterations",
        description="Number of smoothing iterations",
        default=0,
        options={'HIDDEN'},
        update=update_int_value
    )
    vertDensity: FloatProperty(
        name="Vertex Density",
        description="Vertex density",
        default=0.1,
        options={'HIDDEN'},
        update=update_float_value
    )
    size: IntProperty(
        name="Size",
        description="Size parameter",
        default=512,
        options={'HIDDEN'},
        update=update_int_value,
    )

    def draw(self, layout):
        col = layout.column(align=True)
        col.prop(self, "stage")
        col.prop(self, "base_rangemax")
        col.prop(self, "mid_rangemax")
        col.prop(self, "top_rangemax")

    def get_params(self):
        return {
            "stage": self.stage,
            "base_rangemax": self.base_rangemax,
            "mid_rangemax": self.mid_rangemax,
            "top_rangemax": self.top_rangemax,
        }