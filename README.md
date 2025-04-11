# Houdini Worker

This repo is a WIP evolution of the Houdini automation infrastructure behind https://api.mythica.gg

* WebSocket HDK wrapper of Houdini Engine
* Controller script that hosts the server process
* Example web page 
* Example Blender integration of the WebSocket protocol
* Demo HDAs that can be used from Blender


## Building and Running

You will need SFX_CLIENT_ID and SFX_CLIENT_SECRET from SideFX.com 

Create a local environment file with you Houdini license key information:

testing.env
```
SFX_CLIENT_ID=<client-id>
SFX_CLIENT_SECRET=<client-secret>
```

Build the image

`docker build . -t houdini_hdk`

Run the image with the port open:
`docker run -it --rm --env-file testing.env -p 8765:8765 houdini_hdk`

Open test_client.html to test the web client

Zip up blender_scenetalk and drop it onto Blender 4.2 LTS to test from Blender.
