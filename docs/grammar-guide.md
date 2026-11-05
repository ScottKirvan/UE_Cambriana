---
outline: [2, 3]
---

# Grammar Syntax Guide

A Cambriana grammar is a plain text file, with the extension `.lsys`, that describes how a form grows. Cambriana reads it and builds a rigged mesh from it.

::: warning Not usable in Unreal yet
**`.lsys` files can't be used in Unreal yet.** The plugin's import and grammar editor for them are still being built. The examples here were checked against the grammar engine directly, not inside Unreal. The format is also still being tested, so names and rules may change.
:::

## What is a grammar?

Imagine a small robot, the **turtle**, holding a pen. You give it a line of commands: draw forward, turn left, draw forward. Each line it draws becomes a **bone** of the skeleton, with mesh around it.

Writing out every command for a tree would take forever. So a grammar also has **rules**, which replace one word with several. Start from a single word (the **axiom**), apply the rules several times (the **generations**), and a short text turns into thousands of commands. Because a branch rule can contain another branch, the result branches like a plant. This idea is called an L-system.

## Your first grammar

**Step 1: draw.** `axiom` is the starting text and `F` means "draw forward". This draws three bones in a straight line, pointing up.

```text
axiom F F F
```

**Step 2: turn.** `+` turns left by the `angle` setting. This gives a chain that curves.

```text
angle 30
axiom F + F + F
```

**Step 3: branch.** `[` saves where the turtle is and `]` jumps back there, so everything between them is a side branch.

```text
angle 30
axiom F [ + F ] [ - F ] F
```

**Step 4: grow.** Now use a word of your own. `Branch` isn't a command, so the turtle would ignore it, but the rule in the `grow` block replaces it with a bone and two more `Branch` words. After three generations this draws 7 bones.

```text
generations 3
angle 30
axiom Branch

grow {
    Branch -> F [ + Branch ] [ - Branch ]
}
```

**Step 5: refine.** Name the grammar, set its size, and make each level thinner and shorter with a parameter you can tweak.

```text
lsystem FirstTree

generations 4
angle       30
thickness   20%
size        200cm
seed        1

shape param taper = 0.8

axiom Branch

grow {
    Branch -> F !(taper) '(taper) [ + Branch ] [ - Branch ]
}
```

## Writing the file

- One statement per line. A comment runs from `#` to the end of the line.
- The closing `}` of a `grow` or `draw` block goes on its own line. The opening `{` goes on the same line as `grow` or `draw`.
- A name uses letters, digits and `_`, doesn't start with a digit, and is case sensitive: `Left` is your own word, `left` is a turtle command.
- Save as UTF-8. A byte order mark at the start is fine.

Two spacing traps are worth learning now:

- `FF` is one word, not two commands. It draws nothing and gives no warning. Write `F F`. Symbols split words on their own, so `F+F` is fine.
- An argument touches its command: `F(50)`, never `F (50)`.

## Turtle commands

Every command has a short glyph and a long name, and you can mix them freely. These two grammars are the same.

```text
angle 30
axiom F + F
```

```text
angle 30
axiom draw left draw
```

**Moving.** `F` draws a bone. `f` moves without drawing and starts a new chain. An argument is a percentage of the starting step, so this draws one full bone, leaves a half-step gap, then draws a half bone.

```text
axiom F f(50) F(50)
```

**Turning.** With an argument, a turn uses that many degrees instead of `angle`. `+ -` turn left and right, `& ^` pitch, `< >` roll.

```text
angle 25
axiom F +(90) F & F >(45) ^ F
```

**Thickness and length.** `!` and `?` make later bones thinner and thicker, `'` and `"` make them shorter and longer. `thickness` is the bone's width as a percentage of its length, so a shorter bone is also thinner. With an argument, each of these multiplies by exactly that number.

```text
thickness 30%
axiom F ! F ' F !(0.5) F
```

**Polygons.** `{` starts a flat face and `}` ends it. Inside, `f` and `.` place corners. Use it for leaves and petals. A face needs at least three corners, counting the point where `{` was used.

```text
angle 60
axiom F { + f - - f - - f }
```

**Colour.** `c` adds one to the colour number, and `c(3)` sets it. The number selects a material slot.

```text
axiom c(3) F c F
```

The [command reference](#turtle-command-reference) lists them all.

## Rules

### Grow rules

A `grow` block holds rules of the form `Word -> replacement`. In each generation every word is replaced at the same time. A word with no rule stays as it is, and a replacement can be empty, which removes the word.

```text
generations 2
axiom Stem Bud

grow {
    Stem -> F Stem
    Bud  ->
}
```

Commands can be rewritten too, which is the classic way to write plants. Cambriana warns about the `F` rule here because it replaces one bone with several, so this form can't animate growth on one skeleton. It still builds.

```text
generations 3
angle 25
axiom F

grow {
    F -> F [ + F ] F
}
```

> **Warning** line 6, column 5: segment replaced by several.

### Draw rules

A `draw` block says how a word looks without changing how it grows. Words with no `draw` rule draw nothing. A `draw` rule can only contain commands, not other words of yours.

```text
generations 3
axiom Stem

grow {
    Stem -> Segment Stem
}

draw {
    Segment -> F
}
```

### Words with values

A word can carry values. Here each `Seg` remembers its length and passes on a shorter one. All rules for a word must take the same number of values.

```text
generations 4
axiom Seg(100)

grow {
    Seg(len) -> F(len) Seg(len * 0.7)
}
```

### Conditions

Add `: condition` before the arrow. The first rule in the file whose condition holds is used. Here a tip grows while it's young, then ends in a short bone.

```text
generations 5
axiom Tip(0)

grow {
    Tip(age) : age < 3 -> F Tip(age + 1)
    Tip(age)           -> F(30)
}
```

### Random choices

Start the replacement with a weight in parentheses and separate choices with `|`. Here the first choice is twice as likely as the second. The result depends on `seed`.

```text
generations 6
angle 30
seed 42
axiom Tip

grow {
    Tip -> (2) F Tip | (1) F [ + Tip ] [ - Tip ]
}
```

`|` is also the `turnaround` command. It only separates choices in a rule that starts with a weight, and only when followed by `(`.

### Fractional generations

`generations 2.5` runs three generations and draws the last at half scale, which is how growth is blended in. Rules are scaled this way by default, except rules for `F`, `f`, `Z` and `z`. Put `grows` or `nogrowth` in front of a rule to choose for yourself. Here the existing `F` scales down too.

```text
generations 2.5
axiom Stem F

grow {
    Stem -> F Stem
    grows F -> F
}
```

## Settings

Settings are a keyword and a number. A calculation isn't allowed here. Any setting can be left out. If `seed` and `size`/`step` are left out, they will come from the Unreal asset or actor.

```text
generations 3.5
angle       25
thickness   50%
size        150cm
seed        7
bonebudget  500
axiom F
```

`size` scales the whole form so its largest dimension is that many centimetres. `step` sets the length of one bone instead. Use one or the other. See the [settings reference](#settings-reference).

## Parameters

A parameter is a named number you can use in place of a number anywhere in rules. Its **tier** says what changing it costs.

```text
pose      param bend  = 20
shape     param taper = 0.8
structure param depth = 3
param     tint = 2

generations 2
axiom c(tint) Limb

grow {
    Limb -> repeat depth { +(bend) F !(taper) } Limb
}
```

| Tier | What it changes | Use it for |
|---|---|---|
| `pose` | Bone rotations only, so it can be animated | Bends, curls, droop |
| `shape` | Mesh, same skeleton | Thickness, length, taper |
| `structure` | Mesh and skeleton | Counts, which rules run |

Pose parameters are checked strictly, because they have to work on a finished skeleton. They're only allowed in rotation commands (`+ - & ^ < > ~ t : ;`), in a limited set of operators and functions, and in a word's value if that value is in turn only used in rotation commands. See [what pose parameters allow](#what-pose-parameters-allow).

```text
pose param curl = 15
generations 3
axiom Limb(curl)

grow {
    Limb(a) -> +(a) F Limb(a * 0.9)
}
```

Shape parameters are checked too, because they mustn't change the skeleton. Using one in a condition, a weight, a repeat count or a `structure` parameter's value is an error. The same goes for a plain `param` worked out from one, and for a word's value that received one.

A parameter's value can be a calculation that reads earlier parameters and settings, but not `rand`. Parameters can't be named after settings Cambriana lets you read (`generations`, `angle`, `thickness`, `seed`), commands (including the single letters `F f Z z g t c`), or reserved words.

## Expressions

Anywhere a number goes in an argument, condition, weight or repeat count, you can write a calculation. Expressions can read parameters, the word's values, and the settings `generations`, `angle`, `thickness` and `seed`. Trigonometry uses degrees. A true result is 1 and false is 0, and any non-zero number counts as true.

```text
shape param base = 80
generations 4
seed 3
axiom Seg(0)

grow {
    Seg(n) : n < generations and n % 2 == 0 -> F(base + rand(-10, 10)) Seg(n + 1)
    Seg(n) : n < generations                -> +(angle * sin(30)) F(max(base - n * 10, 20)) Seg(n + 1)
}
```

Inside an expression `^` means power and `%` means remainder, while in a command list they are the `up` and `rollaround` commands.

## Building blocks

**Macros** name a piece of text, which is pasted in where it's used. A macro takes no values and may be declared after its use.

```text
macro Fork = [ + F ] [ - F ]
axiom F Fork F Fork
```

**Repeat** copies its contents. The count is rounded down, and can't use `rand` or a word's values.

```text
angle 60
axiom repeat 6 { F + }
```

**Mirror** draws its contents and then a copy with left and right swapped, including everything that later grows from it. Each copy is its own branch.

```text
angle 30
axiom F mirror { + F }
```

**Named axioms** give you several starting points in one file, such as growth stages. With no name chosen, the unnamed axiom is used, otherwise the first one.

```text
axiom sapling F
axiom mature  F F F
```

The first word after `axiom` counts as a name if it isn't a command, a macro, a query or a word with a rule, and something follows it. This means a misspelt first word silently becomes a name. `A()` forces it to be a word instead.

```text
axiom Start() F
draw {
    Start -> F
}
```

**Merging bones.** After `merge`, drawn segments extend the current bone instead of adding new ones. `bone` turns this off, and a move or `]` also starts a new bone. This draws two bones.

```text
axiom merge F F F bone F
```

**Sockets** mark a named attachment point with a zero-length bone, for attaching flowers or props.

```text
axiom F F socket Flower
```

**Attributes** store a named value on the bones and vertices that follow, which can be read per vertex. `[` and `]` restore them.

```text
axiom attr(wetness, 0.5) F [ attr(wetness, 1) F ] F
```

**Credit** records where a grammar came from.

```text
credit "after a tree by a friend"
axiom F
```

**Queries** declare a word whose values something outside the grammar can change between generations, such as how much light reaches a branch. `query Light(amount)` declares `Light` with one value. When nothing answers, it behaves like any other word. `.lsys` files can't be used in Unreal yet, so nothing answers queries there yet.

```text
generations 3
query Light(amount)
axiom Light(1)

grow {
    Light(amount) -> F Light(amount)
}
```

## Not available yet

Rig hints (`fk`, `ik`, `pole`, `aim`) are planned. Their names are reserved, and using them today is an error.

```text
axiom F ik(foot)
```

> **Error** line 1, column 9: reserved name.

## Reading errors

Each problem has a severity, a kind, a line and a column. Lines and columns count from 1, and each character counts as one column, including a tab. Errors stop the build. Warnings don't. The kinds shown here are descriptions, and the wording in Unreal may differ. A *module* is the formal name for a word of your own.

A broken line is dropped, which can cause knock-on errors. Here the `param` line ends in the middle of a calculation, so `length` is never declared, and its use is a second error. **Fix the first error on the list first.**

```text
param length = 50 +
axiom F(length)
```

> **Error** line 1, column 20: syntax error.
>
> **Error** line 2, column 9: unknown name.

A space before an argument. The column points at the `(`.

```text
axiom F (50)
```

> **Error** line 1, column 9: syntax error.

A `}` at the end of a rule line draws as the end of a polygon, so the block never closes. The error is at the end of the file.

```text
axiom A
grow {
    A -> F }
```

> **Error** line 4, column 1: syntax error.

Using a name that was never declared.

```text
axiom F(lenght)
param length = 50
```

> **Error** line 1, column 9: unknown name.

Naming something after a command. Here the single letter `t` is the `gravity` command.

```text
param t = 1
axiom F
```

> **Error** line 1, column 1: command used as a name.

Using a setting's name for a parameter.

```text
param angle = 1
axiom F
```

> **Error** line 1, column 1: reserved name.

Giving a word the wrong number of values.

```text
axiom Seg
grow {
    Seg(len) -> F(len)
}
```

> **Error** line 1, column 7: wrong argument count.

Using one of your words inside a `draw` rule.

```text
axiom A
draw {
    A -> F Other
}
```

> **Error** line 3, column 12: module in a draw rule.

Using a pose parameter outside a rotation.

```text
pose param bend = 50
axiom F(bend)
```

> **Error** line 2, column 9: pose parameter misuse.

Using a function pose parameters don't allow. The column points at the function.

```text
pose param bend = 50
axiom +(floor(bend)) F
```

> **Error** line 2, column 9: pose parameter misuse.

Using a shape parameter where it would change the skeleton, here a repeat count.

```text
shape param count = 3
axiom repeat count { F }
```

> **Error** line 2, column 14: shape parameter misuse.

Using `rand` where a fixed value is needed.

```text
param a = rand(0, 1)
axiom F
```

> **Error** line 1, column 11: not constant.

A setting's value out of range.

```text
seed 1.5
axiom F
```

> **Error** line 1, column 6: invalid number.

Both `size` and `step`.

```text
size 100cm
step 2cm
axiom F
```

> **Error** line 2, column 1: size and step together.

Two axioms with no name, or the same setting twice.

```text
axiom F
axiom F F
```

> **Error** line 2, column 1: duplicate name.

Two macros that use each other.

```text
macro A = B
macro B = A
axiom A
```

> **Error** line 1, column 1: macro cycle.
>
> **Error** line 2, column 1: macro cycle.

A rule that can never run, because the rule before it always applies. This is a warning.

```text
axiom A
grow {
    A -> F
    A -> F F
}
```

> **Warning** line 4, column 5: unreachable rule.

A calculation with no valid answer, such as dividing by zero, uses 0 instead. This is a warning, reported at the start of the calculation, once per calculation. It applies everywhere, including a parameter's value, and a repeat count with no valid answer makes no copies.

```text
axiom F(1 / 0) F
```

> **Warning** line 1, column 9: non-finite value.

Unbalanced `]` and `}` also give warnings (`unmatched pop`, `unmatched polygon end`, `unclosed polygon`), but these come from drawing, not from the text, so they have **no line or column**: they appear as line 0, column 0.

```text
axiom ] F
```

> **Warning** line 0, column 0: unmatched pop.

## Legacy `.ls` files

Cambriana also reads the `.ls` files of Lparser, by Laurens Lapré, which this format descends from. The turtle commands are the same. As designed, a `.ls` file is converted to this format when imported, and you then work with the converted grammar. That conversion is still being built, so today's plugin reads `.ls` text directly.

- The three header numbers become `generations`, `angle` and `thickness`.
- Each rule, like `F=F[+F]F`, becomes a `grow` rule, and each symbol stays a single-character word.
- A symbol with no rule gets an empty replacement, since that's what `.ls` files do implicitly.
- Rules get `grows` or `nogrowth` where needed to match the old growth behaviour.
- Comments are kept. A `.ls` file has no seed, so the asset gets a generated one.

The converted grammar is meant to build the same bone tree as the original file.

## Reference

The formal definition of the format is in the [Grammar Format Specification](./grammar-spec).

### Turtle command reference

An argument, where allowed, replaces the default. Steps are percentages of the starting step, angles are degrees.

| Glyph | Name | Argument | What it does | Default |
|---|---|---|---|---|
| `F` | `draw` | percent | Draw a bone forward | One step |
| `Z` | `drawhalf` | percent | Draw a bone forward | Half a step |
| `f` | `move` | percent | Move without drawing; places a polygon corner | One step |
| `z` | `movehalf` | percent | Move without drawing; places a polygon corner | Half a step |
| `g` | `skip` | percent | Move without drawing; no polygon corner | One step |
| `.` | `vertex` | none | Place a polygon corner without moving | |
| `+` | `left` | degrees | Turn left | `angle` |
| `-` | `right` | degrees | Turn right | `angle` |
| `&` | `down` | degrees | Pitch one way | `angle` |
| `^` | `up` | degrees | Pitch the other way | `angle` |
| `<` | `rollleft` | degrees | Roll one way | `angle` |
| `>` | `rollright` | degrees | Roll the other way | `angle` |
| `%` | `rollaround` | none | Roll 180 degrees | |
| `\|` | `turnaround` | none | Turn 180 degrees | |
| `~` | `jitter` | degrees | Random turn, from `seed` | Up to 6 × `angle` |
| `t` | `gravity` | amount | Droop away from vertical | 0.2 |
| `$` | `level` | none | Roll so the turtle is level | |
| `!` | `thinner` | multiplier | Multiply thickness | × 0.7 |
| `?` | `thicker` | multiplier | Multiply thickness | ÷ 0.7 |
| `:` | `narrower` | multiplier | Multiply `angle` | × 0.9 |
| `;` | `wider` | multiplier | Multiply `angle` | ÷ 0.9 |
| `'` | `shorter` | multiplier | Multiply step length | × 0.9 |
| `"` | `longer` | multiplier | Multiply step length | ÷ 0.9 |
| `[` | `push` | none | Save the turtle's state | |
| `]` | `pop` | none | Return to the saved state | |
| `{` | `polygon` | none | Start a polygon | |
| `}` | `endpolygon` | none | End a polygon | |
| `c` | `colour` | number | Set the colour number | Add 1 |

These names and single letters can't be used for your own words in `draw` rules, macros, sockets, parameters or a word's values.

### Settings reference

| Setting | Value | Default |
|---|---|---|
| `generations` | 0 or more, may be fractional | 4 |
| `angle` | Degrees, may be negative | 25 |
| `thickness` | Any number, optional `%` directly after; below 0 draws bones as thin as possible | 50% |
| `size` | Above 0, optional `cm` directly after | From Unreal |
| `step` | Above 0, optional `cm` directly after | From Unreal |
| `seed` | Whole number, 0 to 4294967295 | From Unreal |
| `bonebudget` | Whole number, 0 or more; the build uses the lower of this and its own limit | The build's limit |

### Parameter tiers

| Declaration | Tier | Allowed in |
|---|---|---|
| `pose param` | Pose | Rotation commands and word values, as below |
| `shape param` | Shape | Anywhere except conditions, weights, repeat counts and `structure` parameter values |
| `structure param` | Structure | Anywhere |
| `param` | None | Anywhere; worked out from a shape parameter, as for `shape param` |

### Operators and functions

Tightest first:

| Operators | Meaning |
|---|---|
| `( )` | Grouping |
| `^` | Power, groups right to left: `-2 ^ 2` is -4 |
| `-` | Negative |
| `* / %` | Multiply, divide, remainder: `-1 % 3` is 2 |
| `+ -` | Add, subtract |
| `< <= > >= == !=` | Comparisons |
| `not` | Not |
| `and` | And |
| `or` | Or |

| Function | Result |
|---|---|
| `if(c, a, b)` | `a` if `c` is true, otherwise `b`; both are always calculated |
| `abs(x)` `floor(x)` `ceil(x)` `round(x)` `sqrt(x)` | The usual meanings; `round` takes halves away from zero |
| `min(a, b)` `max(a, b)` | Smaller, larger |
| `clamp(x, low, high)` | `x` kept within a range |
| `lerp(a, b, t)` | Blend from `a` to `b` by `t` |
| `pow(a, b)` | `a` to the power `b` |
| `exp(x)` `log(x)` | Natural exponent and logarithm |
| `sin(d)` `cos(d)` `tan(d)` | Trigonometry, in degrees |
| `asin(x)` `acos(x)` `atan(x)` `atan2(y, x)` | Inverse trigonometry, results in degrees |
| `rand(a, b)` | Random from `a` up to `b`, from `seed` |

### What pose parameters allow

An expression using a pose parameter, and a pose parameter's own value, may contain only:

- `+ - * /`, negative, and power `^`
- `abs min max clamp lerp sqrt sin cos tan`

`pow` isn't allowed, so write `a ^ b` instead.

A pose parameter may appear only in the arguments of `+ - & ^ < > ~ t : ;`, in the value of another pose parameter, and in a word's value. A word's value that received one carries the same limits. It's an error in any other command, a condition, a weight, a repeat count, an `attr` value, or a parameter of another tier.

### Reserved words

Not usable as your own words: `repeat mirror socket bone merge attr grows nogrowth fk ik pole aim query and or not`.
