import bpy
from collections import deque
from mathutils import Vector


OBJ_PATH = r"C:\Users\sasuk\Downloads\1\3ad75d7d8317bffe70dbd81e8112f575.obj"


bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.wm.obj_import(filepath=OBJ_PATH)
mesh_object = next(obj for obj in bpy.context.selected_objects if obj.type == "MESH")
mesh = mesh_object.data

neighbors = [set() for _ in mesh.vertices]
for edge in mesh.edges:
    a, b = edge.vertices
    neighbors[a].add(b)
    neighbors[b].add(a)

unvisited = set(range(len(mesh.vertices)))
components = []
while unvisited:
    start = unvisited.pop()
    queue = deque([start])
    component = [start]
    while queue:
        current = queue.popleft()
        for neighbor in neighbors[current]:
            if neighbor in unvisited:
                unvisited.remove(neighbor)
                queue.append(neighbor)
                component.append(neighbor)
    components.append(component)

records = []
for index, component in enumerate(components):
    coords = [mesh.vertices[vertex].co for vertex in component]
    minimum = Vector(
        (
            min(co.x for co in coords),
            min(co.y for co in coords),
            min(co.z for co in coords),
        )
    )
    maximum = Vector(
        (
            max(co.x for co in coords),
            max(co.y for co in coords),
            max(co.z for co in coords),
        )
    )
    center = sum(coords, Vector()) / len(coords)
    records.append((len(component), index, minimum, maximum, center))

records.sort(reverse=True, key=lambda record: record[0])
print(f"COMPONENTS {len(records)}")
for size, index, minimum, maximum, center in records[:100]:
    print(
        f"{index:03d} size={size:5d} "
        f"min=({minimum.x:+.3f},{minimum.y:+.3f},{minimum.z:+.3f}) "
        f"max=({maximum.x:+.3f},{maximum.y:+.3f},{maximum.z:+.3f}) "
        f"center=({center.x:+.3f},{center.y:+.3f},{center.z:+.3f})"
    )
