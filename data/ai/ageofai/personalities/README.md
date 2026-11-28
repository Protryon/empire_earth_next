# The personality files

This folder holds the words of eeaimod's computer players: their personalities, the system lines the mod shows
you, and what it tells a language model when the AI voice is on. Every file is plain text (UTF-8) with the
extension `.eeai`; this README is not read by the game. The files are read once, when Empire Earth starts - restart
the game after editing. The folder goes into the mod's folder `ageofai` next to Empire Earth.exe (`ageofai\personalities`, beside ageofai.ini); `ai_dir=` in ageofai.ini's `[general]` reads them from
somewhere else instead (for example where you edit it: `ai_dir=_aimod\config\personalities`, relative to the game folder
or absolute).

**Sharing a folder: `folder.link`.** A folder that holds a file `folder.link` (and nothing else) is the folder named
on that file's first line - absolute, or relative to the folder the link is in. Its subfolders are the target's, and
a subfolder may be a link of its own. So Art of Conquest can read Empire Earth's personalities
(`ageofai\personalities\folder.link` with `..\..\..\Empire Earth\ageofai\personalities`), a game the repository's
(`X:\...\_aimod\config\personalities`), or a mod of its own share only `mod\` (`mod\folder.link`). A link that is
wrong - a file beside it, a target that is no folder, more than 8 links in a row (a circle) - is a mistake the game
tells at its start; the log names every linked folder (`# texts: ... is ... (folder.link)`).

| File | What it is |
|---|---|
| `eroberer.eeai`, `haendler.eeai`, ... | One personality each (`register_personality()`): a name, colours, character traits and the lines it says |
| `_template.eeai` | An empty personality with every key, to copy |
| `mod/computer.eeai` | No personality but the base of every one: neutral lines for what must not go unsaid (war, peace, offers, capitulation, victory), the `decide` rules, `this_war`, `many_dead`, `big_tribute`. AIs without a personality speak with it |
| `mod/prompt.eeai` | What the mod tells the language model (`[llm]` in ageofai.ini): who the AI is, the game as it stands, the situations, the tools - for every personality that doesn't say it its own way |
| `mod/system.eeai` | The system lines the mod shows you (white, from nobody in particular) |

A file of this folder is a personality only when it declares `register_personality()`; any other is a library that
personalities include. `mod/` holds the mod's own files: every personality builds on `mod/computer.eeai` and
`mod/prompt.eeai` without naming them (see "Building on other files" below). They are part of the mod like its DLL -
the mod has no texts of its own: a key missing in `mod/system.eeai` or `mod/prompt.eeai` is a mistake, and without
the `mod/` files the mod says nothing (a message at the start): no personalities, no AI voice, no diplomacy of its
own - the computer players play on. A folder without any personality is fine: every AI speaks with
`mod/computer.eeai` then.

## A new personality

Copy `_template.eeai` to a new name in this folder, for example `seefahrer.eeai`, take the `#` from its
`#register_personality()` and fill it in. Every AI of a game
gets a personality at random, each one once before any repeats; on random maps its favoured colour comes first. To
give a player a particular one, set `personality_P2=seefahrer` (the file name without `.eeai`) in ageofai.ini.
`ai_lines=0` in ageofai.ini silences all lines, `ai_names=0` keeps the game's own player and civilization names.

```
register_personality()

personality = {
    name = "Ines",
    title => epoch < epoch.renaissance ? "Captain" : "Admiral",
    civilization => "Free Harbours",
    color = ["lightblue", "cyan"],
    aggression = 40%,
    loyalty = 70%,
    character = "A sea captain: dry humour, proud of her fleet, keeps her word.",
}

// a name of the file's own, for its lines - worked out where it is used (=>)
friend => opinion > 30% ? "old friend" : "captain"

lines = {
    greeting => ["Fair winds, {them}.", "The tide is with us today."],
    war_declared => [
        them.betrayals(me) > 0 ? "Again, {them}? I should have known.",
        "You will regret this, {them}.",
    ],
    peace_accept => [them.kills(me, units) > 100 ? "Fine. I will not forget my dead."] ?? "Peace it is, {friend}.",
}
```

## The format

The same language runs the mod's test scenarios, with commands and statements on top
([docs/scripts.md](../../docs/scripts.md) in the mod's sources) - a personality's file only names values.

A file is made of `name = value` lines, one a line:

- **Records**, `name = { key = value, key => ..., ... }`: a comma after each entry (after the last one too, or
  not), over as many lines as you like, records in records. Those the mod reads hold its keys: a personality has
  `personality`, `lines`, `style`, `priorities`, `decide` and `prompt`; `mod/system.eeai` has `system`. Each key is
  written the way the mod uses it - `=` and `=>` never look alike:
  - `key = value` - **read once**: the personality's `name`, `color`, traits and `character` and its `style`, when
    the AI takes the personality.
  - `key => ...` - **worked out each time it is used**: every line in `lines` (each time it is said), `title` and
    `civilization` (every 10 s), `priorities` (every 10 s), `decide` (at each decision), the texts of `prompt` and
    `system` (each time they are filled). By the key, not the value: `title => "Sir"` too.

  The other one is a mistake when the file is read (`lines.greeting is worked out each time it is used: greeting =>
  ...`).
- **Names outside them** are the file's own (its *vars*): values its lines use, `{friend}` or `friend` - written
  anywhere outside those records, before or after the lines that use them. `name => ...` is worked out where it is
  used (it changes with the game and with who speaks to whom), `name = ...` is data, the same everywhere (see "Names
  of your own" below). A record of your own takes its entries the same way, `colors.dark`, `colors.light` - and
  `colors` the whole record: `colors = { dark = "black", light = "white" }`.
- **`include("pack_base.eeai")`** names files whose lines, names and keys the personality builds on (see below);
  **`register_personality()`** makes the file a personality.

A comment runs from `//` to the end of the line, also after a value (not inside a string) - as in the game's own
scripts (.rmv, .tai), where `#` is the preprocessor's (`#define`, `#include`): a `#` is a mistake. A value is an
**expression**; it may go over several lines while a `(`, `[` or `{` is open:

| Value | Meaning |
|---|---|
| `"text"` | A string. `\"` is a quote, `\\` a backslash, `\n` a line break, `\{` `\}` braces that are text (no placeholder). `'text'` works too. |
| `@"text"` | A verbatim string, as C#'s: no escapes and no placeholders - a `\` and a `{` are text, `""` is a quote. For paths: `@"Data\Scenarios\x.scn"` |
| `85`, `0.5` | A number |
| `["a", "b", "c"]` | A list: its items, nothing else. Said - as a line, in a `{placeholder}` - it is one of them, at random; `pick(l)` takes one in an expression. `len`, `first`, `item`, `contains`, `map_range` and the filters (`count`, `kills`) see all of them. A comma after the last item is fine. |
| `allied ? "Welcome back."` | A condition without `:` - nothing when it doesn't hold |
| `opinion > 30% ? "friend" : "stranger"` | One or the other |
| `a ?? b` | `a`, unless it is nothing: then `b` |
| `lord != null` | Something is there (`lord == null`: nothing) - `null` is nothing, compared only with `==` and `!=` |

**Inside a string**, braces hold a placeholder or an expression: `"Hello, {them}!"`,
`"Hello, {opinion > 30% ? "friend" : "you"}!"`. The quotes inside the braces belong to the expression and don't end
the string.

**Long values over several lines.** A line break counts as a space while a `(`, `[` or `{` is open. A string itself
(and a `{...}` inside it) stays on one line - for a long text, put strings side by side in `( )`: they are one
string. That works for every key, also in `prompt` and `system`, which take strings only:

```
offer_text => ("Peace, {them}? "
    "My walls have held so far - "
    "but I would rather build than fight.")

report => (
    allied ? "We stand together." :
    them.kills(me, units) > 10 ? (
        "You have killed {them.kills(me, units)} of mine. "
        "I have not forgotten."
    ) :
    "We are not friends - not yet."
)
```

Only inside `( )`: in a list, `["Hello" "Hi"]` stays a mistake (a missing comma), not one joined string. A text is
built in its string, never with `+` (which adds numbers): `"Gold: {me.gold}"`, `"{greeting}{farewell}"`.

**A mistake** - a missing quote or bracket, an expression that doesn't parse (also in a string's `{placeholder}`), an
unknown key, situation or record, a line outside `lines = { }` - is written to the log (`ageofai_log.csv` in the folder `ageofai`, lines starting with
`# texts:`) with the file and line, and only that key is skipped. The files are checked when the game starts, every
line, not only those that happen to be said. A bad file never stops the game.

**A name nobody knows** (`them.soldirs`, `{foo}`) is found when the file is read too and logged with its file and
line, but the key is kept. Some mistakes can only show while the game runs: `"a" * 2`, a text and a number added or
compared, a division by 0, a player who isn't there used without a check (`lord.soldiers` without a lord). Nothing
is guessed then. The mistake is logged, and what it is in is left out: an item of a list (`[foo > 3 ? "Ha!",
"Hello."]` says "Hello."), else the whole line, which then falls back like a line with nothing to say. A broken `{placeholder}` in a system text or prompt writes nothing.

**Writing texts?** Set `strict_parsing=1` in ageofai.ini (`[general]`): every mistake then comes up at once - those in
the files when the game starts, in a message box; those that only show while playing (`"a" * 2`, a player used who
isn't there) as a line in the game.

The VS Code extension in the mod's sources (`extensions/eeaimod-texts`) highlights these files and marks unknown keys,
situations and `epoch.` names - and shows the mistakes while you type: it starts `ageofai_check --lsp` (below; the
one in a folder above the file, or the setting `eeai.checker`), which checks each open file a moment after the last
change, unsaved, the way the game will read it - and offers what fits where you type (a record's keys with = or =>
as each is written, after a dot the members of what stands before it by its type - `them.`, `game.players[1].`, a
script's variable of a struct -, the values, functions and names of the file and its layers), with what each is when
you point at it (a situation's description and chance, a member's or name's doc, a variable's type). F12 goes to
where a name is defined - a name of `mod/computer.eeai` used in a personality there, a script's variable to its
`let`, a struct's field to its line - and Shift+F12 lists its uses: a name of `mod/computer.eeai` in every
personality that uses it (and doesn't define its own), a member like `them.soldiers` wherever it is read. Any editor
that speaks the Language Server Protocol can start it the same way.

**Without the game:** `ageofai_check.exe` (next to `Empire Earth.exe`; a download of its own on the release page)
reads the personalities the way the game does at its start and lists every mistake the game would log there, as
`file:line: what`. Started by itself it checks the folder the game reads (`ageofai\personalities`, or `ai_dir`);
`ageofai_check <folder>` another one (a file of it checks its whole folder; a script is checked as a test run reads
it). In VS Code: Terminal > Run Task > "eeai: check this file" puts them into the Problems panel. Unit names
(`unit.b_farm`) are left to the game - only its data knows them - and so is what only shows while playing.

## Lines

Each key of `lines = { ... }` is a **situation**; its value is what the AI says then - usually a string or a list:

```
lines = {
    attack => "My riders are on their way.",
    greeting => ["Hello.", "Greetings, {them}.", "Who are you?"],
}
```

How a line is chosen:

1. The situation happens (the table below). Lines that aren't always said have a **chance** (scaled by the
   personality's `talk`: at talk=100 twice as often as at 50), a **cooldown** per AI, and need 3 seconds since the
   AI's last line.
2. The value is worked out: conditions leave items out, and of a list the line says one of its items, at random. The same text as
   last time is avoided when there is another.
3. **Nothing to say** - no key, `[]`, or a list whose conditions all fail - falls back to a more general situation
   (the fallback column), then to the layers (an included file, `mod/computer.eeai` last) the same way, else the AI
   stays silent.
   A personality must have a line for what must not go unsaid (war, peace, offers, capitulation, victory, defeat),
   its own or an included file's - else it is a mistake, told when the files are read.
4. Only those it is meant for hear it: everyone, the AI's allies, or the player it is said to (`{them}`).
5. With the AI voice on (`[llm]` in ageofai.ini) a language model says the line in the AI's own words, from its
   `character`, its traits and the game as it stands; the chosen line is its example.

**A line about an event doesn't count that event yet**: in `betrayed`, `them.betrayals(me) > 0` means they betrayed
me before - this is the second time; in `offer_withdrawn`, `them.ignored(me)` counts the offers they let run out
before this one; in `tribute_thanks` and `tribute_small`, `them.tribute(me)` is what they paid before this tribute
(`{tribute}` is this one). To say the count with this one: `{them.ignored(me) + 1} offers without an answer`,
`{them.tribute(me) + tribute_total} in all`.

The lines are English while they still change; translations come with a release - a file per language, in a form
still to be designed (a language needs conditions of its own for its grammar - one listener or several, gender - but
the AI's decisions what to say should stay the same in every language).

### Conditions and variety

```
angry = ["Go away.", "You again?"]

lines = {
    // nothing unless the condition holds - a list leaves it out
    greeting => [allied ? "Good to see you, ally.", "Hello, {them}."],

    // one or the other
    winning => opinion > 0 ? "Let us stay friends." : "You are next, {them}.",

    // the angry lines when they hate it, else the friendly ones
    tribute_thanks => [opinion < -50% ? angry] ?? ["Thank you!", "Most generous."],

    // a list in a list: each of its items counts as one (here a third each)
    peace_refuse => ["No.", angry],

    // now and then
    epoch_up => [chance(30%) ? "Progress, finally.", "A new age begins."],

    // one situation, told apart by how I stand with them: someone else has won
    other_victory => lord == them ? "Long live {them}, my lord!" : allied ? "We won together, {them}!" : "Enjoy it while it lasts.",
}
```

### Situations

<!-- situations: generated by scripts/build/gen_eeai_readme.py -->
| Situation | When - "you" is the AI | Heard by | Chance | Cooldown | Without a line |
|---|---|---|---|---|---|
| `greeting` | the game begins - a first word to everyone | everyone | 60% |  |  |
| `war_declared` | {them} has just declared war on you | {them} | always | 20 s |  |
| `betrayed` | {them}, your ally until now, has betrayed you and declared war | {them} | always | 20 s | `war_declared` |
| `peace_accept` | you accept {them}'s offer of peace (you are allied now) | {them} | always |  |  |
| `peace_refuse` | you refuse {them}'s offer of peace | {them} | always | 60 s |  |
| `alliance_offer` | you offer {them} an alliance against {enemy}, your common enemy | {them} | always |  |  |
| `offer_accepted` | {them} has accepted your alliance offer | {them} | always |  |  |
| `offer_withdrawn` | your alliance offer to {them} went unanswered, and you withdraw it | {them} | always |  |  |
| `betray` | you betray {them}, your ally, and declare war on them | {them} | always |  |  |
| `tribute_thanks` | {them} sent you a generous tribute: {tribute} | {them} | always | 20 s |  |
| `tribute_small` | {them} sent you a tribute - a small one: {tribute} | {them} | always | 60 s |  |
| `attack` | your army sets out to attack {them} | its allies | 40% | 300 s |  |
| `under_attack` | your town is under attack | its allies | 60% | 240 s |  |
| `building_lost` | you lost a building | its allies | 25% | 180 s |  |
| `epoch_up` | you have just reached the {epoch} age | everyone | 50% | 60 s |  |
| `enemy_ahead` | an enemy is a whole age ahead of you | its allies | 40% | 600 s |  |
| `winning` | you are winning against {them} | {them} | 40% | 600 s |  |
| `losing` | you are losing the war | its allies | 50% | 600 s |  |
| `desperate` | you are close to defeat | its allies | always |  |  |
| `enemy_defeated` | {them} has been defeated | everyone | 80% | 30 s |  |
| `ally_defeated` | your ally {them} has been defeated | its allies | 80% | 30 s |  |
| `defeated` | you have been defeated | everyone | always |  |  |
| `wonder_threat` | a wonder is threatened - only while the game's computer player runs (game_ai = 1) | everyone | always | 60 s |  |
| `wonder_help` | you send help for an ally's wonder - only while the game's computer player runs (game_ai = 1) | its allies | always | 60 s |  |
| `wonder_nohelp` | you have no help to give for an ally's wonder - only while the game's computer player runs (game_ai = 1) | its allies | always | 60 s |  |
| `help_land` | you send land troops to help an ally - only while the game's computer player runs (game_ai = 1) | its allies | always | 30 s |  |
| `help_air` | you send aircraft to help an ally - only while the game's computer player runs (game_ai = 1) | its allies | always | 30 s |  |
| `help_naval` | you send ships to help an ally - only while the game's computer player runs (game_ai = 1) | its allies | always | 30 s |  |
| `help_none` | you have no troops to send an ally who asked for help - only while the game's computer player runs (game_ai = 1) | its allies | always | 30 s |  |
| `surrender` | you give up the game | everyone | always |  | `defeated` |
| `capitulate` | you capitulate: {them} is beating you, and you offer to PAY them a tribute ({tribute}) for peace - you give, they receive | {them} | always |  |  |
| `capitulate_accepted` | {them} accepted your capitulation: you PAY them the tribute ({tribute}) and serve them from now on | {them} | always |  |  |
| `capitulate_refused` | {them} did not accept your capitulation, and you withdraw it: the war goes on | {them} | always |  |  |
| `attack_threat` | your army is about to attack {them} | {them} | 60% | 300 s |  |
| `all_submitted` | every other player has submitted to {them}, your lord | {them} | always |  |  |
| `other_victory` | {them} has won the game | everyone | always |  |  |
| `block_refuse` | you refuse {them}'s peace offer because {enemy} in their alliance cannot be at peace with your side | {them} | always | 60 s | `peace_refuse` |
| `block_refuse_own` | you refuse {them}'s peace offer because your own ally {enemy} won't make peace with them | {them} | always | 60 s | `block_refuse` |
| `vassal_grumble` | your lord {them} forces you to make peace with {enemy}, whom you hate | {them} | always | 60 s |  |
| `ask_lord` | {them} offers you peace, but you are the vassal of {enemy} and have no diplomacy of your own - they must talk to {enemy} | {them} | always | 60 s | `peace_refuse` |
| `territory_taken` | you have taken a territory from {them}: their buildings there are gone, yours hold it now | {them} | 60% | 180 s |  |
| `territory_lost` | {them} has taken a territory of yours: the buildings that held it for you are gone | {them} | 60% | 180 s |  |
| `territory_held` | {them} attacked a territory of yours and was beaten off: it is still yours | {them} | 50% | 300 s |  |
| `landing` | your army has crossed the water and landed on {them}'s shore | {them} | 70% | 300 s |  |
| `title` | you have just taken a new title: from now on you are called {me} | everyone | always |  |  |
<!-- /situations -->

## Expressions

| | |
|---|---|
| Numbers, texts | `85`, `0.5`, `70%` (the same as `0.7` - `%` only right after a number), `"text"`, `'text'` |
| Arithmetic | `+ - * /` on numbers only. A text is built in its string: `"Gold: {me.gold}"`, `"{a}{b}"` - `"a" + b` is a mistake (b added to the text, or the text to each of b's items?). Dividing by 0 is a mistake too; `me.strength * 150%` |
| Comparisons | `< <= > >= == !=` - numbers when both are numbers, else texts (case ignored). A text against a number (`"12" == 12`) is a mistake: logged, and only `!=` holds. A list compared is a mistake - `contains(l, x)`. Values of different types are told when the file is read, as in scripts (docs/scripts.md "Types"): `them == "x"`, `epoch + 1` (`int(epoch) + 1`, `epoch >= epoch.middle`) |
| Logic | `a && b` (both), `a \|\| b` (either), `!a` (not) - as in C#, TypeScript and C; `!=` is a comparison. The words `and`, `or`, `not` are a mistake, a single `&` or `\|` too |
| Choice | `a ? b : c`, `a ? b` (nothing when `a` doesn't hold), `a ?? b` (`b` when `a` is nothing) |
| Lists | `[a, b, c]` - leaving out what is nothing, a list in a list giving its items. A list holds its items only: a line and a `{placeholder}` say one of them at random, `pick(l)` takes one; where one value belongs otherwise (`+`, `<`, a number's place) it is a mistake. A list with items is true, an empty one nothing |
| Functions | `chance(p)` - true with the probability p, 0 to 1: `chance(25%)` or `chance(0.25)` (`chance(25)` is a mistake); `rnd()` - a random number, 0 up to but not 1 (`rnd() < 25%` is `chance(25%)`); `rnd(n)` - 0 up to n; `min(a, b, ...)`, `max(a, b, ...)`, `abs(a)`, `round(a)`, `floor(a)` (down to a whole number: `floor(me.gold / 4)`), `ceil(a)` (up), `int(a)` (toward 0: `int(-2.5)` is -2; `int(epoch)` its number) - the four give an int, `log2(a)` (of a number above 0: `log2(2)` is 1, `log2(0.5)` is -1 - for ratios: `log2(them.strength / me.strength)`) |
| List functions | `pick(l)` - one of its items at random (`pick([10, 20]) + 5`; a single value is itself); `len(l)` - how many items (a single value 1, nothing 0); `first(l)` - the first item, not a random one (`first([a ? "x", b ? "y", "z"])`: the first that holds); `item(l, i)` - the i-th, from 1 (nothing past the end); `contains(l, x)` - an item equal to x (as `==`); `map_range(x, [limits], [values])` - the value of the first limit x is below, the last value when it is below none: `map_range(ratio, [0.25, 0.67, 1.5, 4], ["much weaker", "weaker", "about as strong", "stronger", "much stronger"])`. A value that is no list counts as a list of one. |
| Text and number | `str(x)` - x as a text only (`str(epoch)` is "Copper Age", `str(epoch) == "Copper Age"`); `number(x)` - x as a number: a value's number, or a text that is one (`number("12") + 1` is 13), else nothing (`number(t) ?? 0`). Rarely needed - for a text that holds a number |
| Vectors | `vec(x, y)`, `vec(x, y, z)`, `vec(x, y, z, w)` - one value of 2 to 4 numbers (a place, a colour - not a list): `home.x`, `.y`, `.z`, `.w` its numbers (of a name holding one, or right after a call or `( )`: `step(home).x`, `(a + b).y`); `+ -` two vectors of as many numbers, `*` a vector and a number, `/` a vector by a number, `== !=` (no `<`); `distance(a, b)`; in a text `(12, 40)`. `rgb(r, g, b)` - a colour, 0 to 255 each as a colour picker shows it: `rgb(128, 64, 0)` is `vec(0.5, 0.25, 0)` (`vec` takes the 0 to 1 itself). Anything else with a vector (`vec(1, 2) + 1`, `abs(v)`) is a mistake |
| Records | `{ a = 1, b = "x", c = { d = 2 } }` - one value of named members, a comma after each: `r.a`, `r.c.d`, also after a call or `( )`; a member it doesn't have is a mistake - `has(r, "a")` checks first. In a text `{a = 1, b = x}`; compared or added it is a mistake (compare its members). A record of the file's own (`colors = { dark = "black", light = "white" }` outside the mod's records) gives the names `colors.dark`, `colors.light` - each worked out where it is used, like any name of the file - and `colors` the whole record |
| Grouping | `( )` |

- **True** is a number other than 0 or a text that isn't empty. **Nothing** is false, an empty text in a line, 0 in
  arithmetic, and a comparison with nothing is false (`war_minutes > 5` and `war_minutes < 5` are both false when
  there is no war).
- **Numbers and texts at once:** some values are both - the epoch is 3 and "Copper Age", an opinion is -62% (-0.62) and
  "hostile" (hostile, unfriendly, indifferent, friendly, trusting: `opinion1`..`opinion5` in `mod/system.eeai`). The
  number counts in arithmetic, in comparisons with a number and as true/false; the text counts in a line and in a
  string (`"Age: {epoch}"`). Arithmetic gives a plain number: `{round(opinion * 100)}` writes -62.
- **A string is always a text**, even when it looks like a number: `"12" + 1` is a mistake, and `"12"` is no colour
  number. Write numbers without quotes.
- Only what is chosen is worked out: the other side of `? :`, `??`, `&&`, `||` is not (no random draws, no
  errors from text that isn't said).
- Mistakes are never guessed around: an unknown name, a text where a number belongs, a division by 0 leave out the
  list item they are in, else the whole line (see "A name nobody knows" above). Nothing runs from the top of a file
  and nothing loops - a name's value is worked out where it is used: a text can't hang the game.

### Names of your own (vars)

A name outside the records holds a value or an expression for the texts, written one of two ways:

- `name => ...` - **worked out where it is used**: anything that changes with the game or with who speaks to whom
  (`me`, `them`, `opinion`, `epoch`, a `{placeholder}`, a function's call).
- `name = ...` - **data**, the same wherever it is used: numbers, strings without placeholders, lists and records of
  them, `unit.<id>` / `kind.<kind>`, other data names, `vec()` and `rgb()`. Anything else written with `=` is a
  mistake when the file is read (`pawn = ... changes with the game (them.strength) - ... pawn => ...`): the two never
  look alike.

A personality's own names come first, then
those of its layers (`mod/prompt.eeai`'s too: `stance`, `power` ...), then those of `mod/system.eeai`. A prompt text
is filled with the names of the file it comes from: `mod/prompt.eeai`'s own `power` in its texts, whatever a
personality calls `power`. A record's key can't be one (`greeting => ...` outside `lines = { }` is a mistake: it
belongs in the record).

```
address => opinion > 30% ? "friend" : opinion < -30% ? "enemy" : "neighbour"
honour => "my lord {them}"
angry = ["Go away.", "You again?"]
```

Then `{address}`, `{title}` in a line, or `angry` in an expression. A name is worked out anew wherever it
is used: `"{angry} {angry}"` picks twice, maybe two different items, and `len(angry)` is 2. A function's parameter
is worked out once, when it is called: given a list, it keeps the list (`len(l)` inside) and the item it picked
(`{l}` is the same item every time).

**Functions:** a name with parameters, written with `=>`, is worked out with the values it is given:

```
losses(minutes) => them.kills(me, minutes, units)
fighting_report(minutes) => "you killed {me.kills(them, minutes, army)} of their soldiers and sank {me.kills(them, minutes, marine)} of their ships; they killed {them.kills(me, minutes, military)} of yours"

lines = {
    under_attack => losses(10) > many_dead / 3 ? "{losses(10)} dead in ten minutes. Enough!",
}
```

- Inside the function its parameters are names like any other; outside it they don't exist. Its other names are the
  game's values and the file's names, as everywhere (a name it uses doesn't see its parameters).
- It is called with exactly as many values as it has parameters (`losses()` or `losses(1, 2)` is a mistake, found
  when the file is read). Up to 8 parameters; none is fine too: `greeting_line() => ...`.
- A parameter may have the name of a value - `minutes`, `them` - and then means the parameter inside its function.
- A function may call others; one that calls itself stops at the nesting limit (a mistake, never a hang).
- Lookup is as for the other names: the personality's own first, then its layers, then `mod/system.eeai`.

### State: what an AI remembers

A personality file is like a class, and every AI that plays it an instance of it. **Fields** - `let` at the file's
top - are each AI's own: two AIs playing the same file count apart. They start as written when the AI first needs
them in a game (a new game starts them afresh), are read anywhere like any name, and change only in **statements**:

```
let betrayals = 0                          // a number (an int: whole numbers; let x: float = 0, let b = false)
let mood = "calm"                          // a text
let grudges = dict<player, int>(0)         // a dict: every player starts at 0 (the default), see below
let traitor: player? = null                // a player, or nobody yet

on_betrayed() {                            // a handler: runs every time the situation happens
    betrayals += 1
    grudges[them] += 1
    mood = "bitter"
    traitor = them
}
remember(p) {                              // a function with statements: called from a handler or another such one
    grudges[p] += 1
}

lines = {
    peace_accept => grudges[them] > 1 ? "Peace - but I remember, {them}." : "Peace, {them}.",
}
```

- A field starts as data: a number, a bool, a text, or `dict<K, V>(default)` - keys `int`, `text` or `player`,
  values `int`, `float`, `bool` or `text`; `d[key]` is the key's entry or the default, `d[key] = value`,
  `d[key] += 1` set it. A field's type stays: a text into a number field, 2.5 into an int field are mistakes, and a
  dict field keeps its table (its entries are set, the field isn't).
- A **player field**, `let name: player? = null`, starts as nobody and takes a player or `null`: `traitor = them`.
  Read it after `!= null` - `traitor != null && traitor.alive`, `traitor == them`; a defeated player stays in it
  (`traitor.alive` false). Only players: a list or another of the game's objects can't be a field yet.
- **Handlers** `on_<situation>() { ... }` - any situation of the table above: `on_betrayed`, `on_greeting` ... - with
  `me`, `them`, `enemy` as its lines have them. One runs every time its situation happens, whether a line is said or
  not (none written, its chance, the talk switched off, the language model's words), and after the line - the line
  sees the state from before. The AI's own file's handler counts, else its layers' first that has one.
- **Statements** - `let`, `if`, `for`, `while`, `x = ...`, `x += ...`, `return` - only inside a function's `{ }`
  (docs/scripts.md has them all). A line or a `=>` name never changes anything: a function with statements is a
  mistake there (lines are worked out more often than said - to choose one, for the model's prompts), so state
  changes only where something happened.
- A name is a field or a name of the file, never both; `let` at the top only.
- **Saved with the game** (the `.ageofai` file beside the save): a loaded game's AIs remember what they did. Matched
  by name - a field you renamed or removed since is left out, a new one starts as written, another type or another
  personality for that AI start afresh (each told in the log) - so editing a personality never breaks a save.
- A whole example: `fuchs.eeai` (Princess Livia) keeps books - the slights against her and the favours by player, the
  territories taken and lost, whom she courts, her nemesis - written by her handlers and read by her lines ("That makes
  {lost[them] + 1}. Every one of them is in my book."), and by her title: five lands taken make Princess Livia an
  Empress.

### Includes: building on other files

Every personality builds on two files of eeaimod's without naming them: `mod/computer.eeai` (the lines that must
not go unsaid, the `decide` rules) and then `mod/prompt.eeai` (what the language model is told). A personality names
the other files it builds on:

```
include("_knights.eeai")
register_personality()

personality = {
    ...
}
```

or several: `include("_knights.eeai", "_castles.eeai")`. The included files are its **layers**, in the order named,
each one's own includes right after it (4 deep, no circles; a file name of this folder, no path), then
`mod/computer.eeai` and `mod/prompt.eeai` - unless a file of the chain names them already, which puts them where it
names them: `include("mod/computer.eeai", "_knights.eeai")` puts the knights after eeaimod's base. Both include
`mod/units.eeai`, the groups the texts count with (`units`, `army`, `buildings` ... - "Unit types" below).

- **Lines:** a situation's line comes from the personality's own chain first (the situation, then its fallbacks),
  then from each layer's chain in turn - the first that has something to say now. An included file's line is worked
  out with its own names first.
- **Names, `decide`, `personality`, `style` and `prompt` keys:** the personality's own win; a key it doesn't write
  comes from the first layer that has it (`aggression` from `_knights.eeai` when the knight doesn't set its own;
  `prompt = { nature = "..." }` in a personality tells the language model its own words, the other prompt texts come
  from `mod/prompt.eeai`).

A file without `register_personality()` is only a library: no AI gets it. Give a pack of your own its own base (a
file of its own name, before `mod/computer.eeai`): packs dropped into this folder then never depend on each other.

## The game's values

A line is said by one AI - **me** - to or about another player - **them**. Some lines also involve a
third player - **enemy** (`{enemy}`): the enemy in an alliance offer, the ally or lord in some lines. **lord** is
my lord - the player I capitulated to - in every line, and nothing while I have none. In `mod/system.eeai` `them` is
you, the local player.

Each of them is a player: in a text its name (`{me}`, `{them}`, `{enemy}`, `{lord}`), in a
comparison itself - `lord == them` holds when the one I talk to is my lord. A player has members - `me.soldiers`,
`them.strength`, `enemy.epoch`, `them.lord` (a player again: `them.lord.name`, `them.lord == me`) - and
**relations**, which name the other player: `me.opinion(them)` is what I think of them, `them.opinion(me)` what they
think of me, `enemy.at_war(them)` whether the enemy is at war with them. A human has no opinion the game could
read: `them.opinion(me)` of a human is a guess from what I did to them - war (a betrayal most), peace, tribute, a
refused offer, their losses - so a line built on it should sound like my impression ("You must hate me by now"),
not a fact.

A player who isn't there - no third player in a greeting, no lord - is nothing. **Check it before you use it:**

```
lines = {
    under_attack => lord ? "My lord {lord} will hear of this!" : "To arms!",
    greeting => [lord && lord.soldiers > 50 ? "My lord's army is mighty, {them}."] ?? "Greetings, {them}.",
    tribute_thanks => "{lord ?? me} thanks you, {them}.",
}
```

A condition (`lord != null ? ...`, `lord ? ...`, `lord && ...`, `!lord`), `??` and a comparison (`lord == them`) check it. Used without
a check - in a text (`{lord}`), a sum, a member (`lord.soldiers`) or a relation (`opinion(lord)`) - it is a
mistake, told when the file is read (the line is skipped): a line that names a lord nobody has makes no sense. A check
holds where it guards - right of `&&`, in the `?` branch, in `if`'s block - and works for a relation's value too
(`me.war_minutes(them) != null ? "{me.war_minutes(them)} minutes of war"`); a name set elsewhere (`long_war =
me.war_minutes(them) > 30`) is no check where it is used. Where the code guarantees a lord - `vassal_ask_lord` and
`diplo_vassal` in `mod/system.eeai` and `terms_vassal` in `mod/prompt.eeai` are only said by a vassal - `{lord}` needs
none.

Shortcuts: my own `epoch`, traits, `allies` and `enemies` without `me.` (`aggression` is `me.aggression`), a relation of mine without
`me.` (`opinion(enemy)` is `me.opinion(enemy)`), and `opinion`, `at_war`, `allied` alone: mine with them
(`me.opinion(them)`). Everything else needs its players: `them.soldiers`, `me.kills(them)`.

<!-- values: generated by scripts/build/gen_eeai_readme.py -->
**Of each player** - `me.`, `them.`, `enemy.`, `lord.`:

| Member | |
|---|---|
| `X.name` | its name - the same as the player itself in a text: {them} = {them.name} (an AI's with its title: "King Brann") |
| `X.title` | an AI's title as its personality's title =&gt; gives it now ("King" - worked out every 10 s) - nothing without one |
| `X.soldiers` | its soldiers (all numbers of units are counted every 10 seconds) |
| `X.citizens` | its citizens |
| `X.population` | its units: soldiers and citizens - to scale a number to the game's stage: population_limit, max_population |
| `X.buildings` | its buildings |
| `X.towers` | its towers |
| `X.walls` | its wall pieces |
| `X.centers` | its Town Centers and Capitols |
| `X.territories` | the land territories it holds: those where it has the most finished buildings that hold ground - a tower, a Town Center, a Capitol, a production building (game/territory.c's rule, counted every 10 s while an AI of the mod's plays: ai/news.c) - me.territories &gt; them.territories * 2 |
| `X.strength` | its fighting strength (its soldiers' hit points, 30 a building) - to compare: them.strength &gt; me.strength * 2 |
| `X.rank` | its place by strength among the players still in, 1 = the strongest |
| `X.epoch` | its epoch: the name in a text, 1 (Prehistoric) .. 15 (Space) as a number - compare it with epoch.middle etc. |
| `X.color` | its colour: the name in a text (in the game's language), 1 (blue) .. 17 as a number - compare it with color.red etc. |
| `X.civilization` | its civilization's name, as the game shows it (Karthago in a German game; an AI's personality may give it another: civilization=) - nothing when none is chosen |
| `X.civilization_en` | its civilization in the game's database, in English in every language (Carthage) - for conditions: them.civilization_en == "Carthage" |
| `X.alive` | 1 while it is still in the game |
| `X.human` | 1 for the human player at this PC |
| `X.vassals` | how many have capitulated to it |
| `X.allies` | how many are allied with it (both ways), still in - allies alone: mine |
| `X.enemies` | how many it treats as enemies, still in - enemies alone: mine |
| `X.aggression`, `X.loyalty`, `X.pride`, `X.greed`, `X.forgiveness`, `X.caution`, `X.talk` | its traits, a share: 0% .. 100% (50% without a personality) - aggression &gt; 60%, {pride:%} in a text |
| `X.food`, `X.wood`, `X.stone`, `X.gold`, `X.iron` | its stock - another's too (the AI's spies): them.gold; a script sets it: p.gold = 500, p.food += 100 |
| `X.stock` | the five resources together - to scale amounts: them.tribute(me) &gt; me.stock / 2 |
| `X.unit_limit` | its unit limit: the game's setting and its own share (a scenario's Pop Cap changes that) - a script sets it: p.unit_limit = 300 |
| `X.map_revealed` | 1 while it sees the whole map, no fog (a scenario's Visibility-On) - a script sets it: p.map_revealed = true |
| `X.lord` | its lord, the player it capitulated to - a player: them.lord.name, lord == them; nothing when it has none |
| `X.count(filter)` | its units and buildings of a filter: unit.&lt;identifier&gt; (one type), kind.&lt;kind&gt; (the game's family, every epoch's: kind.towers, kind.citizen), or a list of them - any of them: them.count(kind.towers), me.count([kind.tank, kind.tank_strong]) (docs/units.md: the identifiers and kinds) |

**Relations** of player A with player B - `me.opinion(them)`, `them.opinion(me)`, `enemy.at_war(them)`; without `A.` mine (`opinion(enemy)`). Those that count events - `tribute`, `refusals`, `betrayals`, `kills` - take a time window as a second argument: `them.tribute(me, 15)` counts the last 15 minutes only (`16.5` too), without it (or with 0) the whole game. Older events are the whole game minus the window - `them.betrayals(me) - them.betrayals(me, 20)` counts those more than 20 minutes ago, `me.kills(them, 60, buildings) - me.kills(them, 20, buildings)` those 20 to 60 minutes ago; a negative window is a mistake:

| Relation | |
|---|---|
| `A.opinion(B)` | -100..100: what A thinks of B - in a text in words: hostile, unfriendly, indifferent, friendly, trusting (round(opinion): the number). An AI's opinion; a human's of an AI is a guess from what the AI did to them (war, a betrayal, peace, tribute, a refusal, their losses), nothing of another human |
| `A.at_war(B)` | 1 when A and B are enemies, both of them |
| `A.allied(B)` | 1 when A and B are allied, both of them |
| `A.hostile(B)` | 1 when A treats B as an enemy (whatever B does) |
| `A.war_minutes(B)` | how long the war of A and B has lasted: from both enemies until both are allied again (a peace offer doesn't end it; in peace_accept: the war just ended) - nothing without a war |
| `A.alliance_minutes(B)` | how long A and B have been allied - nothing when they are not |
| `A.betrayals(B)` | how often A turned on B while they were allied (an alliance of 2 minutes or more: a peace broken at once is war, not betrayal) - (B, minutes): in the last minutes; in betrayed the betrayals before the one it is about |
| `A.refusals(B)` | how often A refused B's offers of peace or alliance - (B, minutes): in the last minutes |
| `A.ignored(B)` | B's alliance offers A left unanswered until B withdrew them, in a row - in offer_withdrawn those before this one |
| `A.tribute(B)` | resources A has paid B in this game - (B, minutes): in the last minutes, them.tribute(me, 15); in tribute_thanks / tribute_small what was paid before this tribute |
| `A.near(B)` | A's soldiers within 20 tiles of B's base now, while B treats A as an enemy (an AI's base) |
| `A.in_town(B)` | 1 when A's army is in B's town: its soldiers have stayed near B's base for a while (an AI's base) |
| `A.attack_minutes(B)` | minutes since A's last attack set out against B - nothing if none (an AI's: its pushes, the game's attack waves) |
| `A.kills(B)` | B's units and buildings A has destroyed - (B, minutes): in the last minutes (whole minutes, the current one counts); a filter last picks which, as count's: unit.&lt;identifier&gt;, kind.&lt;kind&gt;, a list of them - or one of mod/units.eeai's lists (units, army, airforce, marine, military, civilians, buildings): them.kills(me, units), me.kills(them, 10, kind.tank), them.kills(me, 20, buildings) |

**The game:**

| Name | |
|---|---|
| `me` | the AI that speaks - its name in a text; me.soldiers ... |
| `them` | the player it speaks to or about; in a system line you, the local player |
| `enemy` | a third player: the enemy in an alliance offer, the ally or lord in some lines |
| `lord` | my lord (me.lord), the player I capitulated to - in every line; nothing when I have none |
| `opinion` | me.opinion(them): what I think of them |
| `at_war` | me.at_war(them): 1 when we are enemies, both of us |
| `allied` | me.allied(them): 1 when we are allied, both of us |
| `minutes` | the game time in minutes |
| `players` | the players still in, me among them |
| `population_limit` | the game's unit limit per player (its setting) |
| `max_population` | the largest population of a player still in - kills &gt; max_population / 2: many for this stage of the game |
| `language` | the game's language: de, en, fr, ... |
| `llm_equal` | 1 when the language model is told nothing of who plays the computer ([llm] llm_equal) |
| `llm_constrained` | 1 when the language model decides within the rules' limits ([llm] llm_constrained) |
<!-- /values -->

### Epochs

`epoch` is the epoch's name in a line - in the game's language - and its number 1..15 in a comparison. Compare it
with these names, the same in every language (a name in quotes would only match in one):

<!-- epochs: generated by scripts/build/gen_eeai_readme.py -->
| Name | Number | Age |
|---|---|---|
| `epoch.prehistoric` | 1 | Prehistoric |
| `epoch.stone` | 2 | Stone |
| `epoch.copper` | 3 | Copper |
| `epoch.bronze` | 4 | Bronze |
| `epoch.dark` | 5 | Dark |
| `epoch.middle` | 6 | Middle |
| `epoch.renaissance` | 7 | Renaissance |
| `epoch.imperial` | 8 | Imperial |
| `epoch.industrial` | 9 | Industrial |
| `epoch.atomic_wwi` | 10 | Atomic - WWI |
| `epoch.atomic_wwii` | 11 | Atomic - WWII |
| `epoch.atomic_modern` | 12 | Atomic - Modern |
| `epoch.digital` | 13 | Digital |
| `epoch.nano` | 14 | Nano |
| `epoch.space` | 15 | Space |
<!-- /epochs -->

```
    epoch_up => [epoch >= epoch.middle ? "Steel and stone!", "Another age, {them}."]
```

A player's colour works the same way: `them.color` is its name in a line (in the game's language - "dunkelrot" in a
German game) and a number in a comparison. Compare it with `color.blue`, `color.red`, `color.yellow`,
`color.magenta`, `color.orange`, `color.green`, `color.cyan`, `color.darkred`, `color.darkgrey`, `color.purple`,
`color.grey`, `color.silver`, `color.lightyellow`, `color.lime`, `color.lightblue`, `color.pink`, `color.darkgrey2`.
`them.civilization` is the name of its civilization as the game shows it - a text only, in the game's language.

```
    greeting => them.color == color.red ? "Red banners, {them}? Mine are redder." : "Greetings, {them} of {them.civilization}."
```

### Unit types

Which units and buildings a player has - `X.count(filter)`, counted every 10 seconds: the game's names, the same in
every language (the mod's sources, docs/units.md, list them all):

- `unit.<identifier>` - one type: `unit.b_guard_tower_bronze`, `unit.s02_fishing_boat_stone`;
- `kind.<kind>` - every type of one of the game's families, whatever its epoch: `kind.towers`, `kind.citizen`,
  `kind.tank`, `kind.ship` (warships and fishing boats alike: the families are the game's armour classes);
- a list of them - any of them, lists in lists too: `me.count([kind.citizen, unit.s02_fishing_boat_stone])`.

```
fishing_boats = [unit.s02_fishing_boat_stone, unit.s04_fishing_boat_bronze, unit.s08_fishing_boat_imperial,
                 unit.s11_fishing_boat_modern, unit.s13_digital_fishing_boat]
lines = {
    winning => them.count(kind.towers) == 0 ? "Not a single tower left, {them}." : [],
}
```

The types are the game's own, read from the data it has loaded - a data mod's new types and families count too
(docs/units.md lists the retail game's). A misspelt kind is a mistake when the file is read, a misspelt identifier
once the game has loaded its data (at the start of a game); anything else as a filter (a number, a text) when the
line is worked out.

What `kills` counts - its last argument, after the minutes if any: the same filters, `them.kills(me, 10, kind.tank)`,
`me.kills(them, kind.towers)` - counted by type for the whole game and any window, however long. Without one it
counts everything destroyed, units and buildings alike. The groups the mod's own texts count with are lists of these
in `mod/units.eeai`, a layer of every personality (a name of your own of the same name comes first):

| Name | Counts |
|---|---|
| `units` | every unit: military and civilians |
| `army` | on land - infantry, cavalry, siege, tanks, mechs, priests, heroes |
| `airforce` | aircraft, helicopters, balloons, the flying mechs |
| `marine` | warships, submarines, transports, carriers |
| `military` | army, airforce and marine |
| `civilians` | citizens and fishing boats |
| `buildings` | every building - towers and walls too |

```
sea_war => them.kills(me, 10, marine) > 3 ? "Leave my ships alone, {them}!"
lines = {
    peace_accept => them.kills(me, this_war, buildings) > 5 ? "{them.kills(me, this_war, buildings)} of my buildings in ruins. Peace.",
}
```

## Placeholders

In a line, `{...}` holds any expression: `{them}`, `{me}`, `{enemy}`, `{epoch}`, `{opinion}`, a name of the file,
`{them.soldiers}`, `{round(me.strength / 100)}`. Personality lines also have two placeholders of their own:
`{tribute}` - what a capitulation pays or a tribute brought (`capitulate...`, `tribute_thanks`, `tribute_small`), by
resource ("200 gold, 800 iron") - and `{tribute_total}`, the same in all, a number (1000).

A number is written as it is, with up to 2 decimals: `{0.625}` writes 0.63 (a half rounds away from 0). How else, a
format after a `:` says - the last thing in the braces:

| | 0.625 | 1234.5 |
|---|---|---|
| `{x}` | 0.63 | 1234.5 |
| `{x:%}` - as a share | 63% | |
| `{x:.1%}` - a share with 1 decimal | 62.5% | |
| `{x:.0}`, `{x:.2}` - with that many decimals (0 to 6) | 1, 0.63 | 1235, 1234.50 |

`50%` in an expression is only another way to write 0.5 - a value never remembers how it was written: `{aggression}`
writes 0.6, `{aggression:%}` 60%. A choice in front of a format goes in parentheses - `{(opinion > 0 ? opinion : 0):%}`
- since `{opinion > 0 ? opinion : 0:%}` reads as if only the 0 were formatted (the same for `??`); without them it is
a mistake. A format on a text or a player, or one that is no format, is a mistake too.

`mod/system.eeai` and the prompt texts use the same expressions - `{me}` is the AI, `{them}` the one it talks to (in
`mod/system.eeai` you), `{them.epoch}`, `{me.attack_minutes(them)}` - and add a few texts only the mod has: `{stance}`,
`{terms}`, `{tribute}`, `{ally}` ... Each key's are listed above it in the file. None of them has the name of a
value: `{epoch}` is always the speaker's epoch, wherever it stands.

## personality and style

`personality = { ... }`:

| Key | Value |
|---|---|
| `name` | The player's name in the game - who it is: `"Temur"`, or a list in order of preference - the first no other player has: `["Temur", "Batu"]`; all taken (more AIs than personalities), the first with a number: "Temur II", "III" ... An expression worked out once, when the AI takes the personality, like `color` (constants, `chance()`, `pick()`, `language`, the file's names): `pick(["Gunther", "Roland"])`. A loaded game finds the personality again by the name - any text its `name` holds, also with its number, after a title or not |
| `title` | A title before the name: "Khan" makes "Khan Temur". An expression of the game, worked out again every 10 s - written `title => ...`: `epoch < epoch.dark ? "Chieftain" : epoch < epoch.industrial ? "King" : "General"`, `me.territories`, the file's fields (`conquests >= 5 ? "Great Khan" : "Khan"`). A new title is the situation `title` (not the first one, at the start); nothing or `""`: no title. Its draws (`pick()`, `chance()`) are the same each time it is worked out: a title does not flicker. Title and name at most 40 characters together; `X.title` is the title alone |
| `civilization` | The name of its civilization as the game shows it, instead of the one it plays (that one stays in `X.civilization_en`) - an expression of the game like `title`, worked out again every 10 s: `conquests >= 5 ? "Golden Empire" : "Golden Horde"`; nothing or `""`: the name stays as it shows |
| `color` | Favoured colours on random maps, the first free one wins: `["blue", "cyan"]` - blue, red, yellow, magenta, orange, green, cyan, darkred, darkgrey, purple, grey, silver, lightyellow, lime, lightblue, pink (or the German names, or a number 1..17 as `them.color` has it: `color.red` or `2`, not `"2"`), or a colour of its own: `rgb(128, 64, 0)` (0 to 255 each) or `vec(0.5, 0.25, 0)` - `[rgb(128, 64, 0), "orange"]`: its brown, orange when another AI has it. The game draws such a colour like the table's everywhere and keeps it in a save; `them.color` of a player with one is nothing (no name, no number). An expression like any other, worked out when the game starts - before there are players, so only constants, `chance()`, `rnd()`, `language` and the file's names: `chance(50%) ? ["red", "orange"] : ["grey"]`, `language == "de" ? color.yellow : color.blue` |
| `aggression` | 0% to 100% - written with the %: `aggression = 60%` (50% when left out): betrays, refuses peace |
| `loyalty` | keeps its alliances; a vassal under 50% rebels against its lord once it has served 10 minutes and is clearly stronger than its lord's side (1 + 2 x (loyalty + 100% - pride) times: loyalty 35% and pride 80% 2.1 times) - with or without the language model |
| `pride` | how long it holds a grudge and refuses peace after a declaration of war (pride x 5 minutes: 60% 3 minutes), how long it fights on before it capitulates, how soon it rebels as a vassal |
| `greed` | what a tribute is worth to it |
| `forgiveness` | how fast a grudge fades |
| `caution` | capitulates earlier, pays more |
| `talk` | how often it says the lines that are not always said |
| `air_force`, `navy`, `early_anti_air`, `pressure`, `defences`, `spread` | Its leanings, 0% to 100% like the traits (50% when left out), read with `strategy_plans` in ageofai.ini: what it puts into each - the air force, the warships, the anti-air it builds before any aircraft is seen (0%: only in answer to them), the army's share of the income (high: early pressure, low: the economy first), the towers and walls along its borders (sooner, with less stone in the bank), the claims of empty territories (high: wide, low: compact) - and how likely it picks a plan that puts more into it. Every AI picks a plan at the start (balanced, air power, sea power, guarded, pressure, economy first, fortress, expansion) by its leanings and by chance, and changes it when the fighting shows its plan brings back too little; `hitzkopf.eeai`: `pressure = 85%`, `early_anti_air = 20%` |
| `character` | For the AI voice: who this is, in a few words - the model plays it |

`style = { ... }` is **experimental** and only read with `ai_style=1` in ageofai.ini (off by default: every AI plays the same,
tested way). Its keys are ageofai.ini settings that replace the difficulty's values for this AI - those the mod's own
AI plays by: the army's share of the income and the epochs' times. Each value is an expression worked out once when
the game starts, like `color` (constants, `chance()`, `rnd()`, `language`, names): shares with `%` -
`military_income_share = 35%`, as ageofai.ini writes it - minutes as numbers: `epoch_min = 45`,
`epoch_max = chance(50%) ? 60 : 75`. A value out of its range (`military_income_share = 35` would be 3500%) is a
mistake: that key keeps the difficulty's value. The attack times and chances were style keys until 2026-11-22: they
set the game's own computer player's values, which nothing reads while it is off (`game_ai = 0`, the default) - the
mod's pushes set out when they are judged to win, not by a clock.

<!-- style: generated by scripts/build/gen_eeai_readme.py -->
`military_start_share`, `military_income_share`, `military_income_share_farms`, `epoch_min`, `epoch_max`
<!-- /style -->

`priorities = { ... }` is **experimental** too (`ai_style=1`): what this AI saves for, worked out every 10 seconds with the
game's values, as in `decide`. A building's identifier with how many it wants (docs/units.md) is an entry: the entries in the
file's order, one at a time, its cost held back from soldiers and from the other buildings that aren't food, houses, towns or
docks, and the game asked to build it. One the game doesn't offer yet is passed over; one that makes no progress for 6 minutes
lets the next have its turn. A row of the list with a condition saves only while the condition holds. A file with no such
record plays the list as it stands, and a file's own record replaces the one of a file it builds on.

```
priorities = {
    b_stable => 2,
    b_archery_range => epoch >= epoch.copper ? 2 : 0,
    navy => me.soldiers > 50,
}
```

The rows:

<!-- priorities: generated by scripts/build/gen_eeai_readme.py -->
`air`, `laid`, `research`, `epoch`, `farms`, `wants`, `landing`, `outpost`, `fortress`, `forest`, `upgrades`, `navy`, `special`
<!-- /priorities -->

## decide: what the AI wants

The AI's diplomacy decides by numbers worked out from expressions - the same ones as in its lines, with `me` the AI,
`them` the other player and `enemy` its strongest enemy (there may be none: check it, `enemy && ...`). eeaimod's
are in `mod/computer.eeai`; a personality that writes one of them in its own `decide = { }` decides that its own way:

| Key | When | The AI ... |
|---|---|---|
| `peace_score` | someone offers peace, or an alliance is weighed (by either side) | accepts when it is above 0 (opinion is -100%..100%: a score on that scale) |
| `seek_ally` | every minute from minute 10, at war with a strongest enemy | looks for an ally against it when true |
| `ally_score` | for each of that enemy's other enemies (`them`) | asks the one with the highest score, if above -20% |

```
decide = {
    // a fox: an alliance as soon as anyone gets ahead - and with the strongest when it can
    seek_ally => enemy && enemy.strength > me.strength,
    ally_score => them.rank == 1 ? 100% : opinion,
}
```

What stays the mod's: who can be asked at all (not someone whose army is in its town, not twice in a row too soon),
how long an offer stands, how it is answered - and no peace within pride x 5 minutes of a declaration of war. A
mistake in one (or a value that is no number) is logged once and the next layer's decides instead - eeaimod's in
`mod/computer.eeai` last; when one of those fails the answer is no (no peace, no ally). `chance()` and random choices here draw from the game's own generator, so every PC of a
multiplayer game decides alike. Every PC must have the same files then.

## mod/system.eeai and mod/prompt.eeai

`mod/system.eeai` holds the lines the mod shows you itself, `system = { ... }`, in English. `mod/prompt.eeai`
holds everything the language model is told, `prompt = { ... }`, in English (the model is told which language to
answer in); its pieces are joined with a space. Both files have every key the mod asks for, with its placeholders
listed above it. The names in `mod/system.eeai` serve the system lines and
every personality's lines. `mod/prompt.eeai` is the last layer of every personality: a personality's own `prompt = {
... }` (or a layer's before it) replaces single texts for its AI - `nature = "You speak in short, proud sentences."` -
and its names serve the prompts and, after their own, the personalities' lines.

`mod/prompt.eeai` comes with names of its own: the pieces of `game_state_other`, what the AI is told about the one it
talks to - `stance`, `power` (from `strength_ratio`), `town`, `attacks`, `fighting` and `fighting_recent` (with the
functions `destroyed(by, of, minutes)` and `fought(minutes)`, `recent_minutes = 20`) and `bond` (lord or vassal).
Change one by writing it anew in the file; take one out of `game_state_other` by leaving its `{name}` out. A
function's parameter can be a player: `destroyed(me, them, 0)` uses `by.kills(of, ...)`.

## Trying things out

The control pipe (for developers, see `docs/testing.md` in the sources) has `texts eval P Q <expression>` - what an
expression gives with player P speaking to Q in the running game - and `texts line P <situation> [Q]` - the line P
would say now.
