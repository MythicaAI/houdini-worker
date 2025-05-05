from . import gen_object_properties, scenetalk_properties

def register():
    gen_object_properties.register()
    scenetalk_properties.register()

def unregister():
    gen_object_properties.unregister()
    scenetalk_properties.unregister()
    