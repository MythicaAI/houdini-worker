import bpy
import asyncio
import threading


# Import the SceneTalk client
from .scenetalk_client import SceneTalkClient
from .async_wrap import run_async

# Global connection state
_client = None
_connection_state = "disconnected"
_connection_thread = None


# Connection management functions
def get_client():
    """Get the SceneTalk client instance."""
    global _client
    if _client is None:
        _client = SceneTalkClient()
        
        # Register basic callbacks
        _client.set_callbacks({
            "on_connect": lambda: set_connection_state("connected"),
            "on_disconnect": lambda: set_connection_state("disconnected"),
            "on_error": lambda error: set_connection_state(f"error: {error}")
        })
    
    return _client

def set_connection_state(state):
    """Update the connection state."""
    global _connection_state
    _connection_state = state
    # Force Blender UI refresh
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            area.tag_redraw()

def get_connection_state():
    """Get the current connection state."""
    return _connection_state

def connect_to_server(endpoint):
    """Connect to the Houdini server."""
    global _connection_thread

    # Cancel any existing connection attempt
    if _connection_thread and _connection_thread.is_alive():
        return False
    
    client = get_client()
    set_connection_state("connecting")
    
    async def connect_task():
        # Update client's URL
        client.ws_url = endpoint
        success = await client.connect()
        if not success:
            set_connection_state("connection failed")
    
    _connection_thread = run_async(connect_task())
    return True

def disconnect_from_server():
    """Disconnect from the Houdini server."""
    global _connection_thread
    
    client = get_client()
    set_connection_state("disconnecting")
    
    async def disconnect_task():
        await client.disconnect()
    
    _connection_thread = run_async(disconnect_task())
    return True