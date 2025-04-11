import bpy

def get_selected_objects():
    """Get a list of all currently selected objects"""
    return [obj for obj in bpy.context.selected_objects]

def save_and_restore_selection(function):
    """
    Decorator that saves the current selection state, executes a function,
    and then restores the original selection state
    
    Args:
        function: The function to execute with a temporary selection
    
    Returns:
        A wrapped function that preserves selection state
    """
    def wrapper(*args, **kwargs):
        # Store current selection state
        original_selection = get_selected_objects()
        original_active = bpy.context.view_layer.objects.active
        
        # Deselect all objects
        bpy.ops.object.select_all(action='DESELECT')
        
        # Execute the wrapped function
        result = function(*args, **kwargs)
        
        # Restore original selection state
        bpy.ops.object.select_all(action='DESELECT')
        for obj in original_selection:
            if obj.name in bpy.data.objects:
                obj.select_set(True)
        
        # Restore active object
        if original_active and original_active.name in bpy.data.objects:
            bpy.context.view_layer.objects.active = original_active
        
        return result
    
    return wrapper