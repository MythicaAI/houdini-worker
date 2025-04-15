# SceneTalk

This repo is a WIP evolution of the Houdini automation infrastructure behind https://api.mythica.gg

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
```

Build the image

`docker build . -t scenetalk`

Run the image

`docker run -it --rm --env-file testing.env -p 8765:8765 scenetalk`

Open test_client.html to test the web client

## Installing the Blender Plugin

Zip up blender_scenetalk and drop it onto Blender 4.2 LTS to test from Blender.

_Insert helpful YouTube link here_
