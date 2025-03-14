import asyncio
from typing import Optional
from uuid import UUID

from pydantic import BaseModel

class AsyncWorkRequest:
    def __init__(self, req):
        self.req = req
        self.event = asyncio.Event()


class PackageRef(BaseModel):
    asset_id: Optional[str]
    package_name: Optional[str]
    version: str
    entry_point: str


# A Pydantic model to validate the "resolve-for-cook" package requests
class ResolveForCookRequest(BaseModel):
    op: str = "resolve-for-cook"
    package: PackageRef


class Hello(BaseModel):
    op: str = "hello"


class Log(BaseModel):
    op: str = "log"
    level: str = "info"
    message: str = ""


class WorkerAdvertisement(BaseModel):
    host_keys: list[str]
    process_id: str
    public_endpoint: str
    controller_id: UUID
