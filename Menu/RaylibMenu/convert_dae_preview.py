import argparse
import math
import shutil
import urllib.parse
import xml.etree.ElementTree as ET
from pathlib import Path


def local_name(tag):
    return tag.rsplit("}", 1)[-1]


def child(element, name):
    if element is None:
        return None
    for item in element:
        if local_name(item.tag) == name:
            return item
    return None


def children(element, name):
    if element is None:
        return []
    return [item for item in element if local_name(item.tag) == name]


def source_id(value):
    return value[1:] if value and value.startswith("#") else value


def safe_name(value):
    result = "".join(character if character.isalnum() or character in "_-" else "_" for character in value)
    return result or "material_default"


def read_source(source):
    float_array = child(source, "float_array")
    if float_array is None or not float_array.text:
        return [], 3

    values = [float(value) for value in float_array.text.split()]
    stride = 3
    technique = child(source, "technique_common")
    accessor = child(technique, "accessor")
    if accessor is not None:
        stride = int(accessor.attrib.get("stride", "3"))
    return values, stride


def convert_axis(values, up_axis):
    x, y, z = values[:3]
    if up_axis == "Z_UP":
        return x, z, -y
    if up_axis == "X_UP":
        return y, x, -z
    return x, y, z


def normalize_vector(values, up_axis):
    converted = convert_axis(values, up_axis)
    length = math.sqrt(sum(component * component for component in converted))
    if length <= 0.000001:
        return 0.0, 1.0, 0.0
    return tuple(component / length for component in converted)


def parse_images(root):
    images = {}
    for image in (item for item in root.iter() if local_name(item.tag) == "image"):
        init_from = child(image, "init_from")
        if init_from is not None and init_from.text:
            images[image.attrib.get("id", "")] = urllib.parse.unquote(init_from.text.strip())
    return images


def parse_effects(root, images):
    effects = {}
    for effect in (item for item in root.iter() if local_name(item.tag) == "effect"):
        profile = child(effect, "profile_COMMON")
        if profile is None:
            continue

        new_parameters = {
            parameter.attrib.get("sid", ""): parameter
            for parameter in children(profile, "newparam")
        }
        technique = child(profile, "technique")
        shader = next(iter(technique), None) if technique is not None else None
        diffuse = child(shader, "diffuse")
        color = (0.82, 0.82, 0.82)
        texture_path = None

        diffuse_color = child(diffuse, "color")
        if diffuse_color is not None and diffuse_color.text:
            components = [float(value) for value in diffuse_color.text.split()]
            if len(components) >= 3:
                color = tuple(components[:3])

        texture = child(diffuse, "texture")
        if texture is not None:
            sampler = new_parameters.get(texture.attrib.get("texture", ""))
            sampler_source = child(child(sampler, "sampler2D"), "source")
            surface = new_parameters.get(sampler_source.text.strip(), None) if sampler_source is not None and sampler_source.text else None
            image_source = child(child(surface, "surface"), "init_from")
            if image_source is not None and image_source.text:
                texture_path = images.get(image_source.text.strip(), image_source.text.strip())

        effects[effect.attrib.get("id", "")] = {
            "color": color,
            "texture": texture_path,
        }
    return effects


def parse_materials(root, effects):
    materials = {}
    for material in (item for item in root.iter() if local_name(item.tag) == "material"):
        instance_effect = child(material, "instance_effect")
        effect_id = source_id(instance_effect.attrib.get("url", "")) if instance_effect is not None else ""
        materials[material.attrib.get("id", "")] = effects.get(
            effect_id,
            {"color": (0.82, 0.82, 0.82), "texture": None},
        )
    return materials


def parse_material_bindings(root):
    bindings = {}
    for instance_material in (item for item in root.iter() if local_name(item.tag) == "instance_material"):
        symbol = instance_material.attrib.get("symbol", "")
        target = source_id(instance_material.attrib.get("target", ""))
        if symbol and target:
            bindings[symbol] = target
    return bindings


def source_values(mesh, source_name):
    for source in children(mesh, "source"):
        if source.attrib.get("id", "") == source_name:
            return read_source(source)
    return [], 3


def append_source(cache, key, values, stride, destination, transform):
    if key in cache:
        return cache[key]
    base = len(destination)
    for index in range(0, len(values), stride):
        value = values[index:index + stride]
        if len(value) >= min(stride, 2):
            destination.append(transform(value))
    cache[key] = base
    return base


def load_model_data(path):
    root = ET.parse(path).getroot()
    up_axis_element = next((item for item in root.iter() if local_name(item.tag) == "up_axis"), None)
    up_axis = up_axis_element.text.strip() if up_axis_element is not None and up_axis_element.text else "Y_UP"

    images = parse_images(root)
    effects = parse_effects(root, images)
    materials = parse_materials(root, effects)
    bindings = parse_material_bindings(root)

    positions = []
    normals = []
    texcoords = []
    batches = []
    position_cache = {}
    normal_cache = {}
    texcoord_cache = {}

    for geometry in (item for item in root.iter() if local_name(item.tag) == "geometry"):
        geometry_id = geometry.attrib.get("id", "geometry")
        mesh = child(geometry, "mesh")
        if mesh is None:
            continue

        vertices_sources = {}
        for vertex_set in children(mesh, "vertices"):
            for input_element in children(vertex_set, "input"):
                if input_element.attrib.get("semantic") == "POSITION":
                    vertices_sources[vertex_set.attrib.get("id", "")] = source_id(input_element.attrib.get("source", ""))

        for primitive_index, primitive in enumerate(mesh):
            primitive_type = local_name(primitive.tag)
            if primitive_type not in ("triangles", "polylist"):
                continue

            inputs = children(primitive, "input")
            if not inputs:
                continue

            vertex_input = next((item for item in inputs if item.attrib.get("semantic") == "VERTEX"), None)
            normal_input = next((item for item in inputs if item.attrib.get("semantic") == "NORMAL"), None)
            texcoord_input = next(
                (item for item in inputs if item.attrib.get("semantic") == "TEXCOORD" and item.attrib.get("set", "0") == "0"),
                next((item for item in inputs if item.attrib.get("semantic") == "TEXCOORD"), None),
            )
            if vertex_input is None:
                continue

            index_stride = max(int(item.attrib.get("offset", "0")) for item in inputs) + 1
            vertex_offset = int(vertex_input.attrib.get("offset", "0"))
            normal_offset = int(normal_input.attrib.get("offset", "0")) if normal_input is not None else None
            texcoord_offset = int(texcoord_input.attrib.get("offset", "0")) if texcoord_input is not None else None

            position_source_id = vertices_sources.get(source_id(vertex_input.attrib.get("source", "")), "")
            position_values, position_stride = source_values(mesh, position_source_id)
            position_base = append_source(
                position_cache,
                (geometry_id, position_source_id),
                position_values,
                position_stride,
                positions,
                lambda value: convert_axis(value, up_axis),
            )

            normal_base = None
            if normal_input is not None:
                normal_source_id = source_id(normal_input.attrib.get("source", ""))
                normal_values, normal_stride = source_values(mesh, normal_source_id)
                normal_base = append_source(
                    normal_cache,
                    (geometry_id, normal_source_id),
                    normal_values,
                    normal_stride,
                    normals,
                    lambda value: normalize_vector(value, up_axis),
                )

            texcoord_base = None
            if texcoord_input is not None:
                texcoord_source_id = source_id(texcoord_input.attrib.get("source", ""))
                texcoord_values, texcoord_stride = source_values(mesh, texcoord_source_id)
                texcoord_base = append_source(
                    texcoord_cache,
                    (geometry_id, texcoord_source_id),
                    texcoord_values,
                    texcoord_stride,
                    texcoords,
                    lambda value: (value[0], 1.0 - value[1]),
                )

            raw_indices = []
            for polygon_data in children(primitive, "p"):
                if polygon_data.text:
                    raw_indices.extend(int(value) for value in polygon_data.text.split())
            if not raw_indices:
                continue

            def corner(index):
                start = index * index_stride
                return (
                    position_base + raw_indices[start + vertex_offset],
                    texcoord_base + raw_indices[start + texcoord_offset] if texcoord_base is not None else None,
                    normal_base + raw_indices[start + normal_offset] if normal_base is not None else None,
                )

            faces = []
            if primitive_type == "triangles":
                corner_count = len(raw_indices) // index_stride
                for index in range(0, corner_count - 2, 3):
                    faces.append((corner(index), corner(index + 1), corner(index + 2)))
            else:
                vcount = child(primitive, "vcount")
                polygon_sizes = [int(value) for value in vcount.text.split()] if vcount is not None and vcount.text else []
                cursor = 0
                for polygon_size in polygon_sizes:
                    polygon = [corner(cursor + index) for index in range(polygon_size)]
                    cursor += polygon_size
                    for index in range(1, len(polygon) - 1):
                        faces.append((polygon[0], polygon[index], polygon[index + 1]))

            symbol = primitive.attrib.get("material", "material_default")
            batches.append({
                "name": f"{safe_name(geometry_id)}_{primitive_index}",
                "material": bindings.get(symbol, symbol),
                "faces": faces,
            })

    return positions, normals, texcoords, batches, materials


def resolve_texture(source_path, texture_reference):
    if not texture_reference:
        return None

    normalized = texture_reference.replace("\\", "/")
    if normalized.startswith("file://"):
        normalized = normalized[7:]
    reference = Path(normalized)
    candidates = [
        reference,
        source_path.parent / reference,
        source_path.parent / "textures" / reference.name,
        source_path.parent / reference.name,
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()
    return None


def face_token(corner, position_remap, texcoord_remap, normal_remap):
    position, texcoord, normal = corner
    position = position_remap[position]
    texcoord = texcoord_remap[texcoord] if texcoord is not None else None
    normal = normal_remap[normal] if normal is not None else None
    if texcoord is not None and normal is not None:
        return f"{position}/{texcoord}/{normal}"
    if texcoord is not None:
        return f"{position}/{texcoord}"
    if normal is not None:
        return f"{position}//{normal}"
    return str(position)


def write_preview(source_paths, output_path):
    if isinstance(source_paths, Path):
        source_paths = [source_paths]

    loaded_sources = []
    all_positions = []
    for source_index, source_path in enumerate(source_paths):
        positions, normals, texcoords, batches, materials = load_model_data(source_path)
        if not positions or not batches:
            continue
        loaded_sources.append({
            "index": source_index,
            "path": source_path,
            "positions": positions,
            "normals": normals,
            "texcoords": texcoords,
            "batches": batches,
            "materials": materials,
        })
        all_positions.extend(positions)

    if not all_positions or not loaded_sources:
        raise RuntimeError("No se encontro geometria util en los archivos DAE.")

    minimum = [min(vertex[axis] for vertex in all_positions) for axis in range(3)]
    maximum = [max(vertex[axis] for vertex in all_positions) for axis in range(3)]
    center = [(minimum[axis] + maximum[axis]) * 0.5 for axis in range(3)]
    extent = max(maximum[axis] - minimum[axis] for axis in range(3))
    scale = 5.4 / max(extent, 0.0001)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    texture_folder = output_path.parent / f"{output_path.stem}_textures"
    part_folder = output_path.parent / f"{output_path.stem}_parts"
    if texture_folder.exists():
        shutil.rmtree(texture_folder)
    if part_folder.exists():
        shutil.rmtree(part_folder)
    texture_folder.mkdir(parents=True, exist_ok=True)
    part_folder.mkdir(parents=True, exist_ok=True)

    copied_textures = {}
    manifest_lines = ["# obj|texture|red|green|blue"]
    material_count = 0
    vertex_count = 0
    triangle_count = 0

    for source_data in loaded_sources:
        source_index = source_data["index"]
        source_path = source_data["path"]
        positions = [
            tuple((vertex[axis] - center[axis]) * scale for axis in range(3))
            for vertex in source_data["positions"]
        ]
        normals = source_data["normals"]
        texcoords = source_data["texcoords"]
        batches = source_data["batches"]
        materials = source_data["materials"]
        vertex_count += len(positions)
        triangle_count += sum(len(batch["faces"]) for batch in batches)

        used_materials = []
        for batch in batches:
            material_id = batch["material"]
            if material_id not in used_materials:
                used_materials.append(material_id)

        for material_index, material_id in enumerate(used_materials):
            material = materials.get(material_id, {"color": (0.82, 0.82, 0.82), "texture": None})
            material_faces = [
                face
                for batch in batches
                if batch["material"] == material_id
                for face in batch["faces"]
            ]
            if not material_faces:
                continue

            used_positions = sorted({corner[0] for face in material_faces for corner in face})
            used_texcoords = sorted({corner[1] for face in material_faces for corner in face if corner[1] is not None})
            used_normals = sorted({corner[2] for face in material_faces for corner in face if corner[2] is not None})
            position_remap = {index: local_index + 1 for local_index, index in enumerate(used_positions)}
            texcoord_remap = {index: local_index + 1 for local_index, index in enumerate(used_texcoords)}
            normal_remap = {index: local_index + 1 for local_index, index in enumerate(used_normals)}

            source_name = safe_name(source_path.stem)
            part_name = f"{source_index:02d}_{material_index:02d}_{source_name}_{safe_name(material_id)}.obj"
            part_path = part_folder / part_name
            with part_path.open("w", encoding="ascii", newline="\n") as output:
                output.write("# Pieza completa de un material COLLADA para Raylib\n")
                output.write(f"o {source_name}_{safe_name(material_id)}\n")
                for index in used_positions:
                    vertex = positions[index]
                    output.write(f"v {vertex[0]:.7f} {vertex[1]:.7f} {vertex[2]:.7f}\n")
                for index in used_texcoords:
                    texcoord = texcoords[index]
                    output.write(f"vt {texcoord[0]:.7f} {texcoord[1]:.7f}\n")
                for index in used_normals:
                    normal = normals[index]
                    output.write(f"vn {normal[0]:.7f} {normal[1]:.7f} {normal[2]:.7f}\n")
                for face in material_faces:
                    output.write(
                        "f "
                        + " ".join(
                            face_token(corner, position_remap, texcoord_remap, normal_remap)
                            for corner in face
                        )
                        + "\n"
                    )

            texture_reference = "-"
            texture_source = resolve_texture(source_path, material["texture"])
            if texture_source is not None:
                texture_key = str(texture_source).lower()
                if texture_key not in copied_textures:
                    destination = texture_folder / f"{len(copied_textures):02d}_{texture_source.name}"
                    shutil.copy2(texture_source, destination)
                    copied_textures[texture_key] = destination
                texture_reference = copied_textures[texture_key].relative_to(output_path.parent).as_posix()

            color = material["color"]
            manifest_lines.append(
                "|".join([
                    part_path.relative_to(output_path.parent).as_posix(),
                    texture_reference,
                    f"{color[0]:.6f}",
                    f"{color[1]:.6f}",
                    f"{color[2]:.6f}",
                ])
            )
            material_count += 1

    with output_path.open("w", encoding="ascii", newline="\n") as output:
        output.write("\n".join(manifest_lines))
        output.write("\n")

    return vertex_count, triangle_count, material_count, len(copied_textures)


def main():
    parser = argparse.ArgumentParser(description="Genera OBJ + MTL + texturas para una vista previa Raylib.")
    parser.add_argument("input", type=Path, nargs="+")
    parser.add_argument("output", type=Path)
    arguments = parser.parse_args()

    vertex_count, triangle_count, material_count, texture_count = write_preview(
        arguments.input,
        arguments.output,
    )
    source_label = ", ".join(str(path) for path in arguments.input)
    print(
        f"{arguments.output}: {source_label}: {vertex_count} vertices, {triangle_count} triangulos, "
        f"{material_count} materiales, {texture_count} texturas"
    )


if __name__ == "__main__":
    main()
