import bpy
from bpy.types import Panel, PropertyGroup, Operator
from bpy.props import FloatProperty, IntProperty, StringProperty, PointerProperty, EnumProperty, BoolProperty

import json
import bmesh
import logging
from mathutils import Vector

from .hda_db import find_by_name

logger = logging.getLogger("build_object")

# Property type mapping
PROP_TYPE_MAP = {
    "slider": {
        "int": IntProperty,
        "float": FloatProperty
    },
    "checkbox": BoolProperty,
    "enum": EnumProperty,
    "string": StringProperty
}

def get_object_by_name(name):
    return bpy.data.objects.get(name)

def upsert_object_data(model_type, name, geom_data, schema):
    # Create a new mesh and bmesh
    obj = get_object_by_name(name)
    is_update = False
    if obj:
        # Remove existing meshes from the object
        if obj.type == 'MESH':
            old_mesh = obj.data
        is_update = True
    else:
        # create a new object
        obj = bpy.data.objects.new(name, None)
    
    # TODO: grab from the current schema
    hda_type = model_type
    mesh = bpy.data.meshes.new(f"{hda_type.upper()}_{name}_mesh")
    bm = bmesh.new()
    
    # Extract data from the geometry
    points = geom_data['data']['points']
    indices = geom_data['data']['indices']
    colors = geom_data['data'].get('colors', None)
    
    # Create vertices
    vertices = []
    for i in range(0, len(points), 3):
        vertex = bm.verts.new((points[i], points[i+1], points[i+2]))
        vertices.append(vertex)
    
    # Ensure lookup table is updated for vertex indices
    bm.verts.ensure_lookup_table()
    
    # Create faces from indices
    for i in range(0, len(indices), 3):
        try:
            bm.faces.new([vertices[indices[i]], vertices[indices[i+1]], vertices[indices[i+2]]])
        except Exception as e:
            print(f"Error creating face {i//3}: {e}")
    
    # Set vertex colors if the mesh has faces
    total_verts = 0
    if bm.faces and colors:
        color_layer = bm.loops.layers.color.new("Col")
        for face in bm.faces:
            for j, loop in enumerate(face.loops):
                vert_idx = vertices.index(loop.vert)
                color_idx = vert_idx * 3
                if color_idx + 2 < len(colors):
                    loop[color_layer] = (
                        colors[color_idx],
                        colors[color_idx + 1],
                        colors[color_idx + 2],
                        1.0  # Alpha
                    )
                total_verts += vert_idx
    # Update and free bmesh
    bm.to_mesh(mesh)
    bm.free()
    
    # Create object from mesh
    if is_update:
        obj.data = mesh
        bpy.data.meshes.remove(old_mesh)
    else:
        obj = bpy.data.objects.new(name, mesh)
    
        # Link object to scene collection
        bpy.context.collection.objects.link(obj)

         # HACK: Rotate the object so that Z is up, this is supposed to be
        # fixed by the OCS being transmitted as a property of the prim
        obj.rotation_euler = (1.5708, 0, 0)  # Rotate 90 degrees around the X-axis


    # Select the object
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)

   
    logger.info("[%s%s] %s %s verts", 
                    model_type,
                    '+' if not is_update else '',
                    name, 
                    total_verts)


    # Mark as an HDA
    obj["hda_type"] = hda_type

    # Store the generator schema
    obj.hda.set_from_schema(schema)

    obj.update_tag()

    return obj


def update_object(context):
    """Callback for when a parameter is changed"""
    obj = context.object
    if obj and obj.get('hda_properties', False):
        # Call Houdini to rebuild the object with new parameters
        print(f"Updating {obj.name} with new parameters")
        
        # Get all parameters as a dictionary to pass to Houdini
        params = {}
        for prop_name in obj.get('crystal_param_names', []):
            if hasattr(obj.hda_properties, prop_name):
                params[prop_name] = getattr(obj.hda_properties, prop_name)
        
        logger.info("updating %s with %s", obj.name, params)
        

# Determine property type based on value and step
def get_prop_type(param_data):
    if param_data["type"] == "slider":
        # Check if step is an integer and values are integers
        if param_data.get("step", 1) % 1 == 0 and param_data.get("min", 0) % 1 == 0 and param_data.get("max", 1) % 1 == 0:
            return "int"
        return "float"
    return None



# Create properties dynamically from schema
def populate_dynamic_properties(cls, params):
    # Parameters
    for param_name, param_data in params.items():
        prop_type = param_data.get("type")
        
        if prop_type == "slider":
            value_type = get_prop_type(param_data)
            prop_class = PROP_TYPE_MAP[prop_type][value_type]
            
            # Create property with appropriate args
            prop_kwargs = {
                "name": param_data.get("label", param_name),
                "description": param_data.get("description", f"Adjust the {param_name}"),
                "min": param_data.get("min", 0),
                "max": param_data.get("max", 10),
                "default": param_data.get("default", param_data.get("min", 0)),
                "update": update_object,
            }
            
            # Add step for numeric types
            if "step" in param_data:
                prop_kwargs["step"] = param_data["step"]
            
            # Add precision for float types
            if value_type == "float":
                prop_kwargs["precision"] = param_data.get("precision", 2)
            
            # Create the property
            setattr(cls, param_name, prop_class(**prop_kwargs))
        
        elif prop_type == "checkbox":
            prop_class = PROP_TYPE_MAP[prop_type]
            setattr(cls, param_name, prop_class(
                name=param_data.get("label", param_name),
                description=param_data.get("description", f"Toggle {param_name}"),
                default=param_data.get("default", False)
            ))
        
        elif prop_type == "enum":
            prop_class = PROP_TYPE_MAP[prop_type]
            items = []
            for item in param_data.get("items", []):
                items.append((item["value"], item["name"], item.get("description", "")))
            
            setattr(cls, param_name, prop_class(
                name=param_data.get("label", param_name),
                description=param_data.get("description", f"Select {param_name}"),
                items=items,
                default=param_data.get("default", items[0][0] if items else "")
            ))
        
        elif prop_type == "string":
            prop_class = PROP_TYPE_MAP[prop_type]
            setattr(cls, param_name, prop_class(
                name=param_data.get("label", param_name),
                description=param_data.get("description", f"Enter {param_name}"),
                default=param_data.get("default", "")
            ))

# Create properties dynamically from schema
def populate_properties(cls, params):
    # Parameters
    for param_name, param_data in params.items():
        prop_type = param_data.get("type")
        
        if prop_type == "slider":
            value_type = get_prop_type(param_data)
            prop_class = PROP_TYPE_MAP[prop_type][value_type]
            
            # Create property with appropriate args
            prop_kwargs = {
                "name": param_data.get("label", param_name),
                "description": param_data.get("description", f"Adjust the {param_name}"),
                "min": param_data.get("min", 0),
                "max": param_data.get("max", 10),
                "default": param_data.get("default", param_data.get("min", 0)),
                "update": update_object,
            }
            
            # Add step for numeric types
            if "step" in param_data:
                prop_kwargs["step"] = param_data["step"]
            
            # Add precision for float types
            if value_type == "float":
                prop_kwargs["precision"] = param_data.get("precision", 2)
            
            # Create the property
            setattr(cls, param_name, prop_class(**prop_kwargs))
        
        elif prop_type == "checkbox":
            prop_class = PROP_TYPE_MAP[prop_type]
            setattr(cls, param_name, prop_class(
                name=param_data.get("label", param_name),
                description=param_data.get("description", f"Toggle {param_name}"),
                default=param_data.get("default", False)
            ))
        
        elif prop_type == "enum":
            prop_class = PROP_TYPE_MAP[prop_type]
            items = []
            for item in param_data.get("items", []):
                items.append((item["value"], item["name"], item.get("description", "")))
            
            setattr(cls, param_name, prop_class(
                name=param_data.get("label", param_name),
                description=param_data.get("description", f"Select {param_name}"),
                items=items,
                default=param_data.get("default", items[0][0] if items else "")
            ))
        
        elif prop_type == "string":
            prop_class = PROP_TYPE_MAP[prop_type]
            setattr(cls, param_name, prop_class(
                name=param_data.get("label", param_name),
                description=param_data.get("description", f"Enter {param_name}"),
                default=param_data.get("default", "")
            ))

# This function will be called when a parameter changes
def parameter_update_callback(self, context):
    obj = context.object
    # Collect all parameters for this object
    params = {}
    if hasattr(obj, "custom_parameters"):
        for param_name in obj.custom_parameters.keys():
            params[param_name] = obj.custom_parameters[param_name]
    
    # Here you would call your backend to regenerate geometry
    print(f"Parameters updated for {obj.name}: {params}")
    # Example of how you might call your backend
    # regenerate_geometry(obj.name, params)
