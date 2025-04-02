import bpy
import json
import bmesh
from mathutils import Vector

def build_object(geom_data, name="ImportedGeometry"):
    # Create a new mesh and bmesh
    mesh = bpy.data.meshes.new(name + "_mesh")
    bm = bmesh.new()
    
    # Extract data from the geometry
    points = geom_data['data']['points']
    colors = geom_data['data']['colors']
    indices = geom_data['data']['indices']
    
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
    if bm.faces:
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
    
    # Update and free bmesh
    bm.to_mesh(mesh)
    bm.free()
    
    # Create object from mesh
    obj = bpy.data.objects.new(name, mesh)
    
    # Link object to scene collection
    bpy.context.collection.objects.link(obj)
    
    # Select the object
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    
    # Rotate the object so that Z is up
    obj.rotation_euler = (1.5708, 0, 0)  # Rotate 90 degrees around the X-axis
    
    return obj