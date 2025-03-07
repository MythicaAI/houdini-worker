## Building and Running

You will need SFX_CLIENT_ID and SFX_CLIENT_SECRET from SideFX.com 

testing.env
```
SFX_CLIENT_ID=<client-id>
SFX_CLIENT_SECRET=<client-secret>
```

Build the image
docker build . -t houdini_hdk

Run the image with the port open
docker run -it --rm --env-file testing.env -p 8765:8765 houdini_hdk

Start test_client.html
