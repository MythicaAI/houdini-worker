# SceneTalk

This repo is the WIP evolution of the Houdini automation infrastructure behind [Mythica GG](https://api.mythica.gg).

It provides a WebSocket and framing to do real-time 3D scene interchange.

[RFC](./RFC.md) for binary protocol is under development


**In this repo:**
* Dockerfile to run Houdini HDK inside a web server
* Test HTML client
* Test Python client
* Blender Plugin to use the protocol
* Controller script that hosts the server process
* Demo HDAs


## Building and Running

You will need **SFX_CLIENT_ID** and **SFX_CLIENT_SECRET** from SideFX.com 

Create a local environment file with your Houdini license key information:

testing.env
```
SFX_CLIENT_ID=<client-id>
SFX_CLIENT_SECRET=<client-secret>
MYTHICA_ENDPOINT=http://host.docker.internal:8080 #LINUX 
```

Build the image

```bash
docker build . -t mythica-scenetalk
```

Run the image

```bash
docker run -it --rm \
  --env-file testing.env \
  -p 8765:8765 \
  --name scentalk \
  mythica-scenetalk

#For Linux:
docker run -it --rm \
  --env-file testing.env \
  -p 8765:8765 \
  --name scentalk \
  mythica-scenetalk \
  --add-host=host.docker.internal:host-gateway \
  --network storage \
  --name scenetalk \
  mythica-scenetalk
```

Open test_client.html to test the web client

## Installing the Blender Plugin

Zip up blender_scenetalk and drop it onto Blender 4.2 LTS to test from Blender.

_Insert helpful YouTube link here_
