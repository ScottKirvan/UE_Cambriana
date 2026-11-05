# Format Specification: Cambriana Grammar

Requirements: [SRS.md](SRS.md) (FR-24).

File extension `.lsys`.

The Cambriana grammar format is Cambriana's own format, not a dialect of any
other existing format, with named settings and parameters, rule parameters and whitespace. It is the only format grammar assets hold; legacy `.ls` files are
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
| Environment query | `query Light(amount)`: `query` modules whose parameters are set by an environment query between generations (AR-02); see Environment query |

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
- Conversion from the legacy dialect produces no hints.

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
  overrides. A parameter, macro or rule parameter cannot reuse those names, or the name of a command or reserved word. `[Proposed — unconfirmed]`
- All functions are built from basic IEEE operations, for results bit-identical across compilers. Real-time performance
  takes priority over bit-identity. `[Proposed — unconfirmed]`

### Pose parameters `[Proposed — unconfirmed]`

- Rotation commands: `+ - & ^ < > ~ t : ;`.
- A pose parameter is an error in any other command, in a condition, weight, repeat count or `attr`
  value, and in the value of a parameter of another tier.
- Passed as a module argument, it carries these rules through that module's parameter.

### Shape parameters `[Proposed — unconfirmed]`

- A shape parameter cannot change the bone tree. It is an error, with line and column, in a condition, weight or `repeat`
  count, and in the value of a `structure` parameter.
- Passed as a module argument, it carries this rule through that module's parameter. A `param` whose value reads it carries
  the rule too.

## Syntax details `[Proposed — unconfirmed]`

### Statements and rules

- A statement or rule ends at the end of its line. A condition, and the argument list of a command,
  module or function, may span lines; a `repeat` or `mirror` block may span lines.
- Line breaks are also allowed before a rule's `:` and `->`, inside a weight, and inside parentheses in a parameter value
  or `repeat` count. A line break in the parameter list of a rule or `query` is an error. `[Proposed — unconfirmed]`
- A `grow` or `draw` block's closing `}` starts its own line.
- `(` is an argument list only directly after its command or module, with no space.
- Commands with no argument: `. [ ] { } % | $`. Any other command takes at most one.
- Empty parentheses on a command, `F()`, are an error. `[Proposed — unconfirmed]`
- `~(x)` is not scaled by growth, as in the legacy dialect. `[Proposed — unconfirmed]`
- A turn argument `x` is `x * 0.017453` radians, as in the legacy dialect. `[Proposed — unconfirmed]`
- `lsystem` and `credit` may be repeated; the first of each in file order applies. `[Proposed — unconfirmed]`
- `grows` and `nogrowth` are written before the left side: `nogrowth F -> F F`.
- `}` closes the innermost open `repeat`, `mirror` or `{`, and is otherwise `endpolygon`. A rule on
  `endpolygon` spells the name. Inside a `repeat` or `mirror` block the serialiser writes `polygon` and
  `endpolygon` as names in either style.
- `#` comments are kept in the model, attached to the statement or rule after them, or on the same line.
- `thickness` may be negative. `[Proposed — unconfirmed]`
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
  nogrowth fk ik pole aim query and or not`. Rig hints are errors until they are implemented. `query` starts only a
  declaration.

### Rules

- All rules for a module, in `grow` and `draw`, have the same parameter count, and uses agree with it.
- The first rule in file order whose condition holds applies. A module with no matching `grow` rule stays
  as it is and keeps its generation.
- A `grow` rule on a command matches it with or without an argument. An argument moves to the last item
  of the replacement if that is a command taking an argument and having none; otherwise it is dropped. In the
  generation that wraps the replacement of a rule marked for fractional growth, it is dropped. `[Proposed — unconfirmed]`
- A `draw` rule's body holds no modules. Its commands take the generation of the module drawn.
- A warning with line and column: a rule after an unconditional rule for the same module; a rule on `F`
  or `Z` whose replacement has several segments, counted through `repeat`, `mirror` and macros at the grammar's own
  settings and parameter values. `[Proposed — unconfirmed]`

### `axiom` names

- The first word after `axiom` is a name if it is not a command, not a rule's left side, not a macro and not a
  `query` module,
  has no argument list, and something follows it. `A()` forces a module.
- A name that is also a rule's left side is an error. Two unnamed axioms or two of one name are errors.
- A build names an axiom; otherwise it uses the unnamed one, else the first.

### Macros and `repeat`

- A macro has no parameters, and its expressions read only parameters and settings. Using it with an
  argument list, and macro cycles, are errors. Macros may be declared after their use.
- A `repeat` count is an expression of parameters and settings, with no `rand`, rounded down; below 1
  gives no copies.
- Expansion counts against the work budget.
- A parameter's value is an expression of earlier parameters and settings, with no `rand`.

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

### Conversion details `[Proposed — unconfirmed]`

- "The same production": every symbol the legacy turtle acts on, with its argument and generation, in order.
  The legacy dialect's `production_length` and production position count characters and this format's count modules,
  so they differ, and production and work budgets are reached at different points. A build that reaches a budget is
  outside the identical-behaviour guarantee.
- The header's numbers are written as read. A recursion that is not positive becomes `generations 0`; an infinite one the
  largest finite number.
- A letter or `_` becomes the module of that name. A `F Z f z g t c` letter or any other command glyph stays a command.
  Any other symbol with a rule becomes `sym_XX`, XX its byte in upper-case hexadecimal.
- A symbol with no rule that is a letter or `_` gets the rule `nogrowth X ->`. If the file has a rule for `_`, a symbol
  with no rule outside the legacy built-in list gets `nogrowth X -> _`.
- Any other symbol with no rule, which does nothing in every generation, is left out, except as the last symbol of a rule
  on a command that reads an argument, where it stays so that the argument does not move to the command before it. An axiom
  left with nothing is `axiom axiom_empty`, with the rule `nogrowth axiom_empty ->`.
- Text is read as the legacy turtle reads it: a command that reads an argument, directly followed by `(`, takes
  everything to the next `)`, and its value is written as a number. Every other character is a symbol, so `(30)` behind a
  module or a command with no argument is three or four symbols that do nothing and are left out, and `(-30)` behind one
  keeps the `-`.
- Comments: a line the dialect ignores, text after the cut on a read line, a malformed rule and every line after `@` are
  comments in the same order, with their `#` if they had one. Malformed rules also give the legacy warning.
- Rules, including ones after the first for a symbol, stay in file order. A rule from the first `+` rule on that this
  format would mark gets `nogrowth`. `grows` is never needed.
- Diagnostics carry lines of the legacy text.
- The file is not convertible, reported as an error with line and column and giving no grammar, when it holds:
  - `@` as a symbol in the axiom or a replacement;
  - an argument with no closing `)` in its axiom or replacement;
  - an argument whose value is not finite, or changes when rewriting reaches its characters: one of them has a rule, or
    has none and turning it into `_` changes the number (`1e2`);
  - `angle` or `thickness` that is not finite;
  - a `(` in a replacement with nothing before it, or behind a symbol that can later become a command reading an
    argument or can disappear (`A(30)` with `A=+`);
  - an argument on a command whose replacement chain can leave it behind a symbol that later reads it, or behind whatever
    stands before a symbol that disappears, unless what stands before can never read it;
  - an argument that can end up unread, including through a rule marked for fractional growth, if its text has a command
    glyph or a character with a rule (`g(-5)` with `g=z`).

## Environment query `[Proposed — unconfirmed]`

- `query Light(amount, distance)` is a statement that declares a query module and its parameter names. `query Bare`
  declares one with none. The name is checked like a macro's, and rules and uses agree with the declared count.
- A query module is otherwise an ordinary module: it may have `grow` and `draw` rules and be used in macros and repeats.
- Between generations, after the axiom and after the last generation too, the environment is given the generation, the
  bone tree and every query module with its arguments and the turtle's position, frame and bone after drawing it.
- The bone tree is at the build's step length and min thickness in Step mode, and at a 1 cm step with no min thickness in
  Fit mode.
- The environment sets query module arguments, which are finite, and prunes bones.
- Pruning a bone: the modules on its branch (those that drew a removed bone or leave the turtle on one) stop growing and
  are no longer query modules. They stay in the production and count against budgets. Each later drawing draws them, so
  the turtle's random sequence is unchanged, and removes the bone and its descendants with their polygons and sockets.
  Such a module keeps the draw commands it had when the bone was pruned, whatever later random numbers would choose.
  `[Proposed — unconfirmed]`
  The remaining bones keep the names of the tree with the branch present, and their indices are compacted
  ([SDD-rigging.md](SDD-rigging.md), Pruning).
- Pruning one bone of a module's draw rule keeps the module's bones that are not its descendants.
- Each query counts its module count plus 64 against the work budget, and also pays for drawing the tree it shows. A
  query that the budget cannot pay for is not made, and the build stops with the production it has.
  `[Proposed — unconfirmed]` With an environment that changes nothing, the
  output is that of the unstepped build unless a budget is reached.

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
