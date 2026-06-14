import json

import bpy


GLB_PATH = r"C:\Users\sasuk\source\repos\PG\character_output\pinkx_character_animated.glb"
REPORT_PATH = r"C:\Users\sasuk\source\repos\PG\character_output\validation.json"


bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.gltf(filepath=GLB_PATH)

meshes = [
    obj
    for obj in bpy.context.scene.objects
    if obj.type == "MESH"
    and all(collection.name != "glTF_not_exported" for collection in obj.users_collection)
]
armatures = [obj for obj in bpy.context.scene.objects if obj.type == "ARMATURE"]
actions = sorted(
    (
        {
            "name": action.name,
            "frame_start": float(action.frame_range[0]),
            "frame_end": float(action.frame_range[1]),
        }
        for action in bpy.data.actions
    ),
    key=lambda item: item["name"],
)

report = {
    "mesh_count": len(meshes),
    "mesh_names": [obj.name for obj in meshes],
    "armature_count": len(armatures),
    "mesh_vertices": sum(len(obj.data.vertices) for obj in meshes),
    "bones": sum(len(obj.data.bones) for obj in armatures),
    "actions": actions,
    "materials": sorted(material.name for material in bpy.data.materials),
    "images": sorted(image.name for image in bpy.data.images),
}

if len(meshes) != 1:
    raise RuntimeError(f"Expected 1 mesh after GLB import, found {len(meshes)}")
if len(armatures) != 1:
    raise RuntimeError(f"Expected 1 armature after GLB import, found {len(armatures)}")
action_names = {item["name"] for item in actions}
if not {"Walk_Stylized", "Jump_Stylized"}.issubset(action_names):
    raise RuntimeError(f"Missing animation actions: {action_names}")

with open(REPORT_PATH, "w", encoding="utf-8") as handle:
    json.dump(report, handle, indent=2)
print(json.dumps(report, indent=2))
