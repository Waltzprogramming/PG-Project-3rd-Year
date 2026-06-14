import json
import math
import os

import bpy
from mathutils import Vector


SOURCE_DIR = r"C:\Users\sasuk\Downloads\1"
OBJ_PATH = os.path.join(SOURCE_DIR, "3ad75d7d8317bffe70dbd81e8112f575.obj")
OUTPUT_DIR = r"C:\Users\sasuk\source\repos\PG\character_output\analysis"


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)


def world_bounds(obj):
    corners = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    minimum = Vector((min(v.x for v in corners), min(v.y for v in corners), min(v.z for v in corners)))
    maximum = Vector((max(v.x for v in corners), max(v.y for v in corners), max(v.z for v in corners)))
    return minimum, maximum


def point_camera(camera, target):
    direction = target - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()


def add_material(mesh):
    material = bpy.data.materials.new("Character_PBR")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])

    texture_specs = [
        ("texture_pbr_20250901.png", "Base Color", "sRGB"),
        ("texture_pbr_20250901_metallic.png", "Metallic", "Non-Color"),
        ("texture_pbr_20250901_roughness.png", "Roughness", "Non-Color"),
    ]
    for filename, socket_name, color_space in texture_specs:
        image = bpy.data.images.load(os.path.join(SOURCE_DIR, filename), check_existing=True)
        image.colorspace_settings.name = color_space
        texture = nodes.new("ShaderNodeTexImage")
        texture.image = image
        links.new(texture.outputs["Color"], shader.inputs[socket_name])

    normal_image = bpy.data.images.load(
        os.path.join(SOURCE_DIR, "texture_pbr_20250901_normal.png"),
        check_existing=True,
    )
    normal_image.colorspace_settings.name = "Non-Color"
    normal_texture = nodes.new("ShaderNodeTexImage")
    normal_texture.image = normal_image
    normal_map = nodes.new("ShaderNodeNormalMap")
    normal_map.inputs["Strength"].default_value = 0.65
    links.new(normal_texture.outputs["Color"], normal_map.inputs["Color"])
    links.new(normal_map.outputs["Normal"], shader.inputs["Normal"])

    mesh.data.materials.clear()
    mesh.data.materials.append(material)


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    clear_scene()
    bpy.ops.wm.obj_import(filepath=OBJ_PATH)
    meshes = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
    if len(meshes) != 1:
        raise RuntimeError(f"Expected one mesh, found {len(meshes)}")
    mesh = meshes[0]
    mesh.name = "Character"
    add_material(mesh)

    bpy.context.view_layer.objects.active = mesh
    mesh.select_set(True)
    bpy.ops.object.shade_smooth()

    minimum, maximum = world_bounds(mesh)
    center = (minimum + maximum) * 0.5
    dimensions = maximum - minimum
    max_dimension = max(dimensions)

    bpy.ops.object.light_add(type="AREA", location=center + Vector((max_dimension, -max_dimension, max_dimension)))
    key = bpy.context.object
    key.data.energy = 1300
    key.data.shape = "DISK"
    key.data.size = max_dimension

    bpy.ops.object.light_add(type="AREA", location=center + Vector((-max_dimension, -0.4 * max_dimension, 0.5 * max_dimension)))
    fill = bpy.context.object
    fill.data.energy = 700
    fill.data.size = max_dimension

    bpy.ops.object.light_add(type="AREA", location=center + Vector((0, max_dimension, max_dimension)))
    rim = bpy.context.object
    rim.data.energy = 900
    rim.data.size = 0.8 * max_dimension

    bpy.ops.object.camera_add()
    camera = bpy.context.object
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = max_dimension * 1.15
    bpy.context.scene.camera = camera

    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 700
    scene.render.resolution_y = 700
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.world.color = (0.035, 0.035, 0.035)

    distance = max_dimension * 3
    views = {
        "x_positive": Vector((distance, 0, 0)),
        "x_negative": Vector((-distance, 0, 0)),
        "y_positive": Vector((0, distance, 0)),
        "y_negative": Vector((0, -distance, 0)),
        "z_positive": Vector((0, 0, distance)),
    }
    for name, offset in views.items():
        camera.location = center + offset
        point_camera(camera, center)
        scene.render.filepath = os.path.join(OUTPUT_DIR, f"{name}.png")
        bpy.ops.render.render(write_still=True)

    details = {
        "minimum": list(minimum),
        "maximum": list(maximum),
        "center": list(center),
        "dimensions": list(dimensions),
        "vertices": len(mesh.data.vertices),
        "polygons": len(mesh.data.polygons),
        "uv_layers": len(mesh.data.uv_layers),
        "loose_parts": None,
    }
    with open(os.path.join(OUTPUT_DIR, "analysis.json"), "w", encoding="utf-8") as handle:
        json.dump(details, handle, indent=2)
    print(json.dumps(details, indent=2))


if __name__ == "__main__":
    main()
