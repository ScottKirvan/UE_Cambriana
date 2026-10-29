# Software Design Description: Surfacing and UVs

Requirements: [SRS.md](SRS.md) (FR-19, FR-21).

`[Proposed — unconfirmed]` unless marked.

## Tubes

Legacy name: closed form. Default surfacer for drawn segments.

- One ring per segment end, in the segment's frame. Consecutive segments in a chain share
  rings.
- Sides: setting, default 8. First vertex at 45 degrees between side and up.
- Each new ring's vertex order is rotated for least twist against the previous ring.
- Chains break after `]`, `f`, `z`, `g`. A branch starts from its parent's last ring.
- Caps on open ends: setting.
- Radius: half of segment length x thickness, minimum the min-thickness setting.
- `sk.ls`, 8 sides: 672 vertices, 1,280 triangles.

### Junctions

- A transition ring perpendicular to each branch.
- The parent's continuing ring is welded across the fork.
- Legacy junctions available as an option.

## Blocks

One four-sided box with caps per segment; bound to its bone.

## Polygons

`{ }` polygons: triangle fan from the first vertex; bound to the attachment bone; two-sided.

## Blob skins

**Decided**: blob skins and a general blob API are part of Cambriana. The blob API is in the
core and supports a variety of distance-field primitives.

`[Proposed — unconfirmed]`:

- Primitives: sphere, ellipsoid, line segment (capsule), box, torus. Each has radius,
  strength and falloff.
- Combination: summed density (classic blobs), smooth union, subtraction (negative blobs),
  intersection.
- One line-segment primitive per bone; radius from the bone; strength and falloff per module.
- Hand-placed primitives join the same field.
- Polygonised at a resolution setting. Candidates: Unreal's marching cubes and the author's
  marcher, compared on quality and speed. **Open** ([TBD.md](TBD.md)).
- Skin weights from field contributions, strongest N kept.
- Not based on the author's earlier blob modeller.

## UVs

| Mode | U | V |
|---|---|---|
| Constant density | Around the ring, repeats setting | Accumulated chain length / tile length |
| Proportional | Around the ring | Accumulated length / circumference |
| Per segment | Around the ring | 0 to 1 per segment, or per N segments |

- One duplicated vertex column per ring for the seam, aligned to the turtle's up vector.
- At a fork, V restarts or continues from the parent: setting.
- Polygons: fitted to 0 to 1 in their own plane.
- Blob skins: no UVs.
- Growth by bone scale stretches UVs.

## Data channels

| Channel | Value |
|---|---|
| Along-branch | 0 at chain base, 1 at chain tip |
| Root distance | Path length from the root |
| Generation | Creating generation of the vertex's bone |
| Branch random | Per-chain random in [0, 1) from the seed |
| Thickness | Segment thickness |
| Attributes | User values from `attr(name, value)` |

Mapping, `[Proposed — unconfirmed]`:

| Slot | Contents |
|---|---|
| UV0 | Surface UVs |
| UV1 | U: along-branch; V: generation / maximum generation |
| UV2 | U: root distance (cm); V: branch random |
| Vertex colour R | Thickness / maximum thickness |
| Vertex colour G, B, A | First three user attributes |

## Materials

- Material ID per triangle from `c`; one material slot per ID on baked meshes.
- Preview palette, IDs 0 to 9 repeating: black, brown, red, orange, yellow, green, blue,
  violet, grey, white.
