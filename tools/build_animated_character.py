import math
import os

import bpy
from mathutils import Vector


SOURCE_DIR = r"C:\Users\sasuk\Downloads\1"
OBJ_PATH = os.path.join(SOURCE_DIR, "3ad75d7d8317bffe70dbd81e8112f575.obj")
OUTPUT_DIR = r"C:\Users\sasuk\source\repos\PG\character_output"
BLEND_PATH = os.path.join(OUTPUT_DIR, "pinkx_character_animated.blend")
GLB_PATH = os.path.join(OUTPUT_DIR, "pinkx_character_animated.glb")

FPS = 24


def clear_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)
    for block in (bpy.data.armatures, bpy.data.materials, bpy.data.cameras, bpy.data.lights):
        for item in list(block):
            if item.users == 0:
                block.remove(item)


def import_character():
    bpy.ops.wm.obj_import(filepath=OBJ_PATH)
    meshes = [obj for obj in bpy.context.selected_objects if obj.type == "MESH"]
    if len(meshes) != 1:
        raise RuntimeError(f"Expected one mesh, found {len(meshes)}")
    mesh = meshes[0]
    mesh.name = "PINKX_Character"
    mesh.data.name = "PINKX_Character_Mesh"
    bpy.context.view_layer.objects.active = mesh
    mesh.select_set(True)
    bpy.ops.object.shade_smooth()
    return mesh


def build_material(mesh):
    material = bpy.data.materials.new("PINKX_PBR")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    shader = nodes.new("ShaderNodeBsdfPrincipled")
    shader.inputs["Metallic"].default_value = 0.0
    shader.inputs["Roughness"].default_value = 0.72
    links.new(shader.outputs["BSDF"], output.inputs["Surface"])

    texture_specs = (
        ("texture_pbr_20250901.png", "Base Color", "sRGB"),
        ("texture_pbr_20250901_metallic.png", "Metallic", "Non-Color"),
        ("texture_pbr_20250901_roughness.png", "Roughness", "Non-Color"),
    )
    for filename, input_name, color_space in texture_specs:
        image = bpy.data.images.load(os.path.join(SOURCE_DIR, filename), check_existing=True)
        image.colorspace_settings.name = color_space
        texture = nodes.new("ShaderNodeTexImage")
        texture.name = f"{input_name}_Texture"
        texture.image = image
        links.new(texture.outputs["Color"], shader.inputs[input_name])

    normal_image = bpy.data.images.load(
        os.path.join(SOURCE_DIR, "texture_pbr_20250901_normal.png"),
        check_existing=True,
    )
    normal_image.colorspace_settings.name = "Non-Color"
    normal_texture = nodes.new("ShaderNodeTexImage")
    normal_texture.name = "Normal_Texture"
    normal_texture.image = normal_image
    normal_map = nodes.new("ShaderNodeNormalMap")
    normal_map.inputs["Strength"].default_value = 0.55
    links.new(normal_texture.outputs["Color"], normal_map.inputs["Color"])
    links.new(normal_map.outputs["Normal"], shader.inputs["Normal"])

    mesh.data.materials.clear()
    mesh.data.materials.append(material)


def add_edit_bone(armature, name, head, tail, parent=None, connected=False, deform=True):
    bone = armature.data.edit_bones.new(name)
    bone.head = head
    bone.tail = tail
    bone.parent = parent
    bone.use_connect = connected
    bone.use_deform = deform
    return bone


def build_armature():
    armature_data = bpy.data.armatures.new("PINKX_Rig")
    armature = bpy.data.objects.new("PINKX_Rig", armature_data)
    bpy.context.collection.objects.link(armature)
    armature.show_in_front = True
    armature.data.display_type = "BBONE"

    bpy.context.view_layer.objects.active = armature
    armature.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")

    root = add_edit_bone(
        armature,
        "root",
        Vector((0.0, 0.0, 0.02)),
        Vector((0.0, 0.0, 0.10)),
        deform=False,
    )
    pelvis = add_edit_bone(
        armature,
        "pelvis",
        Vector((0.0, 0.0, 0.34)),
        Vector((0.0, 0.0, 0.48)),
        root,
    )
    spine = add_edit_bone(
        armature,
        "spine",
        Vector((0.0, 0.0, 0.48)),
        Vector((0.0, 0.0, 0.66)),
        pelvis,
        connected=True,
    )
    chest = add_edit_bone(
        armature,
        "chest",
        Vector((0.0, 0.0, 0.66)),
        Vector((0.0, 0.0, 0.77)),
        spine,
        connected=True,
    )
    neck = add_edit_bone(
        armature,
        "neck",
        Vector((0.0, 0.0, 0.77)),
        Vector((0.0, 0.0, 0.83)),
        chest,
        connected=True,
    )
    add_edit_bone(
        armature,
        "head",
        Vector((0.0, 0.0, 0.83)),
        Vector((0.0, 0.0, 1.04)),
        neck,
        connected=True,
    )

    for side, sign in (("L", 1.0), ("R", -1.0)):
        thigh = add_edit_bone(
            armature,
            f"thigh.{side}",
            Vector((0.095 * sign, 0.0, 0.40)),
            Vector((0.10 * sign, 0.0, 0.275)),
            pelvis,
        )
        shin = add_edit_bone(
            armature,
            f"shin.{side}",
            Vector((0.10 * sign, 0.0, 0.275)),
            Vector((0.105 * sign, 0.0, 0.135)),
            thigh,
            connected=True,
        )
        add_edit_bone(
            armature,
            f"foot.{side}",
            Vector((0.105 * sign, 0.0, 0.135)),
            Vector((0.105 * sign, -0.13, 0.065)),
            shin,
            connected=True,
        )

        upper_arm = add_edit_bone(
            armature,
            f"upper_arm.{side}",
            Vector((0.11 * sign, 0.0, 0.72)),
            Vector((0.195 * sign, 0.0, 0.61)),
            chest,
        )
        forearm = add_edit_bone(
            armature,
            f"forearm.{side}",
            Vector((0.195 * sign, 0.0, 0.61)),
            Vector((0.225 * sign, -0.005, 0.50)),
            upper_arm,
            connected=True,
        )
        add_edit_bone(
            armature,
            f"hand.{side}",
            Vector((0.225 * sign, -0.005, 0.50)),
            Vector((0.235 * sign, -0.02, 0.405)),
            forearm,
            connected=True,
        )

    bpy.ops.object.mode_set(mode="OBJECT")
    return armature


def bind_mesh(mesh, armature):
    bpy.ops.object.select_all(action="DESELECT")
    mesh.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
    if not mesh.vertex_groups:
        raise RuntimeError("Automatic weights did not create any vertex groups")
    sanitize_weights(mesh)
    return len(mesh.vertex_groups)


def replace_weights(mesh, vertex_index, weights):
    for group in mesh.vertex_groups:
        group.remove([vertex_index])
    total = sum(weights.values())
    if total <= 0:
        return
    for group_name, weight in weights.items():
        if weight > 0:
            mesh.vertex_groups[group_name].add([vertex_index], weight / total, "REPLACE")


def linear_blend(value, start, end, first, second):
    if end <= start:
        return {second: 1.0}
    factor = max(0.0, min(1.0, (value - start) / (end - start)))
    return {first: 1.0 - factor, second: factor}


def point_segment_distance_2d(point, start, end):
    px, pz = point
    ax, az = start
    bx, bz = end
    dx = bx - ax
    dz = bz - az
    length_squared = dx * dx + dz * dz
    if length_squared == 0:
        return math.hypot(px - ax, pz - az)
    factor = max(0.0, min(1.0, ((px - ax) * dx + (pz - az) * dz) / length_squared))
    closest_x = ax + factor * dx
    closest_z = az + factor * dz
    return math.hypot(px - closest_x, pz - closest_z)


def arm_weights(side, abs_x, z):
    # The sculpted arm has no clean elbow loops, so it deforms best as one
    # rigid chibi limb pivoting from the shoulder.
    return {"upper_arm." + side: 1.0}


def sanitize_weights(mesh):
    # The source is one watertight mesh. Explicit anatomical zones prevent
    # heat weights from connecting raised arms to the face and shirt.
    for vertex in mesh.data.vertices:
        world = mesh.matrix_world @ vertex.co
        x = world.x
        z = world.z
        abs_x = abs(x)
        side = "L" if x >= 0 else "R"
        original = {
            mesh.vertex_groups[element.group].name: element.weight
            for element in vertex.groups
        }
        head_weight = original.get("head", 0.0) + original.get("neck", 0.0)
        body_weight = sum(original.get(name, 0.0) for name in ("pelvis", "spine", "chest"))
        arm_weight = sum(
            original.get(name, 0.0)
            for name in (
                "upper_arm.L",
                "forearm.L",
                "hand.L",
                "upper_arm.R",
                "forearm.R",
                "hand.R",
            )
        )
        leg_weight = sum(
            original.get(name, 0.0)
            for name in (
                "thigh.L",
                "shin.L",
                "foot.L",
                "thigh.R",
                "shin.R",
                "foot.R",
            )
        )

        if z >= 0.73 or (
            z >= 0.62
            and head_weight >= 0.10
            and head_weight >= arm_weight
            and head_weight >= body_weight * 0.5
        ):
            replace_weights(mesh, vertex.index, {"head": 1.0})
            continue

        if 0.665 <= z < 0.73 and abs_x < 0.175:
            replace_weights(mesh, vertex.index, linear_blend(z, 0.665, 0.73, "chest", "head"))
            continue

        is_arm = (
            0.32 <= z < 0.73
            and abs_x > 0.13
            and (arm_weight > body_weight * 0.45 or abs_x > 0.225)
            and arm_weight >= leg_weight
        )
        if is_arm:
            replace_weights(mesh, vertex.index, arm_weights(side, abs_x, z))
            continue

        if z < 0.43:
            if z < 0.115:
                weights = {"foot." + side: 1.0}
            elif z < 0.18:
                weights = linear_blend(z, 0.115, 0.18, "foot." + side, "shin." + side)
            elif z < 0.275:
                weights = {"shin." + side: 1.0}
            elif z < 0.345:
                weights = linear_blend(z, 0.275, 0.345, "shin." + side, "thigh." + side)
            else:
                weights = linear_blend(z, 0.345, 0.43, "thigh." + side, "pelvis")
            replace_weights(mesh, vertex.index, weights)
            continue

        if abs_x < 0.17 or body_weight >= arm_weight:
            if z < 0.49:
                weights = {"pelvis": 1.0}
            elif z < 0.57:
                weights = linear_blend(z, 0.49, 0.57, "pelvis", "spine")
            elif z < 0.65:
                weights = linear_blend(z, 0.57, 0.65, "spine", "chest")
            else:
                weights = {"chest": 1.0}
            replace_weights(mesh, vertex.index, weights)
            continue

        opposite = "R" if side == "L" else "L"
        for group_name in (
            "upper_arm." + opposite,
            "forearm." + opposite,
            "hand." + opposite,
            "thigh." + opposite,
            "shin." + opposite,
            "foot." + opposite,
            "head",
        ):
            mesh.vertex_groups[group_name].remove([vertex.index])

    for vertex in mesh.data.vertices:
        elements = list(vertex.groups)
        total = sum(element.weight for element in elements)
        if total <= 0:
            mesh.vertex_groups["pelvis"].add([vertex.index], 1.0, "REPLACE")
            continue
        for element in elements:
            mesh.vertex_groups[element.group].add(
                [vertex.index],
                element.weight / total,
                "REPLACE",
            )


def reset_pose(armature):
    for bone in armature.pose.bones:
        bone.rotation_mode = "XYZ"
        bone.location = (0.0, 0.0, 0.0)
        bone.rotation_euler = (0.0, 0.0, 0.0)
        bone.scale = (1.0, 1.0, 1.0)


def apply_pose(armature, frame, rotations=None, locations=None, scales=None):
    bpy.context.scene.frame_set(frame)
    reset_pose(armature)
    rotations = rotations or {}
    locations = locations or {}
    scales = scales or {}

    for name, degrees in rotations.items():
        armature.pose.bones[name].rotation_euler = tuple(math.radians(value) for value in degrees)
    for name, location in locations.items():
        armature.pose.bones[name].location = location
    for name, scale in scales.items():
        armature.pose.bones[name].scale = scale

    for bone in armature.pose.bones:
        bone.keyframe_insert(data_path="location", frame=frame, group=bone.name)
        bone.keyframe_insert(data_path="rotation_euler", frame=frame, group=bone.name)
        bone.keyframe_insert(data_path="scale", frame=frame, group=bone.name)


def begin_action(armature, name, start, end):
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    action.use_frame_range = True
    action.frame_start = start
    action.frame_end = end
    armature.animation_data_create()
    armature.animation_data.action = action
    reset_pose(armature)
    return action


def create_walk_action(armature):
    action = begin_action(armature, "Walk_Stylized", 1, 25)
    poses = {
        1: {
            "rot": {
                "pelvis": (1, 0, -4),
                "spine": (-5, 0, 2),
                "chest": (3, 0, 2),
                "head": (1, 0, 1),
                "thigh.L": (29, 0, 1),
                "shin.L": (-8, 0, 0),
                "foot.L": (-7, 0, 0),
                "thigh.R": (-28, 0, -1),
                "shin.R": (15, 0, 0),
                "foot.R": (10, 0, 0),
                "upper_arm.L": (-31, 0, -5),
                "forearm.L": (-12, 0, 0),
                "upper_arm.R": (31, 0, 5),
                "forearm.R": (-18, 0, 0),
            },
            "loc": {"root": (0, 0, 0.008)},
        },
        7: {
            "rot": {
                "pelvis": (3, 0, 0),
                "spine": (-7, 0, 0),
                "chest": (4, 0, 0),
                "head": (2, 0, 0),
                "thigh.L": (5, 0, 0),
                "shin.L": (24, 0, 0),
                "foot.L": (-10, 0, 0),
                "thigh.R": (-7, 0, 0),
                "shin.R": (18, 0, 0),
                "foot.R": (4, 0, 0),
                "upper_arm.L": (-5, 0, 0),
                "forearm.L": (-15, 0, 0),
                "upper_arm.R": (5, 0, 0),
                "forearm.R": (-15, 0, 0),
            },
            "loc": {"root": (0, 0, -0.014)},
            "scale": {"pelvis": (1.015, 1.015, 0.98)},
        },
        13: {
            "rot": {
                "pelvis": (1, 0, 4),
                "spine": (-5, 0, -2),
                "chest": (3, 0, -2),
                "head": (1, 0, -1),
                "thigh.L": (-28, 0, 1),
                "shin.L": (15, 0, 0),
                "foot.L": (10, 0, 0),
                "thigh.R": (29, 0, -1),
                "shin.R": (-8, 0, 0),
                "foot.R": (-7, 0, 0),
                "upper_arm.L": (31, 0, -5),
                "forearm.L": (-18, 0, 0),
                "upper_arm.R": (-31, 0, 5),
                "forearm.R": (-12, 0, 0),
            },
            "loc": {"root": (0, 0, 0.008)},
        },
        19: {
            "rot": {
                "pelvis": (3, 0, 0),
                "spine": (-7, 0, 0),
                "chest": (4, 0, 0),
                "head": (2, 0, 0),
                "thigh.L": (-7, 0, 0),
                "shin.L": (18, 0, 0),
                "foot.L": (4, 0, 0),
                "thigh.R": (5, 0, 0),
                "shin.R": (24, 0, 0),
                "foot.R": (-10, 0, 0),
                "upper_arm.L": (5, 0, 0),
                "forearm.L": (-15, 0, 0),
                "upper_arm.R": (-5, 0, 0),
                "forearm.R": (-15, 0, 0),
            },
            "loc": {"root": (0, 0, -0.014)},
            "scale": {"pelvis": (1.015, 1.015, 0.98)},
        },
    }
    poses[25] = poses[1]
    for frame, pose in poses.items():
        apply_pose(
            armature,
            frame,
            rotations=pose.get("rot"),
            locations=pose.get("loc"),
            scales=pose.get("scale"),
        )
    return action


def create_jump_action(armature):
    action = begin_action(armature, "Jump_Stylized", 1, 40)
    poses = {
        1: {
            "rot": {"spine": (-2, 0, 0), "forearm.L": (-10, 0, 0), "forearm.R": (-10, 0, 0)},
            "loc": {"root": (0, 0, 0)},
        },
        6: {
            "rot": {
                "pelvis": (12, 0, 0),
                "spine": (15, 0, 0),
                "chest": (-8, 0, 0),
                "head": (-5, 0, 0),
                "thigh.L": (28, 0, 3),
                "shin.L": (-45, 0, 0),
                "foot.L": (18, 0, 0),
                "thigh.R": (28, 0, -3),
                "shin.R": (-45, 0, 0),
                "foot.R": (18, 0, 0),
                "upper_arm.L": (32, 0, -8),
                "forearm.L": (-28, 0, 0),
                "upper_arm.R": (32, 0, 8),
                "forearm.R": (-28, 0, 0),
            },
            "loc": {"root": (0, 0.012, -0.055)},
            "scale": {"pelvis": (1.04, 1.04, 0.91), "spine": (1.02, 1.02, 0.96)},
        },
        10: {
            "rot": {
                "pelvis": (-8, 0, 0),
                "spine": (-12, 0, 0),
                "chest": (8, 0, 0),
                "head": (5, 0, 0),
                "thigh.L": (-12, 0, 4),
                "shin.L": (8, 0, 0),
                "foot.L": (-8, 0, 0),
                "thigh.R": (-4, 0, -4),
                "shin.R": (14, 0, 0),
                "foot.R": (-10, 0, 0),
                "upper_arm.L": (10, 0, -15),
                "forearm.L": (-35, 0, 0),
                "upper_arm.R": (55, 0, 72),
                "forearm.R": (-18, 0, 0),
            },
            "loc": {"root": (0, -0.006, 0.07)},
            "scale": {"pelvis": (0.98, 0.98, 1.04)},
        },
        20: {
            "rot": {
                "pelvis": (-3, 0, -4),
                "spine": (-7, 0, 3),
                "chest": (5, 0, 2),
                "head": (2, 0, 0),
                "thigh.L": (20, 0, 8),
                "shin.L": (-52, 0, 0),
                "foot.L": (22, 0, 0),
                "thigh.R": (33, 0, -8),
                "shin.R": (-62, 0, 0),
                "foot.R": (26, 0, 0),
                "upper_arm.L": (-10, 0, -10),
                "forearm.L": (-48, 0, 0),
                "upper_arm.R": (70, 0, 92),
                "forearm.R": (-12, 0, 0),
            },
            "loc": {"root": (0, -0.012, 0.31)},
        },
        28: {
            "rot": {
                "pelvis": (5, 0, 3),
                "spine": (3, 0, -2),
                "chest": (-2, 0, -1),
                "head": (-2, 0, 0),
                "thigh.L": (24, 0, 4),
                "shin.L": (-38, 0, 0),
                "foot.L": (15, 0, 0),
                "thigh.R": (18, 0, -4),
                "shin.R": (-32, 0, 0),
                "foot.R": (12, 0, 0),
                "upper_arm.L": (0, 0, -10),
                "forearm.L": (-28, 0, 0),
                "upper_arm.R": (35, 0, 46),
                "forearm.R": (-24, 0, 0),
            },
            "loc": {"root": (0, 0, 0.15)},
        },
        33: {
            "rot": {
                "pelvis": (14, 0, 0),
                "spine": (14, 0, 0),
                "chest": (-9, 0, 0),
                "head": (-5, 0, 0),
                "thigh.L": (27, 0, 2),
                "shin.L": (-42, 0, 0),
                "foot.L": (17, 0, 0),
                "thigh.R": (27, 0, -2),
                "shin.R": (-42, 0, 0),
                "foot.R": (17, 0, 0),
                "upper_arm.L": (24, 0, -6),
                "forearm.L": (-24, 0, 0),
                "upper_arm.R": (24, 0, 6),
                "forearm.R": (-24, 0, 0),
            },
            "loc": {"root": (0, 0.01, -0.045)},
            "scale": {"pelvis": (1.05, 1.05, 0.90), "spine": (1.02, 1.02, 0.96)},
        },
        37: {
            "rot": {
                "pelvis": (-4, 0, 0),
                "spine": (-5, 0, 0),
                "chest": (3, 0, 0),
                "head": (2, 0, 0),
                "thigh.L": (-4, 0, 0),
                "shin.L": (5, 0, 0),
                "thigh.R": (-4, 0, 0),
                "shin.R": (5, 0, 0),
                "upper_arm.L": (-8, 0, 0),
                "forearm.L": (-12, 0, 0),
                "upper_arm.R": (-8, 0, 0),
                "forearm.R": (-12, 0, 0),
            },
            "loc": {"root": (0, -0.003, 0.012)},
            "scale": {"pelvis": (0.99, 0.99, 1.02)},
        },
        40: {
            "rot": {"spine": (-2, 0, 0), "forearm.L": (-10, 0, 0), "forearm.R": (-10, 0, 0)},
            "loc": {"root": (0, 0, 0)},
        },
    }
    for frame, pose in poses.items():
        apply_pose(
            armature,
            frame,
            rotations=pose.get("rot"),
            locations=pose.get("loc"),
            scales=pose.get("scale"),
        )
    return action


def stash_actions(armature, actions):
    armature.animation_data_create()
    for track in list(armature.animation_data.nla_tracks):
        armature.animation_data.nla_tracks.remove(track)
    for action in actions:
        track = armature.animation_data.nla_tracks.new()
        track.name = action.name
        strip = track.strips.new(action.name, int(action.frame_range[0]), action)
        strip.action_frame_start = action.frame_range[0]
        strip.action_frame_end = action.frame_range[1]
        track.mute = True
    armature.animation_data.action = actions[0]


def add_preview_stage():
    bpy.ops.mesh.primitive_plane_add(size=8, location=(0, 0, -0.002))
    floor = bpy.context.object
    floor.name = "Preview_Floor"
    floor["exclude_from_export"] = True
    floor_material = bpy.data.materials.new("Preview_Floor_Material")
    floor_material.diffuse_color = (0.035, 0.045, 0.07, 1.0)
    floor.data.materials.append(floor_material)

    bpy.ops.object.light_add(type="AREA", location=(-1.2, -1.8, 2.3))
    key = bpy.context.object
    key.name = "Preview_Key"
    key.data.energy = 950
    key.data.shape = "DISK"
    key.data.size = 1.4
    key.rotation_euler = (math.radians(25), 0, math.radians(-30))
    key["exclude_from_export"] = True

    bpy.ops.object.light_add(type="AREA", location=(1.5, -0.5, 1.4))
    fill = bpy.context.object
    fill.name = "Preview_Fill"
    fill.data.energy = 650
    fill.data.size = 1.2
    fill["exclude_from_export"] = True

    bpy.ops.object.light_add(type="AREA", location=(0, 1.2, 1.8))
    rim = bpy.context.object
    rim.name = "Preview_Rim"
    rim.data.energy = 900
    rim.data.size = 1.0
    rim["exclude_from_export"] = True

    bpy.ops.object.camera_add(location=(0, -4.2, 0.72))
    camera = bpy.context.object
    camera.name = "Preview_Camera"
    camera.data.type = "ORTHO"
    camera.data.ortho_scale = 1.62
    direction = Vector((0, 0, 0.68)) - camera.location
    camera.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    camera["exclude_from_export"] = True
    bpy.context.scene.camera = camera


def configure_scene():
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 640
    scene.render.resolution_y = 640
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.film_transparent = False
    scene.render.fps = FPS
    scene.world.color = (0.018, 0.025, 0.05)
    scene.view_settings.look = "AgX - Medium High Contrast"


def render_contact_sheet(armature, action, frames, filename):
    scene = bpy.context.scene
    armature.animation_data.action = action
    preview_dir = os.path.join(OUTPUT_DIR, "preview_frames")
    os.makedirs(preview_dir, exist_ok=True)
    paths = []
    for frame in frames:
        scene.frame_set(frame)
        path = os.path.join(preview_dir, f"{action.name}_{frame:03d}.png")
        scene.render.filepath = path
        bpy.ops.render.render(write_still=True)
        paths.append(path)

    images = [bpy.data.images.load(path, check_existing=False) for path in paths]
    width = sum(image.size[0] for image in images)
    height = max(image.size[1] for image in images)
    sheet = bpy.data.images.new(filename, width=width, height=height, alpha=True)
    pixels = [0.0] * (width * height * 4)
    x_offset = 0
    for image in images:
        source = list(image.pixels)
        image_width, image_height = image.size
        for y in range(image_height):
            source_start = y * image_width * 4
            target_start = (y * width + x_offset) * 4
            pixels[target_start : target_start + image_width * 4] = source[
                source_start : source_start + image_width * 4
            ]
        x_offset += image_width
    sheet.pixels = pixels
    sheet.filepath_raw = os.path.join(OUTPUT_DIR, filename)
    sheet.file_format = "PNG"
    sheet.save()


def export_files(mesh, armature):
    bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH)
    bpy.ops.object.select_all(action="DESELECT")
    mesh.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature
    if os.path.exists(GLB_PATH):
        os.remove(GLB_PATH)
    bpy.ops.export_scene.gltf(
        filepath=GLB_PATH,
        export_format="GLB",
        use_selection=True,
        export_animations=True,
        export_animation_mode="ACTIONS",
        export_extra_animations=True,
        export_skins=True,
        export_morph=False,
    )


def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    clear_scene()
    mesh = import_character()
    build_material(mesh)
    armature = build_armature()
    group_count = bind_mesh(mesh, armature)
    walk = create_walk_action(armature)
    jump = create_jump_action(armature)
    stash_actions(armature, (walk, jump))
    add_preview_stage()
    configure_scene()
    render_contact_sheet(armature, walk, (1, 7, 13, 19), "walk_preview.png")
    render_contact_sheet(armature, jump, (1, 6, 10, 20, 28, 33, 40), "jump_preview.png")
    armature.animation_data.action = walk
    bpy.context.scene.frame_set(1)
    export_files(mesh, armature)
    print(f"Created {BLEND_PATH}")
    print(f"Created {GLB_PATH}")
    print(f"Vertex groups: {group_count}")
    print(f"Actions: {[action.name for action in (walk, jump)]}")


if __name__ == "__main__":
    main()
