import os
import struct

import bpy


OUTPUT_DIR = r"C:\Users\sasuk\source\repos\PG\character_output"
WIDTH = 320
HEIGHT = 320
FRAME_DELAY = 4
TEMP_COUNTER = 0


def linear_to_srgb(value):
    value = max(0.0, min(1.0, value))
    if value <= 0.0031308:
        return value * 12.92
    return 1.055 * (value ** (1.0 / 2.4)) - 0.055


def build_palette():
    palette = bytearray()
    for red in range(6):
        for green in range(6):
            for blue in range(6):
                palette.extend((red * 51, green * 51, blue * 51))
    palette.extend(b"\x00" * ((256 - 216) * 3))
    return bytes(palette)


def render_indices(scene):
    global TEMP_COUNTER
    TEMP_COUNTER += 1
    temp_frame = os.path.join(OUTPUT_DIR, f"_preview_frame_{TEMP_COUNTER:03d}.png")
    scene.render.filepath = temp_frame
    bpy.ops.render.render(write_still=True)
    image = bpy.data.images.load(temp_frame, check_existing=False)
    pixels = list(image.pixels)
    rows = []
    for y in range(HEIGHT - 1, -1, -1):
        row = bytearray()
        base = y * WIDTH * 4
        for x in range(WIDTH):
            offset = base + x * 4
            red = min(5, int(max(0.0, pixels[offset]) * 5.999))
            green = min(5, int(max(0.0, pixels[offset + 1]) * 5.999))
            blue = min(5, int(max(0.0, pixels[offset + 2]) * 5.999))
            row.append(red * 36 + green * 6 + blue)
        rows.append(row)
    bpy.data.images.remove(image)
    return b"".join(rows)


def lzw_encode(indices):
    clear_code = 256
    end_code = 257
    code_size = 9
    bit_buffer = 0
    bit_count = 0
    output = bytearray()

    def emit(code):
        nonlocal bit_buffer, bit_count
        bit_buffer |= code << bit_count
        bit_count += code_size
        while bit_count >= 8:
            output.append(bit_buffer & 0xFF)
            bit_buffer >>= 8
            bit_count -= 8

    for value in indices:
        emit(clear_code)
        emit(value)
    emit(end_code)
    if bit_count:
        output.append(bit_buffer & 0xFF)
    return bytes(output)


def write_subblocks(handle, data):
    for start in range(0, len(data), 255):
        block = data[start : start + 255]
        handle.write(bytes((len(block),)))
        handle.write(block)
    handle.write(b"\x00")


def write_gif_header(handle):
    handle.write(b"GIF89a")
    handle.write(struct.pack("<HH", WIDTH, HEIGHT))
    handle.write(bytes((0xF7, 0, 0)))
    handle.write(build_palette())
    handle.write(b"\x21\xFF\x0BNETSCAPE2.0\x03\x01\x00\x00\x00")


def write_gif_frame(handle, indices):
    handle.write(b"\x21\xF9\x04\x00")
    handle.write(struct.pack("<H", FRAME_DELAY))
    handle.write(b"\x00\x00")
    handle.write(b"\x2C")
    handle.write(struct.pack("<HHHH", 0, 0, WIDTH, HEIGHT))
    handle.write(b"\x00")
    handle.write(b"\x08")
    write_subblocks(handle, lzw_encode(indices))


def render_action_gif(armature, action_name, start, end, filename):
    scene = bpy.context.scene
    armature.animation_data.action = bpy.data.actions[action_name]
    scene.render.resolution_x = WIDTH
    scene.render.resolution_y = HEIGHT
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    path = os.path.join(OUTPUT_DIR, filename)
    with open(path, "wb") as handle:
        write_gif_header(handle)
        for frame in range(start, end + 1):
            scene.frame_set(frame)
            write_gif_frame(handle, render_indices(scene))
        handle.write(b"\x3B")


armature = bpy.data.objects["PINKX_Rig"]
render_action_gif(armature, "Walk_Stylized", 1, 25, "walk_preview.gif")
render_action_gif(armature, "Jump_Stylized", 1, 40, "jump_preview.gif")
for filename in os.listdir(OUTPUT_DIR):
    if filename.startswith("_preview_frame_") and filename.endswith(".png"):
        os.remove(os.path.join(OUTPUT_DIR, filename))
