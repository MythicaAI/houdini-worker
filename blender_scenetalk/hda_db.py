import requests
import logging

logger = logging.getLogger('hda_db')

_db = None


# Define the parameter schemas as a Python dictionary
built_in_hdas = {
    "CRYSTALS": {
        "file_path": "assets/mythica.crystal.1.0.hda",
        "material_name": "crystal",
        "parameters": {
            "length": {
                "type": "slider",
                "label": "Length",
                "min": 0.5,
                "max": 5,
                "step": 0.1,
                "default": 2.5,
            },
            "radius": {
                "type": "slider",
                "label": "Radius",
                "min": 0.1,
                "max": 2,
                "step": 0.1,
                "default": 0.6,
            },
            "numsides": {
                "type": "slider",
                "label": "Sides",
                "min": 3,
                "max": 12,
                "step": 1,
                "default": 6,
            },
        },
    },
    "ROCKS": {
        "file_path": "assets/Mythica.RockGenerator.1.0.hda",
        "material_name": "rock",
        "parameters": {
            "seed": {
                "type": "slider",
                "label": "Randomize",
                "min": 0,
                "max": 100,
                "step": 1,
                "default": 0,
            },
            "smoothing": {
                "type": "slider",
                "label": "Smoothing",
                "min": 0,
                "max": 50,
                "step": 1,
                "default": 25,
            },
            "flatten": {
                "type": "slider",
                "label": "Flatten",
                "min": 0,
                "max": 1,
                "step": 0.1,
                "default": 0.3,
            },
            "npts": {
                "type": "hidden",
                "default": 5,
            },
        },
    },
    "ROCKIFY": {
        "file_path": "assets/Mythica.Rockify.1.0.hda",
        "material_name": "rockface",
        "parameters": {
            "Stage": {
                "type": "slider",
                "label": "Stage",
                "min": 0,
                "max": 3,
                "step": 1,
                "default": 1,
            },
            "base_rangemax": {
                "type": "slider",
                "label": "Base Noise",
                "min": 0.0,
                "max": 10.0,
                "step": 0.5,
                "default": 6.5,
            },
            "mid_rangemax": {
                "type": "slider",
                "label": "Mid Noise",
                "min": 0.0,
                "max": 3,
                "step": 0.25,
                "default": 0.25,
            },
            "top_rangemax": {
                "type": "slider",
                "label": "Top Noise",
                "min": 0.0,
                "max": 5.0,
                "step": 0.5,
                "default": 0.5,
            },
            "smoothingiterations": {
                "type": "hidden",
                "default": 0,
            },
            "vertDensity": {
                "type": "hidden",
                "default": 0.1,
            },
            "size": {
                "type": "hidden",
                "default": 512,
            },
            "input0": {
                "type": "hidden",
                "default": {
                    "file_id": "file_xxx",
                    "file_path": "assets/SM_Shape_04_a.usd",
                },
            },
        },
    },
    "AGAVE": {
        "file_path": "assets/mythica.agave_plant.1.0.hda",
        "material_name": "plant",
        "parameters": {
            "leafcount": {
                "type": "slider",
                "label": "Leaf Count",
                "min": 15,
                "max": 30,
                "step": 1,
                "default": 25,
            },
            "Angle": {
                "type": "slider",
                "label": "Angle",
                "min": 30,
                "max": 60,
                "step": 1,
                "default": 50,
            },
        },
    },
    "SAGUARO": {
        "file_path": "assets/mythica.saguaro_cactus.1.1.hda",
        "material_name": "cactus",
        "parameters": {
            "seed": {
                "type": "slider",
                "label": "Randomize",
                "min": 0,
                "max": 100,
                "step": 1,
                "default": 0,
            },
            "trunkheight": {
                "type": "slider",
                "label": "Height",
                "min": 4,
                "max": 7,
                "step": 0.1,
                "default": 5.5,
            },
            "armscount": {
                "type": "slider",
                "label": "Max Arms",
                "min": 0,
                "max": 4,
                "step": 1,
                "default": 2,
            },
            "armsbendangle": {
                "type": "slider",
                "label": "Bend Angle",
                "min": 75,
                "max": 90,
                "step": 1,
                "default": 82,
            },
            "trunknumsides": {
                "type": "hidden",
                "default": 15,
            },
            "usespines": {
                "type": "hidden",
                "default": False,
            },
            "usebuds": {
                "type": "hidden",
                "default": False,
            },
            "useflowers": {
                "type": "hidden",
                "default": False,
            },
        },
    },
}

def load_from_api():
    r = requests.get("https://api.mythica.gg/v1/assets/groups/blender")
    r.raise_for_status()
    packages = r.json()
    logger.info("Loaded %d packages from API", len(packages))


def get_db():
    global _db
    if _db is None:
        _db = built_in_hdas
    return _db


def find_by_name(name):
    return get_db().get(name, None)


def get_names():
    return list(get_db().keys())