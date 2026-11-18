# Open Issues

When one is decided, update the affected document and remove it here.

| # | Question | Affects |
|---|---|---|
| 3 | Which polygoniser do blob skins use: Unreal's marching cubes or the author's marcher? Unreal's would move polygonising out of the core. | [SDD-surfacing.md](SDD-surfacing.md) |
| 5 | Are the user stories in SRS.md complete and correct? | [SRS.md](SRS.md) |
| 6 | Should rig hints be able to select Modular Control Rig modules (for example a leg module on a leg chain) in addition to individual controls? | [FMT-grammar.md](FMT-grammar.md), [SDD-rigging.md](SDD-rigging.md) |
| 7 | Can the plugin handle the standard Reimport for baked skeletal and static meshes alongside the engine's FBX and Interchange importers? To be checked against engine source at M3 step 9; if it cannot, the author decides the alternative. | [SDD-unreal.md](SDD-unreal.md) |
| 8 | Expressions may read the `angle` setting, but SDD-rigging treats the basic angle as pose-tier. What rule applies when `angle` is read outside a rotation command: the pose restrictions, loss of angle posing for that grammar, or no reading of `angle` in expressions? | [FMT-grammar.md](FMT-grammar.md), [SDD-rigging.md](SDD-rigging.md) |
