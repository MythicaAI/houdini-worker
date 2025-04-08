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

def init_client(event_queue: asyncio.Queue):
    """Initialize the SceneTalk client."""
    global _client
    _client = SceneTalkClient(event_queue)
    return _client

# Connection management functions
def get_client():
    """Get the SceneTalk client instance."""
    global _client
    assert _client is not None, "Client not initialized"
    return _client

    
def get_connection_state():
    """Get the current connection state."""
    global _client
    if _client is None:
        return "uninitialized"
    return _client.connection_state

def connect_to_server(endpoint):
    """Connect to the Houdini server."""
    global _connection_thread

    # Cancel any existing connection attempt before trying to connect again
    if _connection_thread and _connection_thread.is_alive():
        return False
    
    async def connect_task():
        client = get_client()
        client.ws_url = endpoint
        await client.connect()
    
    _connection_thread = run_async(connect_task())
    return True

def disconnect_from_server():
    """Disconnect from the Houdini server."""
    global _connection_thread
    
    client = get_client()
    
    async def disconnect_task():
        await client.disconnect()
    
    _connection_thread = run_async(disconnect_task())
    return True