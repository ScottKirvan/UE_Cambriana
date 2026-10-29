# Format Specification: Cambriana Grammar

Requirements: [SRS.md](SRS.md) (FR-24).

File extension `.lsys`.

The Cambriana grammar format is Cambriana's own format, not a dialect of cpfg or
any other existing format, with named settings and parameters, rule parameters and whitespace. It is the only format grammar assets hold; legacy `.ls` files are
converted to it on import.

Open details: [TBD.md](TBD.md).

## Requirements

1. Turtle commands are identical to the legacy dialect (core repo, `notes/dev/FMT-legacy-ls.md`).
2. User symbols are names.
3. Parameters carry a tier: pose, shape or structure ([SDD-rigging.md](SDD-rigging.md)).
4. The grammar declares bones, sockets and data channels.
5. Every legacy `.ls` file converts to this format with identical behaviour.

## Turtle command names

Every turtle command has a long name, usable anywhere its glyph is. Glyph and
name parse to the same command; an argument is written the same way: `left(30)` = `+(30)`.

Names:

| Glyph | Name | Glyph | Name |
|---|---|---|---|
| `F` | `draw` | `\|` | `turnaround` |
| `Z` | `drawhalf` | `%` | `rollaround` |
| `f` | `move` | `~` | `jitter` |
| `z` | `movehalf` | `t` | `gravity` |
| `g` | `skip` | `$` | `level` |
| `.` | `vertex` | `!` | `thinner` |
| `+` | `left` | `?` | `thicker` |
| `-` | `right` | `:` | `narrower` |
| `&` | `down` | `;` | `wider` |
| `^` | `up` | `'` | `shorter` |
| `<` | `rollleft` | `"` | `longer` |
| `>` | `rollright` | `c` | `colour` |
| `[` `]` | `push` `pop` | `{` `}` | `polygon` `endpolygon` |

- Command names are reserved; modules cannot use them.
- The serialiser writes glyphs or names according to a style setting. Conversion from the
  legacy dialect writes glyphs.

## Example

```
lsystem Spirals

generations 20
angle       25
thickness   50%
size        200cm
seed        1

pose  param shrink = 0.9
shape param taper  = 1.0

axiom  c [ Left ] c [ Right ] c [ Up ] c [ Down ]

grow {
    Left  -> + F '(shrink) Left
    Right -> - F '(shrink) Right
    Up    -> ^ F '(shrink) Up
    Down  -> & F '(shrink) Down
}
```

## Constructs

| Construct | Syntax |
|---|---|
| Settings | `generations`, `angle`, `thickness`, `size` (Fit target) or `step` (step length), `seed` |
| Comments | `#` to end of line, anywhere |
| Modules | Names: `Spine`, `BranchA` |
| Parameters | `pose param`, `shape param`, `structure param`, `param` |
| Parametric modules | `Seg(len) -> Seg(len * stretch)` |
| Conditions | `Leaf(age) : age >= 1 and age <= 2 -> ...` |
| Grow and draw blocks | `grow { }` rewrites modules; `draw { }` maps modules to turtle commands |
| Empty replacement | `X ->` removes `X` in the next generation |
| Growth marking | Rules marked for fractional growth by default, except rules for `F`, `f`, `Z`, `z`; `grows` and `nogrowth` flags override per rule |
| Weighted alternatives | `F -> (2) F[+F]F \| (1) F[-F]F` |
| Macros | `macro Tier = ...`; expanded at parse time |
| Repeat | `repeat 6 { ... }`; expanded at parse time |
| Named axioms | `axiom sapling ...`, `axiom mature ...` |
| Bone control | `bone`, `merge`, bone budget |
| Sockets | `socket Flower` |
| Attributes | `attr(name, value)` |
| Symmetry | `mirror { ... }` |
| Credit | `credit "..."`; stored on the asset |
| Environment query | Reserved: `query` modules whose parameters are set by an environment query between generations (AR-02) |

The parser rejects a `pose` parameter used outside rotation commands.

## Conversion from the legacy dialect

Legacy `.ls` files are converted to this format on import.

- The converted grammar builds the same production and bone tree as the legacy file
  (golden values in the core repo's `notes/dev/FMT-legacy-ls.md`).
- Header numbers become `generations`, `angle` and `thickness`.
- Legacy symbols become single-character module names; each legacy rule becomes a `grow`
  rule.
- Implicit legacy behaviour is written explicitly: a symbol with no rule gets an empty
  replacement; growth marking that differs from this format's default gets a `grows` or
  `nogrowth` flag.
- Comments are kept.
- The source file has no seed; the grammar asset gets a generated one.

## Conversion from cpfg

cpfg models are converted to this format on import.

- Input: the `.l` file and its `.v` view file. The `.v` file supplies defaults the `.l` file
  relies on (angle increment, line width, colour increments).
- The importer applies a C preprocessor subset (`#define`, `#include`, `#if`, `#ifdef`,
  `#else`, `#endif`) before parsing, as cpfg does.
- cpfg turtle symbols are mapped to this format's commands; productions map to `grow`
  rules, homomorphism productions to `draw` rules, contexts, conditions and probabilities
  directly.
- Decomposition, sub-L-systems, global statement blocks, arrays and environment-query
  modules: mapping **Open**.
- L+C models are not convertible.
- Constructs that cannot be converted are reported with line and column; nothing is dropped
  silently.

## Hand-written equivalent: `TREE11.LS`

Built at the same settings, this produces the same bone tree as `TREE11.LS`.

```
lsystem Tree11
credit  "after TREE11.LS by Lj Lapre, Lparser"

generations 12
angle       5
thickness   30%

shape param taper     = 0.8
shape param stretch   = 1.25
shape param leaf_size = 200
param bark = 12
param leaf = 8

axiom  c(bark) Trunk

macro Tier = ~(5) Seg(100) Whorl Whorl [ Droop(0) BranchA ] Leaves >(30)

grow {
    Trunk   -> [ Seg(100) ~(5) Seg(100) Spine Leaves ]
    Spine   -> Tier Tier !(taper) Spine
    Whorl   -> [ Droop(0) BranchA ] Leaves >(120)

    BranchA -> ~(10) $ Seg(100) [ Left(0)  BudA Leaves ] ~(10) $ Seg(100) Leaves !(taper) BranchB
    BranchB -> ~(10) $ Seg(100) [ Right(0) BudB Leaves ] ~(10) $ Seg(100) Leaves !(taper) BranchA
    BudA    -> BranchA
    BudB    -> BranchB

    Leaves  -> [ g(300) repeat 6 { g(40) Leaf(0) } ]

    Seg(len)  -> Seg(len * stretch)
    Droop(n)  -> Droop(n + 1)
    Left(n)   -> Left(n + 1)
    Right(n)  -> Right(n + 1)
    Leaf(age) -> Leaf(age + 1)
}

draw {
    Seg(len)  -> F(len)
    Droop(n)  -> &(n * angle)
    Left(n)   -> +(n * angle)
    Right(n)  -> -(n * angle)
    Leaf(age) : age >= 1 and age <= 2
              -> [ ~ f(7) c(leaf) { +(30) f(leaf_size) -(120) f(leaf_size) -(120) f(leaf_size) } ]
}
```
