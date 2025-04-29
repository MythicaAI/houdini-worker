# SceneTalk Blender Plugin

Example Blender plugin for real time control our [HDK server](../README.md)

## Installing

Get your Python interpreter:

```bash
<blender-path> --python-expr "import sys; print(sys.executable)"
```

Ensure you have PIP installed

```bash
path/to/python -m ensurepip
```

Install requirements

```
# optionally create a virtual environment
pip install -r requirements.txt
```

Zip the folder and drop it onto Blender

## Connecting to a Server

Use the connection pannel to connect to a host machine. 

By default the plugin connects to wss://scenetalk.mythica.gg/ws/

## HDAs

The plugin current exposes the demo HDAS in [the assets folder](../assets) via a drop down. There is a panel file for each HDA. Dynamic panels are not yet a feature although should be next on the roadmap with `-myth.zip` support.

