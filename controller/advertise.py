import asyncio
import logging
from typing import Optional
from uuid import uuid4
from pydantic import BaseModel

from api_client import MythicaClient

log = logging.getLogger(__name__)
identity = uuid4()

async def advertise(
        api_endpoint: str,
        advertise_endpoint: Optional[str],
        shutdown_event: asyncio.Event):
    while not shutdown_event.is_set():
        async with MythicaClient(endpoint=api_endpoint) as client:
            await client.advertise(identity, advertise_endpoint)
            await asyncio.sleep(3)