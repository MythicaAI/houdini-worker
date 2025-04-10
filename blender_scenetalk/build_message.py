
import logging

logger = logging.getLogger("hda_object_operators")

def build_message(obj, input_file_id=None):
    """
    Generate a JSON message for the HDA cook operation based on the currently selected model type.
    
    Args:
        scene: The Blender scene containing the property groups
        input_file_id: Optional file ID for input geometry
        
    Returns:
        dict: JSON-compatible dictionary with the HDA cook parameters
    """
    hda = getattr(obj, "hda", None)
    if not hda:
        logger.warning("No HDA found on object %s", obj.name)
        return {}
    
    model_type = mythica_props.model_type
    
    # Initialize property values directly without deferred access
    # Get file path and parameters for the selected model type
    if model_type == 'CRYSTAL':
        hda_file_id = "mythica.crystal.1.0.hda"
        material_name = "crystal"
        
        # Get values directly instead of keeping a reference to the property group
        length = mythica_props.crystal_props.length
        radius = mythica_props.crystal_props.radius
        numsides = mythica_props.crystal_props.numsides
        
        parameters = {
            "length": length,
            "radius": radius,
            "numsides": numsides
        }
    elif model_type == 'ROCK':
        hda_file_id = "Mythica.RockGenerator.1.0.hda"
        material_name = "rock"
        
        # Get values directly
        seed = mythica_props.rock_props.seed
        smoothing = mythica_props.rock_props.smoothing
        flatten = mythica_props.rock_props.flatten
        npts = mythica_props.rock_props.npts
        
        parameters = {
            "seed": seed,
            "smoothing": smoothing,
            "flatten": flatten,
            "npts": npts
        }
    elif model_type == 'ROCKIFY':
        hda_file_id = "Mythica.Rockify.1.0.hda"
        material_name = "rockface"
        
        # Get values directly
        stage = mythica_props.rockify_props.stage
        base_rangemax = mythica_props.rockify_props.base_rangemax
        mid_rangemax = mythica_props.rockify_props.mid_rangemax
        top_rangemax = mythica_props.rockify_props.top_rangemax
        smoothingiterations = mythica_props.rockify_props.smoothingiterations
        vertDensity = mythica_props.rockify_props.vertDensity
        size = mythica_props.rockify_props.size
        
        parameters = {
            "Stage": stage,
            "base_rangemax": base_rangemax,
            "mid_rangemax": mid_rangemax,
            "top_rangemax": top_rangemax,
            "smoothingiterations": smoothingiterations,
            "vertDensity": vertDensity,
            "size": size
        }
        # If no input_file_id was provided, use a default
        if input_file_id is None:
            input_file_id = "file_xxx"
    elif model_type == 'AGAVE':
        hda_file_id = "mythica.agave_plant.1.0.hda"
        material_name = "plant"
        
        # Get values directly
        leafcount = mythica_props.agave_props.leafcount
        angle = mythica_props.agave_props.angle
        
        parameters = {
            "leafcount": leafcount,
            "Angle": angle
        }
    elif model_type == 'SAGUARO':
        hda_file_id = "mythica.saguaro_cactus.1.1.hda"
        material_name = "cactus"
        
        # Get values directly
        seed = mythica_props.saguaro_props.seed
        trunkheight = mythica_props.saguaro_props.trunkheight
        armscount = mythica_props.saguaro_props.armscount
        armsbendangle = mythica_props.saguaro_props.armsbendangle
        trunknumsides = mythica_props.saguaro_props.trunknumsides
        usespines = mythica_props.saguaro_props.usespines
        usebuds = mythica_props.saguaro_props.usebuds
        useflowers = mythica_props.saguaro_props.useflowers
        
        parameters = {
            "seed": seed,
            "trunkheight": trunkheight,
            "armscount": armscount,
            "armsbendangle": armsbendangle,
            "trunknumsides": trunknumsides,
            "usespines": usespines,
            "usebuds": usebuds,
            "useflowers": useflowers
        }
    
    # Construct the message
    message = {
        "op": "cook",
        "data": {
            "hda_path": {
                "file_id": hda_file_id,
            },
            "definition_index": 0,
            "format": "raw",
            "material_name": material_name
        }
    }
    
    # Add input0 if provided
    if input_file_id is not None and model_type == 'ROCKIFY':
        message["data"]["input0"] = {
            "file_id": input_file_id,
        }
    
    # Add all parameters
    for key, value in parameters.items():
        message["data"][key] = value
    
    return message