import bpy
import os
import importlib

from .mythica_agave import AgaveProperties
from .mythica_crystals import CrystalsProperties
from .mythica_rockify import RockifyProperties
from .mythica_rocks import RocksProperties
from .mythica_sauguro import SaguaroProperties

all_model_classes = [AgaveProperties,
                      CrystalsProperties,
                        RockifyProperties,
                          RocksProperties,
                            SaguaroProperties]

def register_all_models():
    for cls in all_model_classes:
        bpy.utils.register_class(cls)

def unregister_all_models():
    for cls in all_model_classes:
        bpy.utils.unregister_class(cls)

def get_model_item_enum():
    return [
        ('CRYSTALS', "Crystals", "Generate crystal geometry"),
        ('ROCKS', "Rocks", "Generate rock geometry"),
        ('ROCKIFY', "Rockify", "Apply rock texture to objects"),
        ('AGAVE', "Agave", "Generate agave plant"),
        ('SAGUARO', "Saguaro", "Generate saguaro cactus"),
    ]
    
def get_model_props(hda, hda_type):
    if hda_type == 'CRYSTALS':
        return hda.crystals_props
    elif hda_type == 'ROCKS':
        return hda.rocks_props
    elif hda_type == 'ROCKIFY':
        return hda.rockify_props
    elif hda_type == 'AGAVE':
        return hda.agave_props
    elif hda_type == 'SAGUARO':
        return hda.saguaro_props
    else:
        return None