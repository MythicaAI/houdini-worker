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

def stop_event_loop():
    """Stop the global event loop."""
    global _loop, _thread, _running
    with _lock:
        if _running:
            _running = False
            _loop.call_soon_threadsafe(_loop.stop)
            _thread.join()


def run_async_bg(coro):
    """
    Run an async coroutine from synchronous code without waiting for the result.
    
    Args:
        coro: The coroutine to run
    """
    loop = ensure_event_loop_running()
    return asyncio.run_coroutine_threadsafe(coro, loop)