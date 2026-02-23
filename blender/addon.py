import bpy
import bmesh
import math
from collections import defaultdict
from mathutils import Vector
import numpy as np


# Find UV islands consisting on triangles. Uses a UV-space point cache.
# Not ideal, but seems to get the job done well enough.
def find_islands(bm):
    uv_layer = bm.loops.layers.uv.active
    tris = bm.calc_loop_triangles()

    point_to_tri = defaultdict(set)
    for i, tri in enumerate(tris):
        for loop in tri:
            uv = loop[uv_layer].uv
            point_to_tri[tuple(uv)].add(i)

    islands = []
    visited = set()

    for i in range(len(tris)):
        stack = [i]
        island = []

        while stack:
            i = stack.pop()

            if i in visited:
                continue
            visited.add(i)

            tri = tris[i]
            island.append(tri)

            for a, b in ((tri[0], tri[1]), (tri[1], tri[2]), (tri[2], tri[0])):
                a_tris = point_to_tri[tuple(a[uv_layer].uv)]
                b_tris = point_to_tri[tuple(b[uv_layer].uv)]

                # Set nonsense to find a tri that is shared between both points and isn't
                # i.
                neighbours = (a_tris & b_tris) - {i}

                for neighbour in neighbours:
                    stack.append(neighbour)
        if island:
            islands.append(island)
    return islands


# Hacky nonsense for finding some texel density stuff to scale things correctly. REWRITE!
def island_texel_density(island, uv_layer):
    world_area = 0.0
    uv_area = 0.0

    for tri in island:
        p0, p1, p2 = [loop.vert.co for loop in tri]
        world_area += ((p1 - p0).cross(p2 - p0)).length / 2.0

        u0, u1, u2 = [loop[uv_layer].uv for loop in tri]
        uv_area += abs((u1 - u0).cross(u2 - u0)) / 2.0

    if world_area == 0.0:
        return 0.0

    return math.sqrt(uv_area / world_area)


def render_colored_atlas(placements, img):
    import random

    for _idx, mask, y, x in placements:
        color = np.array(
            [random.uniform(0.1, 1.0) for _ in range(3)] + [1.0], dtype=np.float32
        )
        for my in range(mask.height):
            for mx in range(mask.width):
                if mask.get(mx, my):
                    img[y + my, x + mx] = color


import frostpack

for obj in bpy.context.selected_objects:
    bm = bmesh.from_edit_mesh(obj.data)
    uv_layer = bm.loops.layers.uv.active
    islands = find_islands(bm)

    print("found islands")
    sum = 0

    masks_and_islands = []

    scale = 1024

    for i, island in enumerate(islands):
        tris = [loop[uv_layer].uv * scale for tri in island for loop in tri]
        mask = frostpack.raster_island(tris)
        masks_and_islands.append((mask, i))
    masks_and_islands = sorted(masks_and_islands, key=lambda m: -m[0].sort_key())

    print("Got masks")

    atlas = frostpack.Atlas(1024)

    locations = []

    for mask, i in masks_and_islands:
        locations.append(atlas.place(mask))

    print("got locations")

    for i, (mask, index) in enumerate(masks_and_islands):
        location = locations[i]
        island = islands[index]
        for tri in island:
            for loop in tri:
                loop[uv_layer].uv = Vector(
                    [
                        (location[0] + loop[uv_layer].uv[0] * scale - mask.uv_min[0])
                        / atlas.array.width,
                        (location[1] + loop[uv_layer].uv[1] * scale - mask.uv_min[1])
                        / atlas.array.height,
                    ]
                )

    print("Set UVs")

    bmesh.update_edit_mesh(obj.data)

    placements = [
        (i, mask, locations[i][1], locations[i][0])
        for i, (mask, _) in enumerate(masks_and_islands)
    ]
    img = np.zeros((atlas.array.height, atlas.array.width, 4), dtype=np.float32)
    render_colored_atlas(placements, img)

    blender_img = bpy.data.images.new(
        "UV Atlas", width=atlas.array.width, height=atlas.array.height
    )
    blender_img.pixels = img.ravel()
    blender_img.pack()
