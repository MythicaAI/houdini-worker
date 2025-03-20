import os
from uuid import uuid4

import httpx
import time
import logging
from typing import Dict, List, Optional, Any, Union

from models import WorkerAdvertisement

log = logging.getLogger(__name__)


def _validate_and_parse_response(response: httpx.Response):
    """
    Validate and parse the response, raising appropriate exceptions for failures.
    """
    response.raise_for_status()
    return response.json()


def _format_version(asset: Dict[str, Any]) -> str:
    """Format the version array as a string and add it to the asset dict."""
    return ".".join(map(str, asset.get("version", "unknown"))) \
        if isinstance(asset.get("version"), list) else "unknown"


class MythicaClient:
    """
    A client for the Mythica API that maintains a connection pool.
    """
    DEFAULT_RETRY_ATTEMPTS = 3
    DEFAULT_RETRY_BACKOFF = 0.5

    def __init__(
            self,
            endpoint: str = "https://api.mythica.gg/",
            timeout: int = 10,
            limits: Optional[httpx.Limits] = None,
            retry_attempts: int = DEFAULT_RETRY_ATTEMPTS,
            retry_backoff: float = DEFAULT_RETRY_BACKOFF,
    ):
        """
        Initialize a Mythica API client with a persistent connection pool.
        """
        self.endpoint = endpoint
        self.timeout = timeout
        self.retry_attempts = retry_attempts
        self.retry_backoff = retry_backoff
        self.client = httpx.AsyncClient(
            base_url=endpoint,
            timeout=timeout,
            limits=limits or httpx.Limits(max_connections=10, max_keepalive_connections=5),
            http2=False,
        )

    async def __aenter__(self):
        return self

    async def __aexit__(self, exc_type, exc_val, exc_tb):
        await self.client.aclose()

    async def _get_with_retry(self, url: str) -> Optional[httpx.Response]:
        """
        Make GET request with retry logic for transient failures.
        """
        for attempt in range(self.retry_attempts):
            try:
                response = await self.client.get(url)
                if response.status_code < 500:
                    return response
            except (httpx.ConnectError, httpx.ReadTimeout, httpx.WriteTimeout) as e:
                if attempt == self.retry_attempts - 1:
                    raise RuntimeError(f"Request failed after {self.retry_attempts} attempts") from e
            time.sleep(self.retry_backoff * (2 ** attempt))
        return None

    async def _post_with_retry(self, url: str) -> Optional[httpx.Response]:
        """
        Make GET request with retry logic for transient failures.
        """
        for attempt in range(self.retry_attempts):
            try:
                response = await self.client.post(url)
                if response.status_code < 500:
                    return response
            except (httpx.ConnectError, httpx.ReadTimeout, httpx.WriteTimeout) as e:
                if attempt == self.retry_attempts - 1:
                    raise RuntimeError(f"Request failed after {self.retry_attempts} attempts") from e
            time.sleep(self.retry_backoff * (2 ** attempt))
        return None

    async def get_asset(self, asset_id: str, version: str) -> Dict[str, Any]:
        """
        Fetch asset details by ID and version.
        """
        url = f"/v1/assets/{asset_id}/versions/{version}"
        response = await self._get_with_retry(url)
        return _validate_and_parse_response(response)

    async def get_asset_by_name(self, asset_name: str, version: str) -> Dict[str, Any]:
        """
        Fetch asset details by name and version.
        """
        url = f"/v1/assets/named/{asset_name}"
        response = await self._get_with_retry(url)
        assets = _validate_and_parse_response(response)

        if not assets:
            return None

        versions = []
        for asset in assets:
            version_str = _format_version(asset)
            versions.append(version_str)
            if version_str == version:
                return asset
        raise ValueError(f"Asset {asset_name} with version {version} not found. Available versions: {versions}")

    async def get_download_info(self, file_id: str) -> Dict[str, Any]:
        """
        Fetch download info for a given file ID.
        """
        url = f"/v1/download/info/{file_id}"
        response = await self._get_with_retry(url)
        return _validate_and_parse_response(response)


    async def advertise(
            self,
            id: uuid4,
            public_endpoint: Optional[str]):
        process_id = str(os.getpid())
        WorkerAdvertisement(
            controller_id=id,
            process_id=process_id,
            public_endpoint=public_endpoint,
            host_keys=[])
        url = f"/v1/workers/advertise"
        response = await self._post_with_retry(url)
        return _validate_and_parse_response(response)