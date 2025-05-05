import bpy
from bpy.props import FloatProperty, IntProperty, BoolProperty
from ...cook import update_float_value, update_int_value, update_bool_value, update_string_value

class RocksProperties(bpy.types.PropertyGroup):
    seed: IntProperty(
        name="Randomize",
        description="Random seed for rock generation",
        min=0,
        max=100,
        step=1,
        default=0,
        update=update_int_value
    )
    smoothing: IntProperty(
        name="Smoothing",
        description="Rock smoothing amount",
        min=0,
        max=50,
        step=1,
        default=25,
        update=update_int_value
    )
    flatten: FloatProperty(
        name="Flatten",
        description="Rock flattening amount",
        min=0.0,
        max=1.0,
        step=0.1,
        default=0.3,
        update=update_float_value
    )
    npts: IntProperty(
        name="Points",
        description="Number of points",
        default=5,
        options={'HIDDEN'},
        update=update_int_value,
    )

    def draw(self, layout):
        col = layout.column(align=True)
        col.prop(self, "seed")
        col.prop(self, "smoothing")
        col.prop(self, "flatten")

    def get_params(self):
        return {
            "seed": self.seed,
            "smoothing": self.smoothing,
            "flatten": self.flatten,
        }
    
classes = (RocksProperties,)  # Add this line to register the class