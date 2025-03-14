import logging
import os
import re
import zipfile

import httpx

from api_client import MythicaClient

log = logging.getLogger(__name__)

def download_file(url, output_path):
    write_location = None
    with httpx.stream("GET", url) as response:
        response.raise_for_status()  # Raise an exception for 4XX/5XX responses

        # Get filename from Content-Disposition header if available
        filename = None
        if "content-disposition" in response.headers:
            content_disposition = response.headers["content-disposition"]
            filename_match = re.search(r'filename="?([^"]+)"?', content_disposition)
            if filename_match:
                filename = filename_match.group(1)

        # If no filename found in headers, extract from URL
        if not filename:
            filename = url.split("/")[-1]

        write_location = os.path.join(output_path, filename)
        with open(write_location, 'wb') as file:
            # Iterate through the response data in chunks
            for chunk in response.iter_bytes(chunk_size=8192):
                file.write(chunk)

    if write_location and write_location.endswith("zip"):
        # Check if it's a ZIP file and extract if it is
        if filename.lower().endswith('.zip'):
            extraction_location = os.path.join(output_path, filename.replace(".zip", ""))
            print(f"Extracting ZIP file to {extraction_location}")
            with zipfile.ZipFile(write_location, 'r') as zip_ref:
                zip_ref.extractall(extraction_location)
            return output_path, output_path

    log.info("file downloaded %s", output_path)


async def package_resolver(endpoint, cache_path, resolve_queue, shutdown_event):
    async with MythicaClient(endpoint=endpoint) as client:
        log.info("resolving packages to: %s", cache_path)
        while not shutdown_event.is_set():
            resolve_work_req = await resolve_queue.get()
            req = resolve_work_req.req
            if req.package.asset_id is not None:
                asset = await client.get_asset(req.package.asset_id, req.package.version)
            else:
                asset = await client.get_asset_by_name(req.package.package_name, req.package.version)
            if asset is None:
                raise ValueError(f"package not found {req.package}")

            if asset["package_id"] is None:
                raise ValueError(f"package not created for asset {req.package}")
            file_info = await client.get_download_info(asset["package_id"])
            log.info("resolved package: %s, %s", asset, file_info)
            download_file(file_info["url"], cache_path)
            resolve_work_req.event.set()
