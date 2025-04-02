print("__init__.py loaded")

import bpy
import sys
import os

# Add the 'libs' folder to the Python path
libs_path = os.path.join(os.path.dirname(os.path.realpath(__file__)), "lib")
if libs_path not in sys.path:
    sys.path.append(libs_path)
import httpx

from .panels import connection_panel, hda_object_panel
from .operators import connection_operators, hda_object_operators

bl_info = {
    "name": "Blender SceneTalk",
    "author": "Jacob Repp",
    "version": (0, 1, 0),
    "blender": (4, 2, 0),
    "location": "Properties > Object > Houdini Digital Asset",
    "description": "Integrates Houdini Digital Assets (HDAs) into Blender",
    "warning": "Experimental",
    "doc_url": "",
    "category": "Object",
}


def register():
    connection_panel.register()
    hda_object_panel.register()
    connection_operators.register()
    hda_object_operators.register()


def unregister():
    connection_panel.unregister()
    hda_object_panel.unregister()
    connection_operators.unregister()
    hda_object_operators.unregister()


if __name__ == "__main__":
    register()
