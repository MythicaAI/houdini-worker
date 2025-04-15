Here’s a **cleaned-up and clearly structured version** of the **Scene Talk RFC (Draft 5)**, written specifically for **technical artists** to review and provide feedback. It avoids overly technical jargon while preserving precision and structure.

---

# 🎙️ Scene Talk Protocol – Draft 5

**Purpose:**  
Scene Talk is a lightweight binary protocol for streaming 3D scene data in real time between tools like **Houdini**, **Unreal**, **Blender**, and **USD pipelines**.  
It’s designed to be fast, structured, and friendly to procedural workflows.

---

## 🧠 What Does Scene Talk Do?

Scene Talk sends **structured scene data** over WebSocket.  
It describes:

- What the object is (`Mesh`, `Volume`, etc.)
- How it’s transformed (`matrix`, `objectUp`, etc.)
- How it’s organized (`Begin` / `End` like a folder structure)
- How it's animated (time-sampled attributes)
- How it fits into layers and references (like USD)

---

## 🧱 Core Concepts

### ✅ Frame Types

| Code | Name        | Purpose                            |
|------|-------------|------------------------------------|
| `H`  | Hello       | Starts the stream                  |
| `B`  | Begin       | Begin a new prim or context        |
| `E`  | End         | Close the most recent `Begin`      |
| `S`  | Attribute   | Send a named value (position, etc) |
| `A`  | Animation   | Time-sampled attribute value       |
| `T`  | Status      | Logging, diagnostics               |

Each frame has a header + payload.  
Data is **binary**, not text, and packed tightly for speed.

---

## 🧾 Attributes

### 🎯 Attributes describe your object

Each `S` or `A` frame has:

- A **name** (like `"points"` or `"transform"`)
- A **value type** (like `vec3f`, `str`, etc.)
- A **value** (actual data)
- A **2-byte payload length**

### 🧩 Core Attribute Registry

Here are the standard attributes Scene Talk understands:

| Name               | Type     | Purpose                                  |
|--------------------|----------|------------------------------------------|
| `typeName`         | `str`    | What kind of thing this is (`Mesh`, `Volume`, etc.) |
| `kind`             | `str`    | USD prim kind (`component`, etc.)        |
| `name`             | `str`    | Prim name                                |
| `parent`           | `str`    | Parent path for partial scenes           |
| `layer`            | `str`    | Layer this belongs to                    |
| `layerRef`         | `str`    | External reference file path             |
| `sceneScaleMM`     | `u32`    | Scene scale in millimeters (1=mm, 100=cm, 1000=m) |
| `objectUp`         | `vec3f`  | Defines the "up" direction               |
| `objectRight`      | `vec3f`  | Right direction                          |
| `objectFront`      | `vec3f`  | Forward direction                        |
| `transform`        | `mat4f`  | 4x4 matrix transform (row-major)         |

---

### 🧱 Geometry Attributes

| Name               | Type        | Purpose                      |
|--------------------|-------------|------------------------------|
| `points`           | `vecND`     | Positions (e.g. vec3 per point) |
| `normals`          | `vecND`     | Normals (same structure)     |
| `velocities`       | `vecND`     | Optional motion blur data    |
| `faceVertexCounts` | `u32[]`     | Vertices per face            |
| `faceVertexIndices`| `u32[]`     | Flattened list of face indices |
| `displayColor`     | `vec3f[]`   | Color per point or face      |
| `opacity`          | `f32[]`     | Opacity values               |
| `materialBinding`  | `str`       | Bound material path          |
| `purpose`          | `str`       | USD-style `"render"`, `"proxy"`, etc. |

---

### 🕒 Animated Attributes

Use `A` frames (Animation Attribute) for animated values:

```plaintext
A frame:
- name = "transform"
- type = mat4f
- time = 1.0
- value = <matrix>
```

Send one `A` frame per time sample (e.g., per frame of animation).

---

## 🗺️ Object Orientation and Transform

### Global Object Orientation

Scene Talk lets the sender define **how identity transforms are interpreted**:

| Attribute     | Description                     |
|---------------|---------------------------------|
| `objectUp`    | Vector pointing "up" (e.g., Z+) |
| `objectRight` | Vector pointing right           |
| `objectFront` | Vector pointing forward         |

### Scene Scale

Use `sceneScaleMM` to tell the receiver what 1 unit means:

| Value | Meaning        |
|-------|----------------|
| 1     | 1mm            |
| 100   | 1cm            |
| 1000  | 1m             |

---

## 🧭 Layering and References

Scene Talk supports USD-style layering:

| Attribute  | Purpose |
|------------|---------|
| `layer`    | Which layer the object belongs to |
| `layerRef` | Load another file/layer by path   |

You can send a `Begin` with just a `layerRef` to "inject" external data.

---

## 🧬 Parenting and Partial Graphs

You don’t have to send the scene top-down.

Send a prim **with a parent path**, and the receiver will attach it properly:

```plaintext
parent = "/World/Character"
```

This makes Scene Talk good for live editing or procedural generators.

---

## 📏 Max Attribute Size Guarantee

We guarantee that **every Scene Talk frame** is big enough to hold:

- One full attribute (e.g., `vec4f` × N)
- A name, type, and metadata

You’ll never need to split a single per-point attribute into partials unless you're streaming huge arrays — and even then, we provide support for **partial frames** and **indexed arrays** via `vecND`.

---

## ✅ What This Enables

- ✅ Fast, real-time procedural streaming of scenes
- ✅ Robust support for USD-like features
- ✅ Partial scene delivery + parenting
- ✅ Semantic transform handling
- ✅ Easily extensible for more prim types

---

## 💬 We Want Your Feedback

We’re looking for input from **technical artists**, **tool developers**, and **pipeline engineers**:

- Are the attribute names clear and sufficient?
- Anything missing for your USD workflows?
- Would you want to add more types (e.g., camera intrinsics, primvars)?
- What kind of editor/debug tooling would help?

---

Let me know if you'd like this exported to PDF, Notion, or as a GitHub `.md` file with diagrams.
