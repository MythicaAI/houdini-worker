#!/usr/bin/env python3
import asyncio
import json
import re
import os
import datetime
from tempfile import TemporaryDirectory

import httpx
import argparse
import logging
from pydantic import BaseModel, ValidationError
from httpx_ws import aconnect_ws

from advertise import advertise
from api_client import MythicaClient
from launch_worker import get_worker_cmd, launch_worker
from models import AsyncWorkRequest, PackageRef, ResolveForCookRequest
from package_resolver import package_resolver

log = logging.getLogger(__name__)
temp = TemporaryDirectory()

def setup_logging(log_dir=None):
    """Configure logging to both console and file if log_dir is provided."""
    root_logger = logging.getLogger()
    root_logger.setLevel(logging.INFO)
    
    for handler in root_logger.handlers[:]:
        root_logger.removeHandler(handler)
    
    console_handler = logging.StreamHandler()
    console_handler.setLevel(logging.INFO)
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    console_handler.setFormatter(formatter)
    root_logger.addHandler(console_handler)
    
    if log_dir:
        os.makedirs(log_dir, exist_ok=True)
        timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
        log_file = os.path.join(log_dir, f"scenetalk_{timestamp}.log")
        file_handler = logging.FileHandler(log_file)
        file_handler.setLevel(logging.INFO)
        file_handler.setFormatter(formatter)
        root_logger.addHandler(file_handler)
        log.info("Logging to %s", log_file)

def parse_package_entrypoint(package_string):
    """
    Parse a string in the format 'package-version/entrypoint' into its components.

    Args:
        package_string (str): String in the format 'package-version/entrypoint'
            Examples: 'numpy-1.24.3/array_functions', 'django-4.2.0/views'

    Returns:
        tuple: (package_name, version, entrypoint) if valid format
               None if invalid format
    """
    # Pattern to match: package-version/entrypoint
    pattern = r'^([a-zA-Z0-9_.-]+)-([0-9]+(?:\.[0-9]+)*(?:[-_][a-zA-Z0-9]+)?)(?:/([a-zA-Z0-9_.-]+))?$'
    match = re.match(pattern, package_string)
    if match:
        package_name = match.group(1)
        version = match.group(2)
        entry_point = match.group(3) if match.group(3) else None
        return package_name, version, entry_point
    else:
        raise ValueError(f"Invalid format. Use 'package-version/entrypoint'.")


async def stdin_command_task(resolve_queue, shutdown_event):
    """
    Task to read user input from stdin, process 'resolve packagename-version' commands,
    send them to the resolve_queue, and print results from the response_queue.
    """
    while not shutdown_event.is_set():
        user_input = await asyncio.to_thread(input, "Enter command: ")
        await exec_cmd(user_input, resolve_queue, shutdown_event)


async def exec_cmd(cmd_text, resolve_queue, shutdown_event):
    if cmd_text.startswith("exit"):
        shutdown_event.set()

    elif cmd_text.startswith("resolve "):
        _, package_info = cmd_text.split(maxsplit=1)
        package_name, version, entry_point = parse_package_entrypoint(package_info)
        package_ref = PackageRef(
            asset_id=None,
            package_name=package_name,
            version=version,
            entry_point=entry_point)
        req = ResolveForCookRequest(package=package_ref)
        log.info("queueing resolve request: %s", req)
        await resolve_queue.put(AsyncWorkRequest(req))
    else:
        log.error("Invalid command. Use 'resolve packagename-version'.")


def parse_args():
    # Parse command-line arguments.
    parser = argparse.ArgumentParser(description="Houdini sidecar service.")
    parser.add_argument(
        "--cache-path",
        required=False,
        default=temp.name,
        help="File system path for package caching.")
    parser.add_argument(
        "--endpoint",
        required=False,
        default="https://api.mythica.gg",
        help="Base URL for sending HTTP requests.")
    parser.add_argument(
        "--worker",
        required=False,
        default=None,
        help="Path to the Houdini-Worker executable."
    )
    parser.add_argument(
        "--clientport",
        required=False,
        type=int,
        default=8765,
        help="Port for client websocket connections.")
    parser.add_argument(
        "--adminport",
        required=False,
        type=int,
        default=9876,
        help="Port for admin websocket connections.")
    parser.add_argument(
        "--stdin",
        required=False,
        action="store_true",
        default=False,
        help="Enable console commands from stdin."
    )
    parser.add_argument(
        "--exec",
        required=False,
        help="Command to execute."
    )
    parser.add_argument(
        "--advertise",
        required=False,
        default=None,
        help="Take an optional public endpoint to advertise this server."
    )
    parser.add_argument(
        "--logdir",
        required=False,
        default=None,
        help="Directory to store log files."
    )
    return parser.parse_args()


async def main():
    args = parse_args()

    setup_logging(args.logdir)

    exe_path = "./houdini_worker"
    admin_ws_endpoint = f"ws://localhost:{args.adminport}"
    client_ws_endpoint = f"ws://0.0.0.0:{args.clientport}"
    worker_cmd = get_worker_cmd(
        exe_path,
        client_ws_endpoint,
        admin_ws_endpoint)

    resolve_queue = asyncio.Queue()
    response_queue = asyncio.Queue()
    shutdown_event = asyncio.Event()

    tasks = [asyncio.create_task(
        package_resolver(
            args.endpoint,
            args.cache_path,
            resolve_queue,
            response_queue,
            shutdown_event))]

    # Launch the worker if specified and connect to its admin
    # port to receive resolve requests
    if args.worker is not None:
        tasks.append(asyncio.create_task(
            launch_worker(
                worker_cmd,
                admin_ws_endpoint,
                resolve_queue,
                response_queue,
                shutdown_event)))

    # Accept commands from STDIN
    if args.stdin:
        tasks.append(asyncio.create_task(
            stdin_command_task(
                resolve_queue,
                shutdown_event)))

    if args.exec:
        await exec_cmd(args.exec, resolve_queue, shutdown_event)

    if args.advertise:
        tasks.append(asyncio.create_task(
            advertise(args.endpoint,
                      args.advertise,
                      shutdown_event)))

    await asyncio.gather(*tasks)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("Houdini-sidecar terminated via keyboard interrupt.")
