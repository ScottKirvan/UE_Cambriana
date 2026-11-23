---
outline: [2, 3]
---

# Data Channels

Every vertex Cambriana builds carries values that describe where it sits on the plant: how far along its branch, how far from the root, which generation made it, how thick it is, and any attributes the grammar sets. A material reads them to tint tips, fade bark with age, or vary each branch.

The channels are on the live actor's mesh and on both baked meshes, the skeletal mesh and the static mesh.

## Where each channel is

| Slot | Component | Channel | Range |
|---|---|---|---|
| UV1 | U | **Along-branch**: 0 at the base of the vertex's chain, 1 at its tip | 0 to 1 |
| UV1 | V | **Generation**: the generation that created the vertex's bone, divided by the highest generation in the mesh | 0 to 1 |
| UV2 | U | **Root distance**: the path length from the root, in centimetres | 0 and up |
| UV2 | V | **Branch random**: a random value per chain, from the seed | 0 up to, but not including, 1 |
| Vertex colour | R | **Thickness**: the segment's thickness divided by the thickest segment's in the mesh | 0 to 1 |
| Vertex colour | G | The grammar's first attribute | As set |
| Vertex colour | B | The grammar's second attribute | As set |
| Vertex colour | A | The grammar's third attribute | As set |

UV0 holds no texture coordinates: every vertex has (0, 0) there. On the static mesh, UV3 holds the generated lightmap UVs.

Each chain gets its own branch random value, and the same seed gives the same values.

## Attributes

A grammar sets attributes with `attr(name, value)`; see the [Syntax Guide](../grammar-guide). The vertex colour's G, B and A hold the first three attribute names in the order the grammar first uses them, unscaled. A vertex where an attribute is not set has 0 there, as does every vertex when the grammar uses fewer than three. A fourth attribute and later ones are in no slot.

For example, in this grammar `wet` goes to G, `age` to B and `heat` to A, and `extra` is not stored:

```text
axiom attr(wet, 0.25) attr(age, 0.5) attr(heat, 0.75) attr(extra, 0.9) A
```

## Precision on the baked static mesh

The static mesh stores UVs at full precision, so root distance keeps its fine detail on large plants.

It stores the vertex colour at 8 bits per channel, the engine's static mesh format. A material reads each value clamped to 0 to 1, to the nearest 1/255. Keep attribute values between 0 and 1 to avoid clamping. The values are stored so that the material reads the values themselves, not an sRGB-encoded form of them.

## Reading the channels in a material

Use the material editor's standard nodes on a material assigned to a baked mesh's slots:

| To read | Node | Output |
|---|---|---|
| Along-branch | **TextureCoordinate**, Coordinate Index 1 | R |
| Generation | **TextureCoordinate**, Coordinate Index 1 | G |
| Root distance (cm) | **TextureCoordinate**, Coordinate Index 2 | R |
| Branch random | **TextureCoordinate**, Coordinate Index 2 | G |
| Thickness | **Vertex Color** | R |
| Attributes 1 to 3 | **Vertex Color** | G, B, A |

For example, to darken branch tips, feed along-branch into the Alpha of a **Lerp** between a base colour and a tip colour. To give each branch a slightly different tint, use branch random as the Alpha instead.

The live actor and the grammar editor's preview show their own preview colours per material ID, so a material reading the channels shows its effect on the baked meshes.
