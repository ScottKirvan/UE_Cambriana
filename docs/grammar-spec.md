---
outline: [2, 3]
---

# Grammar Format Specification

The Cambriana grammar format, file extension `.lsys`, for programmers who write tools that read or generate it, or who implement it. For an introduction, see the [Syntax Guide](./grammar-guide).

::: warning Not usable in Unreal yet; not stable
**`.lsys` files can't be used in Unreal yet.** The plugin's import and grammar editor for them are still being built. This document describes the grammar engine directly, and that engine is what is normative here. **The format is not stable:** anything in this document may change without notice, and there is no format version number yet.
:::

Throughout, "the caller" is whatever invokes a build.

## Encoding and lexical rules

A file is a sequence of bytes treated as ASCII, so any UTF-8 is accepted inside comments and strings. Other bytes outside those two are a syntax error. A UTF-8 byte order mark at the very start of a file is skipped, and column numbers on the first line don't count it; anywhere else it's a syntax error. Column numbers count bytes, from 1. Lines count from 1. `\n`, `\r\n` and a lone `\r` each end a line.

| Token | Definition |
|---|---|
| Blank | Space or tab. Blanks may separate any two tokens, except as listed below |
| Comment | `#` to the end of the line, except inside a string |
| Word | A letter or `_`, then letters, digits or `_`. Case sensitive. Matched greedily: `FF` is one word |
| Number | See the grammar. Always non-negative in an expression; values too large for a double are an error |
| String | Double quotes on one line. `\"` and `\\` are escapes; any other backslash is literal |
| Glyph | One of `+ - & ^ < > % \| ~ $ ! ? : ; ' " [ ] { } .` |

Blanks are not allowed between:

- a word or glyph and the `(` of its arguments, including functions, `attr`, and the parameter list of a rule's left side or a `query` declaration
- a number and its unit, or the `-` and number of a setting value
- the two characters of `->`, `<=`, `>=`, `==`, `!=`

### Line structure

Each statement or rule ends at the end of its line, after an optional comment. Line breaks, and comments, are allowed only:

- inside any parentheses of an expression, a command's or module's arguments, or a weight
- anywhere inside `attr( )`
- between a rule's left side and `->`, including around its condition
- between items while a `repeat` or `mirror` block, or a `{ }` polygon, is open

Line breaks aren't allowed inside the parameter list of a rule or `query`, right after `->`, or at the top level of a parameter's value or repeat count. A `grow` or `draw` keyword and its `{` share a line. A rule may follow that `{` on the same line. The block's closing `}` must start a line, since anywhere else it's the `endpolygon` command. Even at the start of a line, a `}` closes the block only when no polygon, `repeat` or `mirror` is open. Otherwise it ends the innermost one, so this rule's polygon spans three lines and builds what `Leaf -> { + f - - f - - f }` builds:

```text
generations 1
axiom F Leaf
grow {
    Leaf -> {
        + f - - f - - f
    }
}
```

A polygon, `repeat` or `mirror` still open at the end of the file is code 9, at the end of the file. Only the glyph `{` keeps a body open. The name `polygon` doesn't, so a polygon that a later rule finishes is started with `polygon`. Text after the block's closing `}`, other than a comment, is an error.

### Keywords and names

Statement keywords are recognised only at the start of a statement and aren't reserved elsewhere: `lsystem credit param pose shape structure macro axiom grow draw generations angle thickness size step seed bonebudget sides surface minthickness`.

`query` is a statement keyword too, and is also reserved.

Command words are the names and single-letter glyphs in the [command table](#commands). Reserved words are `repeat mirror socket bone merge attr grows nogrowth fk ik pole aim query`, with the operators `and or not` treated alike. Readable settings are `generations angle thickness seed`. Function names aren't reserved.

| Where a name appears | Command word | Reserved word | `and or not` | Readable setting |
|---|---|---|---|---|
| Parameter, macro, rule parameter, `query` name or parameter | Code 10 | Code 11 | Code 11 | Code 11 |
| `socket` name | Code 10 | Code 11 | Code 11 | Accepted |
| Item in a body | Is the command | Its own construct, else code 11 | Code 11 | Module |
| `grow` left side, no parameters | Rule on the command | Code 11 | Code 11 | Module |
| `grow` left side, with parameters | Code 10 | Code 11 | Code 11 | Module |
| `draw` left side | Code 10 | Code 11 | Code 11 | Module |
| `attr` name, `lsystem` name | Accepted | Accepted | Accepted | Accepted |

`fk`, `ik`, `pole` and `aim` are reserved for rig hints, which are not available yet: using one is code 11. `query` only starts a [query declaration](#environment-queries).

## Grammar

ISO-style EBNF. Blanks and line breaks follow the rules above and aren't written. A file may have blank lines and comments between statements, and may indent them freely. Statements may be in any order.

```text
newline      = "\r\n" | "\n" | "\r" ;
word         = word-start , { word-start | digit } ;
word-start   = "A" | ... | "Z" | "a" | ... | "z" | "_" ;
digit        = "0" | ... | "9" ;
number       = ( digit , { digit } , [ "." , { digit } ] | "." , digit , { digit } ) , [ exponent ] ;
exponent     = ( "e" | "E" ) , [ "+" | "-" ] , digit , { digit } ;
string       = '"' , { string-char } , '"' ;
string-char  = '\"' | "\\" | ? any character but newline or double quote ? ;
comment      = "#" , { ? any character but newline ? } ;
glyph        = "+" | "-" | "&" | "^" | "<" | ">" | "%" | "|" | "~" | "$" | "!" | "?" | ":" | ";"
             | "'" | '"' | "[" | "]" | "{" | "}" | "." ;

file         = { statement } ;
statement    = lsystem | credit | setting | parameter | macro | query | axiom | block ;
lsystem      = "lsystem" , word ;
credit       = "credit" , string ;
setting      = setting-name , setting-value | "surface" , surface-value ;
setting-name = "generations" | "angle" | "thickness" | "size" | "step" | "seed" | "bonebudget" | "sides"
             | "minthickness" ;
setting-value = [ "-" ] , number , [ unit ] ;
surface-value = "tubes" | "blocks" ;
unit         = "%" | "cm" ;
parameter    = [ tier ] , "param" , word , "=" , expression ;
tier         = "pose" | "shape" | "structure" ;
macro        = "macro" , word , "=" , body ;
query        = "query" , word , [ formals ] ;
axiom        = "axiom" , body ;
block        = ( "grow" | "draw" ) , "{" , { rule } , "}" ;

rule         = [ growth-flag ] , left-side , [ ":" , expression ] , "->" , alternatives ;
growth-flag  = "grows" | "nogrowth" ;
left-side    = word , [ formals ] | glyph ;
formals      = "(" , [ word , { "," , word } ] , ")" ;
alternatives = body | weighted , { "|" , weighted } ;
weighted     = "(" , expression , ")" , body ;

body         = { item } ;
item         = command | module | repeat | mirror | socket | attribute | "bone" | "merge" ;
command      = ( glyph | word ) , [ arguments ] ;
module       = word , [ arguments ] ;
arguments    = "(" , [ expression , { "," , expression } ] , ")" ;
repeat       = "repeat" , expression , "{" , body , "}" ;
mirror       = "mirror" , "{" , body , "}" ;
socket       = "socket" , word ;
attribute    = "attr" , "(" , word , "," , expression , ")" ;

(* Precedence and associativity of binary-op and prefix-op follow the Operators table. *)
expression   = operand , { binary-op , operand } ;
operand      = { prefix-op } , primary ;
primary      = number | word | call | "(" , expression , ")" ;
call         = word , "(" , expression , { "," , expression } , ")" ;
prefix-op    = "-" | "not" ;
binary-op    = "or" | "and" | "<" | "<=" | ">" | ">=" | "==" | "!=" | "+" | "-" | "*" | "/" | "%" | "^" ;
```

The grammar is ambiguous on its own. These rules settle it:

- A `command` is a `word` that is a command word; any other word in a body is a `module`, or a `repeat`, `mirror`, `socket`, `attr`, `bone` or `merge` construct.
- `}` closes the innermost open `repeat`, `mirror` or `{`, and otherwise is the `endpolygon` command. A rule on `endpolygon` must spell the name.
- A rule is weighted only if its body starts with `(`. In a weighted rule, `|` followed by `(`, outside any open `repeat`, `mirror` or `{`, separates alternatives. Every other `|` is `turnaround`. `|` then `(` in an unweighted rule is a syntax error.
- `->` inside a body isn't an arrow. It's the commands `right` and `rollright`.
- Commands accept at most one argument, and `. [ ] { } % |` and `$` accept none. `F()` is code 14.
- A `module`'s empty argument list, `A()`, is kept and is the same as no arguments.
- `grows` and `nogrowth` go before the left side, and a block can have no rules.
- A `setting-value` allows `%` only for `thickness` and `cm` only for `size` and `step`. Anything else there is a syntax error.

## Operators

Tightest first. `( )` and function calls are atoms.

| Level | Operators | Arity | Associativity |
|---|---|---|---|
| 8 | `^` | 2 | Right |
| 7 | `-` (negate) | 1 | Prefix |
| 6 | `* / %` | 2 | Left |
| 5 | `+ -` | 2 | Left |
| 4 | `< <= > >= == !=` | 2 | Left |
| 3 | `not` | 1 | Prefix |
| 2 | `and` | 2 | Left |
| 1 | `or` | 2 | Left |

A prefix operator may appear before any operand and takes everything that binds tighter than it to its right. So `-2 ^ 2` is -4, `2 ^ -1` is 0.5, `1 + not 0 + 2` is `1 + not (0 + 2)`. There is no unary `+`.

## Expression evaluation

Every value is an IEEE 754 double. There are no other types. Comparisons and logical operators give 1 or 0, and treat non-zero as true (a NaN is non-zero). All operands and all function arguments are always evaluated, including in `and`, `or` and `if`.

| Function | Definition |
|---|---|
| `if(c, a, b)` | `a` if `c` is non-zero, else `b` |
| `abs(x)` `floor(x)` `ceil(x)` `sqrt(x)` | As usual |
| `round(x)` | Nearest whole number, halves away from zero |
| `min(a, b)` | `b` if `b < a`, else `a` |
| `max(a, b)` | `b` if `a < b`, else `a` |
| `clamp(x, low, high)` | `min(max(x, low), high)` |
| `lerp(a, b, t)` | `a + (b - a) * t` |
| `pow(a, b)` | Same value as `a ^ b`, but not allowed with [pose parameters](#parameters-and-tiers) |
| `exp(x)` `log(x)` | Base e |
| `sin(d)` `cos(d)` `tan(d)` | Argument in degrees |
| `asin(x)` `acos(x)` `atan(x)` | Result in degrees |
| `atan2(y, x)` | Result in degrees |
| `rand(a, b)` | `a + (b - a) * u`; see [random numbers](#random-numbers) |

`a % b` is floored: the result takes the sign of `b`. `-1 % 3` is 2, `1 % -3` is -2. Wrong argument count is code 14, and an unknown function or name is code 12.

An expression may read:

| Where | Rule parameters | Parameters | Readable settings | `rand` |
|---|---|---|---|---|
| Parameter's value | No | Earlier ones only | Yes, wherever stated | No, code 21 |
| Repeat count | No | All | Yes | No, code 21 |
| Macro body | No | All | Yes | Yes |
| Axiom | No | All | Yes | Yes |
| Rule condition, weight, arguments | Yes | All | Yes | Yes |

Rule parameters hide parameters of the same name. Readable settings give their value after build overrides. `size`, `step`, `bonebudget`, `sides`, `surface` and `minthickness` can't be read, so they're available as parameter names. An unstated setting reads its default.

**Non-finite values.** Only the final value of an expression is checked, so `if(1, 50, 1 / 0)` is 50 with no warning. A final value that isn't finite, in a condition, weight, argument, `attr` value, parameter's value or repeat count, becomes 0 and gives code 23, once per expression, at the start of the expression. So a repeat count that isn't finite gives no copies.

A weight is only evaluated if its rule has more than one alternative.

## Commands

The turtle has unit vectors forward `H`, left `L` and up `U`. Rotation follows the right-hand rule about the axis given, and vectors are renormalised after turning. `a` is the current angle, or `x × 0.017453` radians when written `(x)`. That constant is exact and part of the format: an argument of 90 isn't a true right angle.

| Glyph | Name | Argument | Effect | Default |
|---|---|---|---|---|
| `F` | `draw` | Percent of the starting step | Move forward, drawing a segment | Current step |
| `Z` | `drawhalf` | Percent of the starting step | As `F` | Current half step |
| `f` | `move` | Percent of the starting step | Move forward with no segment, record a polygon vertex, break the chain | Current step |
| `z` | `movehalf` | Percent of the starting step | As `f` | Current half step |
| `g` | `skip` | Percent of the starting step | As `f`, recording no vertex | Current step |
| `.` | `vertex` | None | Record a polygon vertex | |
| `+` | `left` | Degrees | Rotate `H`, `L` about `U` by `-a` | Current angle |
| `-` | `right` | Degrees | Rotate `H`, `L` about `U` by `+a` | Current angle |
| `&` | `down` | Degrees | Rotate `H`, `U` about `L` by `+a` | Current angle |
| `^` | `up` | Degrees | Rotate `H`, `U` about `L` by `-a` | Current angle |
| `<` | `rollleft` | Degrees | Rotate `L`, `U` about `H` by `-a` | Current angle |
| `>` | `rollright` | Degrees | Rotate `L`, `U` about `H` by `+a` | Current angle |
| `%` | `rollaround` | None | Rotate `L`, `U` about `H` by 180 degrees | |
| `\|` | `turnaround` | None | Rotate `H`, `L` about `U` by 180 degrees | |
| `~` | `jitter` | Degrees, range `r` | Rotate `H`, `L` about `U`, then `H`, `U` about `L`, then `L`, `U` about `H`, each by a random angle in `[-r, r)` | `r` = 6 × current angle |
| `t` | `gravity` | Amount `g` | Unless `H` has no horizontal part: `d` = normalised `(-Hx, -Hy, 0)`; rotate `H`, `U` about `L` by `-(g × H·d)` | `g` = 0.2 |
| `$` | `level` | None | Unless `H` is exactly `(0, 0, 1)`: `L = H × Z`, `U = H × L`, negating both if `Uz < 0`. Not normalised | |
| `!` | `thinner` | Multiplier | Thickness × `x` | × 0.7 |
| `?` | `thicker` | Multiplier | Thickness × `x` | ÷ 0.7 |
| `:` | `narrower` | Multiplier | Angle × `x` | × 0.9 |
| `;` | `wider` | Multiplier | Angle × `x` | ÷ 0.9 |
| `'` | `shorter` | Multiplier | Step and half step × `x` | × 0.9 |
| `"` | `longer` | Multiplier | Step and half step × `x` | ÷ 0.9 |
| `[` | `push` | None | Push the turtle state | |
| `]` | `pop` | None | Pop it and break the chain. Empty stack: code 6, and only the chain breaks | |
| `{` | `polygon` | None | Start a polygon, recording its first vertex here. Saves any polygon in progress | |
| `}` | `endpolygon` | None | Finish the polygon and restore any saved one. None in progress: code 7 | |
| `c` | `colour` | Number | Colour = `x`, truncated toward zero | Colour + 1 |

Other body items:

| Item | Effect |
|---|---|
| `merge` | Turn merging on |
| `bone` | Turn merging off |
| `socket Name` | Add a zero-length socket bone at the turtle, parented to the current bone, which stays current. Names may repeat |
| `attr(name, value)` | Set a named attribute on the turtle |
| `mirror { X }` | See [mirror](#macros-repeat-and-mirror) |
| `repeat n { X }` | See [repeat](#macros-repeat-and-mirror) |

## Settings

A setting's value is a literal, not an expression. Stating one twice is code 13. Stating both `size` and `step` is code 22. Out of range, or a `surface` word other than `tubes` or `blocks`, is code 2.

| Setting | Range | Unit | Default | Build override |
|---|---|---|---|---|
| `generations` | ≥ 0, fractional allowed | | 4 | Yes |
| `angle` | Any, degrees | | 25 | Yes |
| `thickness` | Any, percent of segment length | `%` optional | 50 | Yes |
| `size` | > 0 | `cm` optional | From the build | Yes |
| `step` | > 0 | `cm` optional | From the build | Yes |
| `seed` | Whole, 0 to 4294967295 | | From the build | Yes |
| `bonebudget` | Whole, ≥ 0 | | No limit of its own | The lower of it and the build's bone budget applies |
| `sides` | Whole, 3 to 4294967295 | | From the build | Yes |
| `surface` | `tubes` or `blocks` | | From the build | Yes |
| `minthickness` | ≥ 0 | `cm` optional | From the build | Yes |

`size` chooses the step length so the largest dimension of the bounding box of the full-growth surface equals the value. `step` sets it directly. With neither, the build's own size mode and value apply, with a default of fit to 200 cm.

A stated setting with a build override applies unless the build overrides it.

`surface tubes` gives each bone a tube with `sides` sides. `surface blocks` gives each bone a box, and `sides` has no effect. `minthickness` is the least thickness of a bone, twice its radius. With any of these unstated, the build's own value applies, with defaults of 8 sides, tubes and 0 cm.

`lsystem` and `credit` have no effect on a build, and may be stated more than once.

## Parameters and tiers

`[tier] param name = expression` declares a named constant. Each name may be declared once (code 13). Its value is computed before rewriting.

| Tier | Meaning |
|---|---|
| `pose` | Only rotates bones. Restricted, below |
| `shape` | Changes geometry but not the bone tree. Restricted, below |
| `structure` | Meant to change the bone tree |
| None | No stated intent |

`structure` and untiered parameters are treated alike, except that an untiered parameter whose value reads a shape-restricted parameter becomes shape-restricted itself. Shape-restricted means a `shape` parameter, such an untiered parameter, or a rule parameter that is passed one.

| Construct | `pose` | Shape-restricted | Other |
|---|---|---|---|
| Argument of `+ - & ^ < > ~ t : ;` | Yes | Yes | Yes |
| Argument of any other command, `attr` value | No | Yes | Yes |
| Module argument | Yes; that rule parameter becomes pose-restricted, transitively | Yes; that rule parameter becomes shape-restricted, transitively | Yes |
| Condition, weight, repeat count | No | No | Yes |
| Value of a `pose` parameter | Yes | Yes | Yes |
| Value of a `shape` or untiered parameter | No | Yes | Yes |
| Value of a `structure` parameter | No | No | Yes |

An expression that reads a pose parameter or pose-restricted rule parameter, and every `pose` parameter's own value, may contain only `+ - * /`, negate, `^`, and `abs min max clamp lerp sqrt sin cos tan`.

Pose violations are code 15. A use in the wrong place reports the name, and a disallowed operator or function reports the operator or function, so one expression may give several. Shape violations are code 25, once per expression, at the first shape-restricted name in it.

## Build overview

A build runs four steps in order:

1. **Axiom and parameters.** The axiom is chosen, every parameter's value is computed, and macros, `repeat` and `mirror` are expanded.
2. **Grow.** `grow` rules rewrite the production for each [generation](#generations), with [growth marking](#growth-marking) in the extra pass of a fractional `generations`.
3. **Draw.** One pass of [draw rules](#draw-rules) turns modules into commands. A word that has no `draw` rule and isn't a command draws nothing.
4. **Turtle.** The turtle walks the commands and makes the bones, polygons and sockets, and from them the surface.

## Rewriting

### Generations

The axiom is the starting production. `generations` `g` runs `floor(g)` passes, plus one more if `g` has a fractional part `p`. Each pass replaces every item at once, left to right, into a new production. Each item records the generation that created it: 0 for the axiom, or the pass number.

The commands in a body are items just like modules, and can be rewritten. An item with no applicable rule is copied, keeping its generation and arguments.

### Rule selection

Rules for one left side are tried in file order, across all `grow` blocks. The first with no condition, or whose condition is non-zero, applies.

Code 14 applies if a module's rules, in `grow` and `draw`, have different parameter counts, or if a use differs from them or from its `query` declaration. A module with no rules or declaration accepts any number of arguments at each use. Module arguments are evaluated when the replacement is written, reading the parameters of the module being replaced. In `grow` these are the only inputs: no surrounding items are visible.

A rule on a command matches it with or without an argument. If the command had one, it moves to the last item of the replacement when that is a command that accepts an argument and has none. Otherwise it is dropped, and it is always dropped in the extra pass for a [marked](#growth-marking) rule.

### Alternative selection

With several alternatives, each weight is evaluated. Non-positive weights count as 0. If the total is not positive and finite, the first alternative is used with no random number drawn. Otherwise, with `u` from the random sequence, the first alternative with `u × total` below the running sum of weights is used.

### Growth marking

In the extra pass for fractional `p`, the replacement of a marked rule is wrapped in growth markers. A rule is marked unless its left side is `F`, `f`, `Z` or `z`; `grows` and `nogrowth` override this.

Drawing an opening marker saves step, half step, angle and thickness and multiplies each by `p`. A closing marker restores them. Between markers:

- step arguments, angle arguments except `~`, and the `t` argument are multiplied by `p`
- a multiplier `x` on `! ? : ; ' "` becomes `1 + p(x - 1)`
- default multipliers become `1 + p(0.7 - 1)` and `1 + p(0.9 - 1)`, dividing for `?`, `;` and `"`

### Macros, repeat and mirror

All three are expanded before rewriting, wherever they appear.

**Macros.** A body use of a macro's name, which takes no arguments, is replaced by its body. Using one with an argument list, even an empty one, is code 14. A macro may be used before it's declared. A cycle is code 16. A rule whose left side is a macro's name is accepted and never applies.

**Repeat.** `repeat n { X }` writes `X` `floor(n)` times; below 1 gives none. The count is fixed before rewriting. Each copy's expressions are evaluated separately when written, so `rand` differs per copy.

**Mirror.** `mirror { X }` is `[ X ] [ m X ]`, where `m` toggles a mirrored flag in the turtle state. While it's set, `+` and `-` swap and `<` and `>` swap, for every item drawn, including items that grow from `X` later and draw rules. Nested mirrors cancel.

### Axioms

```text
axiom sapling F
axiom mature F F
axiom Start() F
```

The first item of an axiom is taken as its name if it's a module with no argument list, isn't a left side of any rule, isn't a macro or query module, and at least one item follows. Otherwise the axiom is unnamed, and `A()` is how to force a module. Duplicate names and multiple unnamed axioms are code 13. A build names one axiom, and an unknown name is code 18. With no name, the unnamed axiom is used, else the first.

### Environment queries

```text
query Light(amount)
axiom Light(0) F
```

`query Name(a, b)` declares a query module and its parameter names; `query Name` declares one with none. The name is checked like a macro's: a command word is code 10, a reserved word or readable setting is code 11, and reusing a macro's or another query's name is code 13. Its rules, in `grow` and `draw`, and every use must have the declared number of parameters (code 14). Otherwise it's an ordinary module, and a build without an environment treats it as one.

A build may be given an environment by the caller. After the axiom and after each generation, the last one included, the environment is shown the generation, the bone tree drawn so far, and every query module with its arguments and the turtle's position, frame and bone after drawing it. It may set query module arguments, which must be finite, for the next generation's rules, and it may prune bones. An environment that changes nothing gives the same output as a build without one, unless a budget is reached.

Pruning a bone removes it and its descendants from that step on, and nothing grows from them. The modules on the pruned branch stop growing and are no longer query modules, but stay in the production and keep the draw commands they had when pruned, whatever later random numbers would choose. The remaining bones keep the names they would have had if the pruned bones were still drawn.

## Drawing

### Draw rules

After rewriting, the production is walked once. A command is kept. A module with a `draw` rule is replaced by that rule's commands, chosen as for [rule selection](#rule-selection) and [alternatives](#alternative-selection) but from the draw random sequence. The commands take the generation of the module. A module with no `draw` rule, or none that applies, draws nothing with no warning.

A `draw` rule's left side can't be a command, and its body, after macros, can't contain modules (code 17).

### Start state

| State | Start | Saved by `[` |
|---|---|---|
| Position | `(0, 0, 0)` | Yes |
| `H`, `L`, `U` | `(0, 0, 1)`, `(0, 1, 0)`, `(1, 0, 0)` | Yes |
| Step | From `size` or `step` | Yes |
| Half step | 0.5 × step | Yes |
| Angle | `angle` × 0.017453 radians | Yes |
| Thickness | `thickness` / 100 | Yes |
| Colour | 0 | Yes |
| Current bone | None | Yes |
| Attributes | All 0 | Yes |
| Mirrored, merging | Off | Yes |
| Polygons in progress | None | No |
| Chain broken | No | No |
| Growth markers | Closed | No |

The stack has no limit. A `[` left open at the end is not reported.

### Bones

Each `F` or `Z` adds a bone from the turtle's position to the new position, with radius `0.5 × length × thickness`, or 0 if that is negative, with `minthickness` also applying, and parent the current bone. It becomes the current bone. `f`, `z`, `g` move without changing the current bone, but break the chain.

While merging, if a current bone exists and the chain isn't broken, a segment instead extends the current bone to the new end, recalculating its frame and radius. This applies to the bone current at the time, including the parent bone right after a `[`. A later `]` still returns the turtle to where it was, which can leave a bone extending past its child's start.

A polygon finishing with at least 3 recorded vertices adds a zero-length attachment bone at the first vertex, parented to the bone current at its `{`. With fewer it adds nothing, without a warning. `F` and `Z` never record vertices. A polygon open at the end gives code 8 and is dropped.

Bones are output depth first. Their names are their kind and index: `F_0`, `Z_1`, `S_2` for a socket, `P_3` for a polygon.

An attribute's value applies to bones created while it's set, and to their vertices, with 0 until first set. Columns are attribute names in first-use order.

### Random numbers

The generator is the Visual C++ `rand()` sequence: `state = state × 214013 + 2531011` modulo 2<sup>32</sup>, output `(state >> 16) & 0x7FFF`, and `u = output / 32768`. Three independent sequences start from the effective seed:

| Sequence | Start state | Used by |
|---|---|---|
| Grow | `seed XOR 0x52455752` | Axiom, `grow` conditions, weights and arguments |
| Draw | `seed XOR 0x44524157` | `draw` conditions, weights and arguments |
| Turtle | `seed` | `~` only |

Within a sequence, numbers are drawn in processing order: items left to right, a rule's conditions in order, then its weights, then the single choice, then the replacement's arguments. `~` draws three values, each giving `output / 32768 × 2r - r`.

## Budgets

The caller sets budgets. Reaching one sets a flag in the result. It isn't a diagnostic.

| Budget | Default | Flag | When reached |
|---|---|---|---|
| Production | 2,000,000 items | 2 | Axiom: truncated. Rewriting: stops mid-pass and keeps the partial production. Draw rules: stops adding commands. Whatever was reached is drawn |
| Work | 16,000,000 units | 16 | Compiling, including expansion of macros, repeat and mirror: nothing is built. Axiom, if it and its drawing don't fit: nothing is built. Rewriting: stops and keeps the last complete generation, which is drawn in full. Environment query: not made; the build stops with the production it has. Attributes: bones and surface are kept, with no attributes |
| Bones | No limit | 4 | Drawing stops; bones so far are kept. `bonebudget` can only lower it |
| Vertices | No limit | 32 | Bones are kept; the surface is left incomplete |

A work unit is an implementation detail, roughly one item or expression step. The work budget is one account: compiling, rewriting, each environment query, and drawing all spend from it. Rewriting keeps back what drawing its production can cost, which is how reaching the budget still draws the last complete generation. An environment query costs its number of query modules, plus 64, plus drawing the tree it shows. Other result flags: 1 for errors, in which case nothing is built, and 8 for cancellation.

## Determinism

- **The same build of the engine, given the same text, axiom, settings and seed, gives identical output.**
- Different compilers and platforms aim for bit-identical output, but this gives way to real-time performance. Where the two conflict, performance wins and results may differ slightly between compilers. Don't rely on cross-compiler identity.
- Results don't depend on threads or global state, and nothing is seeded from the clock. The caller supplies any seed the grammar doesn't state.

## Diagnostics

A diagnostic has a severity, a numeric code, a line and a column. It has no message text. Errors stop the build. Parsing continues after an error, at the next line, and the failed statement is dropped, so later diagnostics may be consequences of it: for example, every use of a parameter whose line failed is code 12. A failed `axiom` line doesn't also give code 4.

| Code | Label | Severity | Meaning | Position |
|---|---|---|---|---|
| 1 | Missing end marker | | Legacy `.ls` files only; not produced for `.lsys` | |
| 2 | Invalid number | Error | Number malformed, too large, or out of range for its setting, or a `surface` word other than `tubes` or `blocks` | The number or word |
| 3 | Missing setting | | Legacy `.ls` files only; not produced for `.lsys` | |
| 4 | Missing axiom | Error | No axiom in the file, or an axiom with an empty body | 1:1, or the statement |
| 5 | Malformed rule | | Legacy `.ls` files only; not produced for `.lsys` | |
| 6 | Unmatched pop | Warning | `]` with an empty stack | 0:0, see below |
| 7 | Unmatched polygon end | Warning | `}` with no polygon in progress | 0:0, see below |
| 8 | Unclosed polygon | Warning | Polygon open at the end, once for each | 0:0, see below |
| 9 | Syntax | Error | Text doesn't fit the grammar | The offending token |
| 10 | Command used as name | Error | A command word where a name is required; see [names](#keywords-and-names) | The name |
| 11 | Reserved name | Error | A reserved word or readable setting where a name is required, or an unavailable construct | The word |
| 12 | Unknown name | Error | An expression reads an undeclared name, or calls an unknown function | The name |
| 13 | Duplicate name | Error | Setting, parameter, macro, query, axiom or rule parameter declared twice | The second one |
| 14 | Argument count | Error | Wrong number of arguments to a command, function, macro, `attr` or module | The use |
| 15 | Pose parameter | Error | A pose restriction was broken | The name or operator |
| 16 | Macro cycle | Error | Macro takes part in a cycle | Each macro statement in it |
| 17 | Module in draw rule | Error | Module in a `draw` body, directly or through a macro | The use |
| 18 | Unknown axiom | Error | The build named an axiom the file doesn't have | 1:1 |
| 19 | Segment replaced by several | Warning | A `grow` rule on `F` or `Z` with more than one `F` or `Z` in one alternative, counting copies from `repeat` and `mirror` and the contents of macros. Repeat counts use the grammar's own settings and parameters, not build overrides | The rule |
| 20 | Unreachable rule | Warning | A rule after an unconditional rule on the same left side, in the same block or an earlier block of the same kind, `grow` or `draw` | The rule |
| 21 | Not constant | Error | `rand` in a parameter's value or a repeat count | The `rand` |
| 22 | Size and step | Error | Both are stated | The later one |
| 23 | Non-finite value | Warning | See [non-finite values](#expression-evaluation) | Start of the expression |
| 24 | Not convertible | Error | Converting `.ls` text only: the file holds something this format can't express | The offending text, by line and column in the `.ls` file |
| 25 | Shape parameter | Error | A shape restriction was broken; see [tiers](#parameters-and-tiers) | The name |

Codes 6, 7 and 8 come from drawing, have line and column 0, and instead carry a production position: the index, in the production after rewriting, of the module whose commands include the one that caused it.

## Round trip and serialisation

A grammar parses into an editable model and serialises back to text, in one of two styles: glyphs or names. Parsing, serialising and parsing again gives an equal model, and that model builds the same result. Serialising is stable: a second pass gives the same text.

Kept: statement order, rule order, comments, tiers, growth flags, names, axiom names, `A()`, expression structure, and the choice of `grow` and `draw` blocks.

Not kept: blank lines, indentation, spacing, line breaks inside expressions, repeat and mirror blocks and polygons, and unneeded parentheses.

Canonical form:

- Statements one per line, with rule lines indented by four spaces, ` -> ` around the arrow, and items separated by one space
- `{` on the `grow` or `draw` line, `}` on its own line
- Argument lists with `, `, binary operators with spaces around them, parentheses only where needed
- Numbers in the shortest form that reads back as the same double
- Units always written: `thickness 20` becomes `thickness 20%`, `size 100` becomes `size 100cm`, `minthickness 1` becomes `minthickness 1cm`
- In either style, inside `repeat` or `mirror`, polygons are written `polygon` and `endpolygon`, and a rule on `endpolygon` spells its name
- In either style, a polygon start with no `}` after it in the same body is written `polygon`
- A comment on a line by itself stays before the statement or rule after it, and a same-line comment stays on that line. A comment on a continuation line, or on the line of a block's `{`, moves before the next statement, rule or closing `}`. Trailing blanks in a comment are removed

## Conversion from `.ls`

Legacy `.ls` files of Lparser, by Laurens Lapré, are specified to convert to this format, with header numbers becoming `generations`, `angle` and `thickness`, symbols becoming single-character modules, each rule becoming a `grow` rule, a symbol with no rule getting an empty replacement, growth marking written with `grows` and `nogrowth` where it differs, comments kept, and a generated seed. The result is specified to build the same production and bone tree. A file holding something this format can't express is code 24, and gives no grammar. This conversion is still being built, so this section describes the intent. Apart from code 24, it has not been checked against an implementation.
