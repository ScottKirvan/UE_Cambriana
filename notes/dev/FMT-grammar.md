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
4. The grammar declares bones, sockets, data channels and rig hints.
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

shape param shrink = 0.9
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
| Rig hints | `fk(name)`, `ik(name)`, `ik(name, n)`, `pole(name)`, `aim(name)`; see Rig hints |
| Attributes | `attr(name, value)` |
| Symmetry | `mirror { ... }` |
| Credit | `credit "..."`; stored on the asset |
| Environment query | Reserved: `query` modules whose parameters are set by an environment query between generations (AR-02) |

The parser rejects a `pose` parameter used outside rotation commands.

## Rig hints

A rig hint attaches to the current bone and draws nothing. Hints are written in `draw` rules,
so every expansion of a rule carries them.

| Command | Meaning |
|---|---|
| `fk(name)` | FK control on the current bone |
| `ik(name)` | IK goal at the current bone; the chain runs from the first bone of the enclosing branch (or the root) to the current bone |
| `ik(name, n)` | IK goal at the current bone; the chain is the last `n` bones |
| `pole(name)` | Pole target for IK chain `name`, at the current turtle position |
| `aim(name)` | Aim control: the current bone points at a target placed at the current turtle position |

- Hint names are qualified by the structural name of the bone they attach to
  ([SDD-rigging.md](SDD-rigging.md)): a `Leg` rule expanded four times gives four `foot` goals,
  each with a unique qualified name. `[Proposed — unconfirmed]`
- `pole` and `aim` targets are usually placed with moves inside a branch:
  `[ f(30) pole(knee) ]`.
- Hint command names are reserved, as turtle command names are.
- Conversion from the legacy dialect and from cpfg produces no hints.

```
draw {
    Leg -> F & F ^ F ik(foot) [ ^ f(30) pole(knee) ]
}
```

## Expressions

A module with no `draw` rule draws nothing, with no warning.

| Kind | Allowed |
|---|---|
| Operators | `+ - * /`, unary minus, power, modulo, `< <= > >= == !=`, `and or not`, parentheses |
| Conditional | `if(cond, a, b)` |
| Maths | `abs floor ceil round min max clamp lerp sqrt pow exp log` |
| Trigonometry | `sin cos tan asin acos atan atan2`, in degrees |
| Random | `rand(a, b)`, from the seed |

Pose parameters may use only `+ - * /`, unary minus, power and
`abs min max clamp lerp sqrt sin cos tan`. Anything else in a pose parameter is an error with
line and column.

### Expression semantics `[Proposed — unconfirmed]`

- Power is `^`, right-associative and tighter than unary minus: `-2 ^ 2` is -4. Modulo is `%`,
  floored: `-1 % 3` is 2. `pow(a, b)` is power.
- Precedence, loosest first: `or`; `and`; `not`; comparisons; `+ -`; `* / %`; unary minus; `^`.
- `and`, `or`, `not`, `if` and comparisons treat non-zero as true and give 1 or 0. All arguments are
  always evaluated, including in `if`, `and` and `or`.
- A result that is not finite becomes 0, with one warning per expression at its line and column.
- `rand(a, b)` is `a + (b - a) * u`, `u` in [0, 1) from the Visual C++ sequence. Grow rules, draw rules
  and the turtle's `~` each have their own sequence from the seed, so one does not disturb another.
- Expressions may read the settings `generations`, `angle`, `thickness` and `seed`, after build
  overrides. A parameter cannot reuse those names, or the name of a command or reserved word.
- Bit-identical across compilers: all functions are built from basic IEEE operations.

### Pose parameters `[Proposed — unconfirmed]`

- Rotation commands: `+ - & ^ < > ~ t : ;`.
- A pose parameter is an error in any other command, in a condition, weight, repeat count or `attr`
  value, and in the value of a parameter of another tier.
- Passed as a module argument, it carries these rules through that module's parameter.

## Syntax details `[Proposed — unconfirmed]`

### Statements and rules

- A statement or rule ends at the end of its line. A condition, and the argument list of a command,
  module or function, may span lines; a `repeat` or `mirror` block may span lines.
- A `grow` or `draw` block's closing `}` starts its own line.
- `(` is an argument list only directly after its command or module, with no space.
- Commands with no argument: `. [ ] { } % | $`. Any other command takes at most one.
- `grows` and `nogrowth` are written before the left side: `nogrowth F -> F F`.
- `}` closes the innermost open `repeat`, `mirror` or `{`, and is otherwise `endpolygon`. A rule on
  `endpolygon` spells the name. Inside a `repeat` or `mirror` block the serialiser writes `polygon` and
  `endpolygon` as names in either style.
- `#` comments are kept in the model, attached to the statement or rule after them, or on the same line.
- Unstated settings default to generations 4, angle 25, thickness 50%. `size` and `step` together are an
  error. `seed` is a whole number from 0 to 4294967295.

### `|`

A rule is weighted only if its body starts with `(`. In a weighted rule, `|` followed by `(`, outside any
`repeat`, `mirror` or `{`, separates alternatives. Every other `|` is `turnaround`. A weight is an
expression; a negative one counts as 0; if all are 0 the first alternative is used.

### Names

- Turtle commands may be a `grow` rule's left side, with no parameters: `F -> F F`, `+ -> -`.
- A command name or glyph is an error, with line and column, as a `draw` rule's left side, as a
  parametric left side (`draw(x) ->`), and as a macro, socket, `param` or module parameter name.
- Reserved in the same places, and as a module anywhere: `repeat mirror socket bone merge attr grows
  nogrowth fk ik pole aim query and or not`. Rig hints and `query` modules are errors until they are
  implemented.

### Rules

- All rules for a module, in `grow` and `draw`, have the same parameter count, and uses agree with it.
- The first rule in file order whose condition holds applies. A module with no matching `grow` rule stays
  as it is and keeps its generation.
- A `grow` rule on a command matches it with or without an argument. An argument moves to the last item
  of the replacement if that is a command taking an argument and having none; otherwise it is dropped.
- A `draw` rule's body holds no modules. Its commands take the generation of the module drawn.
- A warning with line and column: a rule after an unconditional rule for the same module; a rule on `F`
  or `Z` whose replacement has several segments.

### `axiom` names

- The first word after `axiom` is a name if it is not a command, not a rule's left side and not a macro,
  has no argument list, and something follows it. `A()` forces a module.
- A name that is also a rule's left side is an error. Two unnamed axioms or two of one name are errors.
- A build names an axiom; otherwise it uses the unnamed one, else the first.

### Macros and `repeat`

- A macro has no parameters, and its expressions read only parameters and settings. Using it with an
  argument list, and macro cycles, are errors. Macros may be declared after their use.
- A `repeat` count is an expression of parameters and settings, with no `rand`, rounded down; below 1
  gives no copies.
- Expansion counts against the work budget.

### `bone`, `merge`, `socket`, bone budget

- `merge`: from here, a drawn segment continuing the current bone extends it to the new end, with frame and
  radius recalculated, instead of adding a bone. `bone` ends this. Both are saved by `[` and restored by `]`.
  A move or `]` starts a new bone.
- `socket Flower`: a zero-length bone of kind Socket at the turtle, parented to the current bone, which
  stays current. It draws nothing and keeps the structural name `S_12`; the socket's name is reported beside it.
- `bonebudget 500`: a setting; the build uses the lower of this and its own bone budget.

### `mirror`

`mirror { X }` expands to `[ X ] [ X' ]`, where in `X'` the turtle swaps `+` with `-` and `<` with `>`,
including in everything that later grows from `X'`. Nested mirrors cancel.

### `attr`

`attr(name, value)` sets the named attribute on the turtle; `[` and `]` save and restore it. A bone takes the
values in force when it is created, 0 for one not set yet, and each vertex takes those of the bone whose channels
it takes. Names are columns in first-use order.

### Seed and size

The grammar's `seed` and `size` or `step` apply unless the build overrides them. A grammar stating neither takes
them from the build settings.

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

Built at the same settings, this produces the same bone tree as `TREE11.LS`, bit for bit in every field.

```
lsystem Tree11
credit  "after TREE11.LS by Lj Lapre, Lparser"

generations 12
angle       5
thickness   30%

shape param taper     = 0.8
shape param stretch   = 1.25
shape param shrink    = 0.8
shape param leaf_size = 200
param bark = 12
param leaf = 8

axiom  c(bark) Trunk

macro Tier = ~(5) Seg Whorl Whorl [ Droop BranchA ] Leaves >(30)

grow {
    Trunk   -> [ Seg ~(5) Seg Spine Leaves ]
    Spine   -> Tier Tier !(taper) Spine
    Whorl   -> [ Droop BranchA ] Leaves >(120)

    BranchA -> ~(10) $ Seg [ Left  BudA Leaves ] ~(10) $ Seg Leaves !(taper) BranchB
    BranchB -> ~(10) $ Seg [ Right BudB Leaves ] ~(10) $ Seg Leaves !(taper) BranchA
    BudA    -> BranchA
    BudB    -> BranchB

    Leaves  -> [ g(300) repeat 6 { g(40) Leaf } ]
    Leaf    -> [ ~ Edge(7, 0) c(leaf) { +(30) Edge(leaf_size, 0) -(120) Edge(leaf_size, 0) -(120) Edge(leaf_size, 0) } ]

    Seg            -> '(stretch) Seg '(shrink)
    Edge(len, age) -> Edge(len, age + 1)
    Droop          -> & Droop
    Left           -> + Left
    Right          -> - Right
}

draw {
    Seg                        -> F
    Edge(len, age) : age <= 1  -> f(len)
}
```
