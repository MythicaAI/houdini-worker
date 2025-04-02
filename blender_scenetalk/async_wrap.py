import asyncio
import threading
import concurrent.futures
from functools import wraps

# Global event loop and thread
_loop = None
_thread = None
_running = False
_lock = threading.Lock()

def ensure_event_loop_running():
    """Ensure the global event loop is running in a background thread."""
    global _loop, _thread, _running
    
    with _lock:
        if _running:
            return _loop
            
        # Create new event loop
        _loop = asyncio.new_event_loop()
        _running = True
        
        # Start event loop in a background thread
        def run_event_loop():
            asyncio.set_event_loop(_loop)
            try:
                _loop.run_forever()
            finally:
                _loop.run_until_complete(_loop.shutdown_asyncgens())
                _loop.close()
        
        _thread = threading.Thread(target=run_event_loop, daemon=True)
        _thread.start()
        
        return _loop

def run_async(coro, timeout=None):
    """
    Run an async coroutine from synchronous code and return the result.
    
    Args:
        coro: The coroutine to run
        timeout: Optional timeout in seconds
        
    Returns:
        The result of the coroutine
    """
    loop = ensure_event_loop_running()
    future = asyncio.run_coroutine_threadsafe(coro, loop)
    
    try:
        return future.result(timeout=timeout)
    except concurrent.futures.TimeoutError:
        future.cancel()
        raise TimeoutError(f"Operation timed out after {timeout} seconds")