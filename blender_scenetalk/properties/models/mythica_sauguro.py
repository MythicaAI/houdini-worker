import bpy
from bpy.props import FloatProperty, IntProperty, BoolProperty
from ...cook import update_float_value, update_int_value, update_bool_value, update_string_value

class SaguaroProperties(bpy.types.PropertyGroup):
    seed: IntProperty(
        name="Randomize",
        description="Random seed for cactus generation",
        min=0,
        max=100,
        step=1,
        default=0,
        update=update_int_value,
    )
    trunkheight: FloatProperty(
        name="Height",
        description="Trunk height",
        min=4.0,
        max=7.0,
        step=0.1,
        default=5.5,
        update=update_float_value,
    )
    armscount: IntProperty(
        name="Max Arms",
        description="Maximum number of arms",
        min=0,
        max=4,
        step=1,
        default=2,
        update=update_int_value,
    )
    armsbendangle: IntProperty(
        name="Bend Angle",
        description="Arm bend angle",
        min=75,
        max=90,
        step=1,
        default=82,
        update=update_int_value,
    )
    trunknumsides: IntProperty(
        name="Trunk Sides",
        description="Number of trunk sides",
        default=15,
        update=update_int_value,
    )
    usespines: BoolProperty(
        name="Use Spines",
        description="Enable spines",
        default=False,
        update=update_bool_value,
    )
    usebuds: BoolProperty(
        name="Use Buds",
        description="Enable buds",
        default=False,
        update=update_bool_value,
    )
    useflowers: BoolProperty(
        name="Use Flowers",
        description="Enable flowers",
        default=False,
        update=update_bool_value
    )

    def draw(self, layout):
        col = layout.column(align=True)
        col.prop(self, "seed")
        col.prop(self, "trunkheight")
        col.prop(self, "armscount")
        col.prop(self, "armsbendangle")
    
    def get_params(self):
        return {
            "seed": self.seed,
            "trunkheight": self.trunkheight,
            "armscount": self.armscount,
            "armsbendangle": self.armsbendangle,
        }
    
classes = (SaguaroProperties,) 