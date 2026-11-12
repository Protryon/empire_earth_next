# Strategy research for the Lua AIs

Research notes for the people building this port's computer opponents (`boom` and `ageofai`). Sources:
- community strategy for Empire Earth (EE) and The Art of Conquest (AoC);
- general RTS strategy for human players;
- how strong scripted and research RTS AIs are built.

The last section turns the findings into recommendations for the Lua AIs. Written 2026-11-10. It is web research plus
a little arithmetic; nothing here was run in the game.

**How well each claim is supported.** Each claim carries a tag:

| Tag | Meaning |
|---|---|
| **[many]** | several independent sources agree |
| **[one]** | a single guide or forum post |
| **[data]** | read from this repository's converted game data (`build/open_data/tree/Data/db`) or from `docs/AI_RESEARCH.md` |
| **[inference]** | my own reasoning or arithmetic, not a source |

The EE community material is old (2001-2004) and thin. Most of it is forum threads and two or three written guides;
the online servers closed in November 2008 [many]. Expect fewer hard numbers for EE than for AoE2 or StarCraft.

---

## Executive summary

**1. The published EE timings are for the cheap variant.** The best written competitive guide (RS_Gollum and
NW_Kroucrouch, Small 1v1) puts the Stone Age at about **7:30** for a fast opening and **13-15 minutes** after a
Prehistoric citizen rush. It also gives the third epoch by 22-23 minutes. Those numbers assume the **Tournament** variant:
- the first epoch costs **170 food** and the second **350 food + 200 iron + 200 gold** [one, matches data];
- the variant's epoch-cost factor is 1.0, the first epoch's factor 0.2 and the second's 0.5 [data].

This port's benches play **Standard**: Stone **1,122 food**, Copper **1,584 food + 844 gold + 844 iron** [data, many]. That is
6.6x and about 3.5x the Tournament costs, so the community's clock times cannot be used as targets directly.

**2. For Standard on a Small map, a defensible human-level target is the Stone Age at about 16-20 minutes and the
Copper Age at about 38-45 minutes** [inference]. These come from a simple food model (§4.1), checked against the
project's own measured rates (about 5-6 food a minute per gatherer, walking included). For comparison:
- the classic AI reaches the Stone Age at 26 minutes and the Copper Age at 63 [data];
- the Lua AIs reach the Stone Age at 30-48 minutes.

The model shows a flat optimum. Training citizens up to about 25-30 before saving for the Stone Age costs only
1-2 minutes of Stone timing, and it buys a much larger economy for the Copper Age. This supports the owner's "all
food into citizens, then advance". The model also points at the biggest single brake in `ageofai`: saving for both
the Stone and the Copper Age costs before the Stone Age (upstream's own notes call it the most harmful feature).

**3. Early-raid defence in EE rests on towers, not on an early army** [many]. Before the Copper Age, the Stone-Age tower:
- does 50 damage a hit and has 1,000 HP for 175 stone;
- can be hurt by nothing except the Sampson siege unit [one, a generated damage table].

Standard Low starts with **630 stone**, enough for three towers [data]. The community answer to a Prehistoric rush has
five parts [many]:
- towers at the worked resources;
- citizens that retreat under the towers, or fight raiders with the citizen-attack upgrade when they outnumber them;
- no army before the Stone Age;
- an army in the Stone Age;
- keep the economy growing, because the raider is paying for the raid with his own economy.

This matches the owner's notes closely.

**4. Constant worker production and no idle producers** are the most-repeated piece of RTS advice in every game
surveyed [many]. AI economies without cheats fall behind mainly through small idle gaps, not through bad strategy
[one, an AoE4 AI modder].

**5. Strong scripted AIs share one shape** [many]:
- a headquarters or strategy manager over economy, production, building, defence and attack managers;
- a priority queue for spending;
- opening books, with build-order search for the opening;
- a combat estimate that decides "fight or retreat";
- for the best bots, learning which opening works against which opponent.

They lose to humans in the same ways [many]:
- units dive into towers;
- raids hit buildings instead of workers;
- the AI cannot adapt across repeated games;
- workers are thrown into fights.

**6. Fair difficulty is built by scaling the AI's own targets** (population, worker count, how many attacks, how
early) **and its reaction speed, not by hidden resource gifts** [many]. 0 A.D.'s Petra is a well-documented model
(§3.7). Players forgive visible, explained help and resent hidden help (Soren Johnson) [one, widely cited].

**Top recommendations** (detail in §4):
1. Never let the Capitol idle in the Prehistoric. Use a food-first split, keep at most 6 on a site, and put a
   settlement at the site first. Save only for the *next* epoch, from about 25-30 citizens. Target: the Stone Age by 20
   minutes on Standard Small.
2. Spend the starting stone on 2-3 towers covering the food and wood sites in the first minutes. Make "flee under a
   tower" the citizens' default answer to a raid. A militia fights only with clear local superiority.
3. In the Stone Age, keep a home guard of about 15-20 at home under the towers. Mix spearmen with slingers or
   clubmen against the raid seen; spearmen alone lose to slingers. Then go back to citizens and the Copper Age.
4. Attack on timings with a combat estimate, never on a trickle. Raid the gatherers and route around towers. Retreat
   when the estimate turns.
5. Bench what humans measure: idle-producer seconds, citizen counts at 5/10/15/20 minutes, food float, citizens lost
   per raid, sites with more than 6 workers, and epoch times. Add "exploiter" opponents (a scripted Prehistoric rush,
   a tower rush) to the bench, as AlphaStar's league did.

---

## 1. Empire Earth strategy, competitive and community level

### 1.1 The settings divide: Tournament against Standard

The game variant decides almost everything about early timing. From this repository's converted tables [data]:

| | Tournament | Standard |
|---|---|---|
| Epoch cost factor | 1.0 | 2.64 |
| First / second epoch factor | 0.2 / 0.5 | 0.5 / 0.8 |
| Stone Age cost | 170 food | 1,122 food |
| Copper Age cost | 350 F + 200 I + 200 G (the guide) | 1,584 F + 844 G + 844 I |
| Work-time factor | 1.0 | 1.66 (what it scales needs a check) |
| Towers | tower cost factor -0.5 | normal cost |
| Starting resources | Tournament Low: 200 F, 175 W, 210 S | Standard Low: 500 F, 400 W, 630 S; High adds 100 gold and iron |

These match what the community wrote:
- *Random Map Guide*: Tournament Pre->Stone 170 food; Standard 1,122 food, then 1,584 food + 844 iron + 844 gold [one];
- a 2002 thread on beating the Medium AI: "You need 1122 food to get to epoch 2" [one];
- Steam's store text: "Standard mode offers a more strategic empire-building game, while Tournament mode offers faster-paced
  RTS action" [one];
- a 2001 review notes that epoch costs depend on the variant and the number of players [one].

Competitive players mostly played Tournament, often in a single epoch (start epoch = end epoch). The Save-EE wiki's
common single-age setup is "Game Variant: Tournament; Starting Epoch = Ending Epoch; Game Speed: Very Fast; Reveal
Map: On". The "league settings" used in official tournaments started in a random epoch and ran to Nano [one, fan wiki,
via search snippet; the page itself refused the connection]. The Random Map Guide recommends:
- game speed above normal ("Very Fast" is 2.5x);
- a unit limit above 400 (it suggests 1,200);
- wonder victory off [one].

A GameSpot preview of AoC said the expansion's lobby would let players sort games by epochs, player counts and rules,
and named the medieval epochs among the popular ones [one].

**Consequence for this project** [inference]: the community's openings are *shapes* to borrow, not clock times. A
Tournament build reaches the Stone Age on about 4 foragers' worth of food. On Standard the same opening is a 15-20
minute economy game, where the owner's "boom first" view is the natural one.

### 1.2 Openings and timings

**RS_Gollum and NW_Kroucrouch, "Empire Earth Guide"** (HeavenGames). This is the most concrete competitive guide
found. It is written for Small 1v1 maps, mostly with Austria, and its age numbers count epochs (Age 2 = Stone) [one]:

| Rule | Detail |
|---|---|
| Scouting | train a dog before any citizen |
| Production | train citizens without a break from every Town Center and settlement-forum; never leave a citizen idle (TAB finds them) |
| Stock | keep it low; spend income quickly |
| Defence | "I would highly recommend a tower to defend yourself during the first 15 minutes" |

The openings it names:

| Opening | Settlements and gatherers | Timing |
|---|---|---|
| Very fast rush | no forage settlement; one settlement right next to the mine; 3-5 on wood, 3-6 on the mine, citizens trained without a break | |
| Rush with a good economy | two settlements, one on food and one on the mine | |
| Boom on forage | economic lead early; the army starts later than with the two-settlement rush | |
| Boom on mines | two settlements first, so it starts 1-2 minutes after a forage boom; it gains the town centre's +25% harvesting bonus | |
| Age-1 pikeman (spearman) rush | barracks; upgrade speed and attack, start slingers | Age 2 at about **7:30**; Age 3 by **22-23 min** at the latest |
| Peon rush | all citizens to the enemy's forage at 6:30-7:30, a tower built beside it | Age 2 at about **13-15 min** |
| Boom with slingers | houses as defence | needs 375 food for Age 3 (Tournament) |

The guide's settings are Tournament (its house defence bonus quotes "40% (tournament) or 60% (standard)") and normal
or slow speed. Its multiplayer is 1v1 on Small maps.

**"Random Map Guide" by introspection** (HeavenGames, 2002), Tournament Low [one]:
- With the map revealed, send 3/5 of the citizens to the nearest forage and 2/5 to the nearest wood. Queue five
  citizens and rally them to the forage, then to the wood. Continue to 6 foragers and 4 woodcutters, then put new
  citizens on iron or gold.
- With the map hidden, send the starting citizens exploring while you pick a civ.
- Build a barracks, range or stable as soon as the wood allows. One builder then builds a tower. Attack with 3-5 units.
- Advance only "just after a failed enemy attack", with 1.5x the cost in hand (Standard) or 2.5x (Tournament).
- Upgrade settlements to town centres so more than one citizen trains at a time. Claim and guard as many mines as
  you can. Farm when the forage runs out.

**PC Gamer walkthrough** (2002), mostly campaign advice but with economy rules [one]:
- 10-15 citizens on food is a good start; then keep 6 on food and the rest on wood;
- start the metals with at least 2 citizens a site, up to 6;
- "up to six citizens per plot of vegetables, stone, gold, or iron";
- if 1,000+ food or wood is stockpiled, move most of its gatherers elsewhere.

**Large Islands map guide** (Empire Earth Fandom wiki, via search snippet) [one]: five citizens to the nearest stone mine
and a settlement there; the Copper Age after about 10-12 minutes (almost certainly Tournament); then granaries, 20
spearmen, a hero and 20 horsemen. It calls the Copper Age "the pivotal age ... to get a veritable Boom going".

**Thread on beating the Medium AI** (2002, Standard costs) [one]:
- 6 citizens on each tower-guarded resource, with at least 2 towers per resource, preferably 3;
- upgrade tower range early;
- about 20 slingers, 2 barracks and a couple of ranges in epoch 2;
- 50-60 citizens in all.

**What this means for openings** [inference]:
- *Settlement first, at a resource*: supported by the Gollum guide (the very fast rush puts its settlement by the mine;
  the booms put settlements on food and the mine) and by the tutorial, which builds a settlement by the forage to
  shorten the walk. It matches the owner's first move.
- *Gatherers per site*: 6 per site is consistent across guides [many]. No source suggests more.
- *Dog first*: one guide [one]; cheap insurance against a rush. On revealed maps it matters less.
- *Food first*: universal, because citizens and the first epoch are food [many].

### 1.3 Rushes and how they are stopped

EE rushes come in three kinds [many]:

| Rush | What it is |
|---|---|
| Citizen and tower rush | the attacker's citizens build a tower next to the defender's forage or mine, sometimes with rock throwers or clubmen to distract |
| Early infantry raid | clubmen or spearmen from an early barracks |
| Copper-Age horsemen | Copper-Age cavalry against gatherers |

How each source handles a rush:

| Source | Answer |
|---|---|
| "Stopping a rush" thread (EE Heaven, 2002) [one each] | Ferrari_Warrior: no army before the Stone Age; a fast Stone Age from hunting, forage and wood; a few towers and houses; research the citizens' 30% attack upgrade ("the 30% attack cit bonus saved my arse"); "every age but pre you better have an army" |
| | Storm_Shadow: 2-3 early towers and about 6 combat units |
| | LKS_Phalanx: 30-40 early combat units stop a rush but "waste epoching" |
| | Ferrari_Warrior again: killing 10 clubmen with citizens leaves the defender ahead, since the rusher gave up economy for them |
| "Prehistoric rush techniques" thread [one each] | Clay: towers before the Stone Age "can pack a punch into units"; move the citizens under the town centre's towers, come back when the attackers leave; build a barracks out of the attacker's sight; an economy-first player usually beats a balanced one in the early ages |
| | Twins of DOOM: a tower, then a barracks, then more towers, and attack once the barracks produces |
| | the original poster: the tower-by-the-forage rush "rarely seems destructive enough" |
| Copper horsemen thread (2001) [one each] | macemen 2-3 to 1; javelin throwers; your own cavalry outnumbering theirs; spearmen to deny routes rather than to chase; guard-stance groups at resource sites plus towers; towers and houses (houses slow raiders killing citizens); walls disputed ("the very best" against "walls only help you turtle") |
| Sinful_666, multiplayer guide (2002-03) [one] | Rob Worrillow's "killing fields": funnel raiders into walls and artillery; let the rush split, then counter-harass; do not counter-rush |

**Stone-Age unit facts behind these answers** (HeavenGames unit table, Standard values not stated) [one]:

| Unit | Cost | Train time | HP | Attack |
|---|---|---|---|---|
| Citizen | 50 F | 28 s | 65 | 5 |
| Dog | 50 F | 28 s | 60 | none |
| Clubman | 30 F + 30 I | 32 s | 135 | 8 shock |
| Spearman | 30 F + 30 I | 32 s | 85 | 15 pierce, shock armour 2 |
| Rock thrower | 30 F + 30 I | 37 s | 75 | 5 shock, range 2 |
| Slinger | 30 W + 30 G | 38 s | 70 | 9 arrow, range 3 |
| Sampson | 65 W + 65 G | | | 60 siege, buildings only |

Note that the **slinger needs no food**. In the food-bound Prehistoric and Stone Ages, every spearman or clubman
competes with citizens and the epoch for food. A slinger instead draws wood and gold [inference].

### 1.4 Booming, settlements and populating

- **Populating.** Citizens placed inside a settlement, town centre, Capitol or granary raise that building's resource
  output. Five citizens turn a settlement into a town centre, and 15 turn a town centre into a Capitol [one, fan wiki
  via search snippet]. The Gollum guide's "+25% harvesting bonus" for a mine boom is this effect [one].
  PC Gamer prices the conversion at 100 wood plus 250 food "in citizens", and advises it only for far settlements or
  for defence [one].
- **More producers.** Upgrading settlements to town centres lets more than one citizen train at once [one]. Every
  guide insists on citizens "from every TC" [many].
- **Booms in later epochs** come from town centres at the mines plus farms (granaries, from the Copper Age) [many].
- Sinful_666's boom layout (later epochs, Standard, one source, partly untested by its author):
  - at least 5 citizens a site, then 6;
  - 12-24 on wood (never more than 35);
  - 8 per farm.

### 1.5 Towers and walls

- **Towers are the early game's backbone.** A generated damage table for the Stone Age's land units (byrdie/empire_earth,
  computed from the game's data) shows [one, derived data]:
  - the primitive tower does 50 a hit to every Stone-Age infantry type and has 1,000 HP for 175 stone;
  - Sampson is the only Stone-Age land unit that can damage it.
- **Players agree** [many]: a tower in the first 15 minutes (Gollum); 2-3 towers per worked resource (Medium AI thread);
  towers plus houses against horsemen; and the tl.net recollection "towers were really strong".
- **Placement.** Towers go at the resources being worked, staggered so that each covers another (Medium AI thread).
  Upgrade the tower's range early [one].
- **Houses.** They wall and add defence: +10% resistance in a town centre's zone, up to 40% (Tournament) or 60%
  (Standard). They cost 50 wood each [one].
- **Walls.** Use them in narrow passages, never as a full enclosure (Gollum) [one]. Players dispute walls in the early
  game. Against AIs they work very well (Medium AI thread: "walls mainly help against the AI") [one].
- **Forward towers.** Gollum: a tower beside two enemy gold mines can block them. Support towers on attack are common
  from the modern epochs but useful earlier [one].

### 1.6 Unit counters

EE has a rock-paper-scissors system by damage type: shock, pierce, arrow, gun, and later laser and missile [many].

**The early triangle** (GameFAQs unit FAQ, Pikeman wiki page, readthedocs damage table) [many]:

```
pierce infantry (spearman, pikeman)  beats  shock infantry (clubman, maceman, swordsman) and cavalry
arrow units (slinger, bowman, archers) beat pierce infantry
shock infantry                        beats  arrow units
pierce cavalry                        beats  shock infantry and shock cavalry; loses to pierce infantry
towers                                beat   all early infantry; only siege (Sampson, later rams/catapults) answers them
```

Stone-Age numbers from the generated table [one, derived data]:
- the spearman does 22 a hit to a clubman, which does 6 back;
- the slinger has a tactical edge of 1.21 over the spearman;
- the clubman has 2.54 over the slinger.

**Later counters from the Gollum guide** [one]:
- archers counter elephants; elephants wreck infantry, and two kill a tower;
- cataphracts beat cavalry archers;
- machine guns counter infantry, grenade launchers tanks;
- after the Industrial epoch "only tanks and planes can kill cavalry".

**Upgrades** (Gollum) [one]:
- ranged units: range, attack, HP, range, attack; skip armour;
- melee units: one cheap armour level; speed only if it gives +2 or more.

### 1.7 Civ builder and civ powers

**The civ builder.** Points buy bonuses. Advice [one each]:
- Boom civs: +15% population cap, and +15-20% to stone, gold and iron gathering (Sinful_666).
- Early epochs: cheaper and faster common units; range for ranged units, attack for swordsmen, armour for melee;
  skip the archer bonuses.
- Rush civs: forage or hunting, citizen cost, the unit's own cost and build time.
- Building and tower cost cuts: one guide's tower drops from 175 to 148 stone. A citizen cost cut lets 200 food buy
  five citizens instead of four (Random Map Guide).

**Premade civs.** The Gollum guide picks a premade civ per epoch: Austria early, Assyria for cavalry archers, Byzantine
Rome, Great Britain for tower range, Novaya Russia late. The Random Map Guide says to carry four civs, one per era
band [one].

**Civ powers are AoC-only** [many: Wikipedia, the manual's index]:
- each civilization has one unique power, often limited to certain epochs;
- Japan and Korea are new;
- the Space Age (epoch 15) is new;
- robots replace citizens in the Nano and Space Ages.

This port's `civpowers.yaml` lists powers such as flaming arrows, slavery, priest tower, crusaders, exploration (sight
pulses from town centres), advanced mining (the mining limit) and camouflage [data]. **No community source found
rates the AoC powers for competitive play**; that evidence is missing.

Two powers matter for an early-game AI [inference]:
- `advanced_mining`: it changes the mining limit, i.e. the 6-per-site rule;
- `exploration`: free scouting.

### 1.8 Sea and island play

- Docks open in the Stone Age with fishing, transport and war rafts. Fishing adds food beyond the forage [one, fan wiki].
- PC Gamer [one]:
  - six fishing boats where fish allow, which frees citizens;
  - at least two warships to guard them;
  - galleys beat battleships, battleships beat frigates, frigates beat galleys;
  - catapult ships are the best wall breakers.
- Large Islands: settle the nearest stone mine first, then reach the Copper Age [one].
- Sinful_666's later-epoch fleet mixes and island assaults are one author's untested ideas [one]. Two points from it:
  artillery on the coast (it outranges towers), and prophets spread round the base against grouped fleets.

Transferable from AoE island play [many, other games]:
- the dock is a second town centre for food;
- losing the fishing fleet is the most common early collapse;
- don't send transports before you hold the water.

### 1.9 Heroes, priests and prophets

- **Heroes** come from the Capitol from the Copper Age on [one, fan wiki]:
  - the *strategist* heals nearby units and can demoralise enemies;
  - the *warrior* raises nearby morale and hits hard.
  Sinful_666 prefers a strategist with the attack group, and puts the hero with one of the main groups [one].
- **Priests** convert. PC Gamer: keep two priests to convert lone enemies; priests can reach units on high ground [one].
  Mass priests were used against horsemen in a campaign [one]. AoC's "priest tower" power converts from towers [data].
  A forum thread complains about priest towers [one].
- **Prophets** cast calamities (plague, earthquake and others). They are slow when not upgraded and easy to kill, and
  ground inside a temple's area is immune [one]. A few escorted prophets can break walls and devastate a town [one].
  Sinful_666 [one]:
  - prophets cost 200 gold and food;
  - earthquakes on the temples first, then siege;
  - 5-10 prophets with an escort of 10-30.
- **AI relevance** [inference]: in Small-map games that are decided by the Copper or Bronze Age, heroes come in only at
  the Copper Age, and prophets and priests are late. They matter less than the early economy for this project's
  current problem.

### 1.10 What is specific to Art of Conquest

AoC adds [many]:
- civ powers;
- Japan and Korea;
- the Space Age, robot workers and new heroes;
- internet play for up to eight.

The community strategy found (EE Heaven guides, Sinful_666, PC Gamer) is base-game material from 2001-2003. None of
its early-game advice depends on AoC, and the Prehistoric-to-Copper rules are the same in both [inference].

---

## 2. General RTS strategy for humans

### 2.1 Economy macro: constant worker production

Every game's guides say the same thing [many]:

| Game | Advice |
|---|---|
| AoE2 | "a villager every ~25 seconds for the first 8-10 minutes is the foundation of every strategy"; "A 30-second idle Town Center = 1 missed villager"; food under 50 idles the TC |
| AoE4 | "idling Town Center is always a mistake, no matter how short"; the pause "is almost always food underflow"; the fix is "more food villagers earlier" |
| StarCraft | worker production never stops until saturation |

Other habits the guides list:
- **Rally points and idle-worker hotkeys** [many]. New workers go straight to a resource. An AoE4 AI modder traced the
  built-in AI's economic lag to exactly this: it "does not use rally points", so each new villager sits idle for a
  moment, and "those precious few moments adds up a lot". In his words, "without cheats AI will always have lesser
  economy than the players due to the way AI works" [one].
- **Never be supply- or house-blocked** [many]. Build the next house before the cap is reached.
- **Benchmarks.** One AoE4 guide: 21 villagers at 5 minutes, 36 at 10 and 66 at 20 from constant production [one,
  unverified mirror]. AoE2: about 120-140 villagers in a 1v1, with 60-80 of the 200 population kept for the army [one].

### 2.2 The logic of build orders

A build order is a schedule that keeps every producer busy and every resource used, so that the next goal
(the age-up, the first army) arrives at the same minute every game [many]. Its parts:
- **Food first for workers.** AoE2's 22-population Feudal opening puts 6 on sheep, 4 on wood, 1 luring the boar, 3 on
  berries, 5 on the second boar and 4 more on wood, and reaches the Feudal Age at about 10:40 [one]. The general
  pattern: food sources by speed (hunt and boar > sheep > berries > farms), then wood, then gold only as the next goal
  needs it [many].
- **A fixed population for the age-up.** The age-up is clicked at a fixed worker count (AoE2: 21-22 pop for Feudal;
  more for a fast Castle). Workers keep training during the age-up [many].
- **Gold only when needed.** AoE2 puts just 2 villagers on gold by the Feudal click [one]. Resources for a later goal
  are not gathered before the earlier goal.
- **Saturation.** In StarCraft II the first 16 workers on 8 mineral patches (2 per patch) mine at full efficiency.
  From the 17th, returns fall, and a third worker per patch adds little: tests put the practical cap at about 20-24
  [many, forum analyses]. Two bases of 8 workers each out-mine one base of 16. That is the case for spreading
  gatherers across sites up to the per-site room, and expanding instead of crowding [inference].
- **Practice tolerance.** "If your Feudal click time is off by more than 30 seconds, restart and find the missed step"
  [one]. Good human play is *repeatable* to within half a minute.

### 2.3 Floating resources

Unspent stock is wasted economy. "Spend your money" is the first macro rule:
- Gollum's EE guide: keep the stock low [one];
- PC Gamer: if 1,000+ food or wood piles up, move most of its gatherers [one];
- the common AoE and StarCraft line: a bank is a sign of missing producers or a missing plan [many].

The one exception is a deliberate save for a known purchase, such as an age-up or an expansion [inference].

### 2.4 Scouting and information

Scouting decides what to build:
- the EE guides: a dog first; "scouting should decide which units to produce" [one each];
- UAlbertaBot's scout finds the enemy base, harasses its closest worker and retreats when that worker fights back [one];
- StarCraft bots fingerprint the opponent's timings (pool timing, expansion timing) to recognise strategies [one].

On EE's revealed maps (a common setting) map information is free. *Army* information still needs line of sight
[inference].

### 2.5 Boom, rush, turtle, and timing attacks

**The classic triangle** [many: Wikipedia's "Turtling" and "Rush" articles, genre essays]:
- rush beats boom: the boomer has no army yet;
- boom beats turtle: the turtle pays for defence that is never used;
- turtle beats rush: the rush breaks on static defence.

Players dispute it per game. An AoE4 thread argues "rush > tech > turtle/boom > rush" and that "a naked boom" is not
possible there [one].

**EE's version** [inference]:
- EE's strong early towers make the turtle-against-rush corner unusually strong. The Prehistoric "rush" is mostly a
  tower rush. That is why the experienced EE posters say an economy-first player beats a balanced one early (Clay,
  Ferrari_Warrior).
- "Rush beats boom" applies when the boom has no towers.

**Timing attacks** hit at a moment of known strength against weakness [many]:
- just after your own age-up or upgrade;
- just before the enemy's age-up lands, when he has spent on the age and not on units;
- when the enemy army is away.

The Random Map Guide's EE version: advance "just after a failed enemy attack", because then the enemy cannot punish the
spending [one].

**Harass versus all-in.** Raids aim at workers, not buildings [many]:
- Random Map Guide: "killing three or four food gatherers is a major economic blow"; intercept citizens walking back to
  the drop site; avoid the towers by going round the back; attack in frequent small waves; use attack-move;
- Sinful_666 [one]: the goal of a rush is to weaken the economy, not to destroy the base. His sword-cavalry rush (Brioli)
  attacks at 8:30 on the game clock with 4-6 cavalry: 2 against soldiers, 4 against citizens, preferring food, then gold,
  iron, wood.

### 2.6 Army composition and counters

**Composition** [many]:
- counter what you scout (EE: "check the enemy civ first, then build units that counter its main strength");
- mix units of the same speed for raids (Gollum);
- group by type for the main fight.

**Split** [one each]:
- Gollum's army layout: about 80% main force plus two raiding groups of about 10%;
- Sinful_666's later-epoch assault: three mixed groups of about 35 plus siege groups, with a held-back main army and
  the smaller groups as bait to draw defenders out.

### 2.7 Micro that matters

The list below is ranked by how much it is worth to a player who is not a top micro player [inference from many
sources]:
1. **Pull workers out of a raid, and back in once it ends.** In EE: under the town centre's or the towers' fire
   (Clay, Medium AI thread).
2. **Fight under your own towers.** "Fight near towers rather than in open areas" (Medium AI thread).
3. **Focus fire, and target workers when raiding** (Random Map Guide, Sinful_666).
4. **Attack-move** so units engage what they pass (Ctrl+right-click in EE) [many].
5. **Retreat damaged or outnumbered groups** before they die. Heal them: strategist heroes, hospitals [many].
6. **Kite** (ranged units against melee). It matters at high level, less so for an AI that is still failing its
   economy.

### 2.8 Multitasking priorities

Human attention is scarce, so strong players order it [many, inference]:
1. keep producers working;
2. answer threats to workers;
3. move the army;
4. keep the build-order spending on track;
5. scout.

An AI's attention is not scarce, but its **decision cadence** is the equivalent. In this project the Lua micro step
has an instruction budget, and one second of reaction to a raid is good. The `ageofai` safety module's one raider scan
a second and its citizens in sixteenth slices, each every 4 s, follow the same ordering [data].

---

## 3. RTS AI design for computer opponents

### 3.1 Architecture: managers, queues and blackboards

| AI | Shape |
|---|---|
| **UAlbertaBot** (StarCraft, AIIDE 2013 champion) [many] | `GameCommander` updates the worker, production, building, combat, scouting and information managers. A `StrategyManager` supplies the opening book and the build-order goal; BOSS plans the build order. Only `CombatCommander` talks to squads, "following military command structures". Shared state (the information manager, map tools) is a global blackboard. |
| **0 A.D.'s Petra** [many] | A *headquarters* (economy, bases, building) and a *queue manager* that directs resources to prioritised queues. The queue manager lets a lower-priority house use surplus wood while an outpost waits. An *attack manager* holds rush, raid, default and huge attack plans; each plan "picks a target ... a path ... and feeds the queue manager with what it needs to train". A *defence manager* groups enemy units near its buildings into armies and assigns counter-units. |
| **AoE2 scripts** (`.per`) [many] | Rule-based: if-then rules over facts (unit counts, resources, ages) plus about 300 "strategic numbers" that tune the engine's built-in economy and military managers. The community AIs (Barbarian by a former top-10 player, Promi and others) are rule sets of this kind. Their strength comes from tuned build orders and fixed engine defaults. |

The defaults that AoE2 scripters change first (airef.github.io) [one]:
- **drop distances**: by default villagers walk across the map, so set them to 20-30 tiles early;
- the number of builders at once: the default of 2 is too low;
- **idle-unit reassignment delay**: from 15 s to 1 s;
- no hunting of boar by default;
- **attack-group sizes** that scale with time until attacks stop.

These are a ready list of "default AI economy mistakes" [inference].

**The shape they share** [many]:
- a strategic layer that sets goals (the epoch, the army size, the next attack);
- a spending layer that turns goals into a prioritised queue;
- specialist managers: workers, buildings, defence, attack;
- a squad or unit layer.

This port's design (Lua strategist, C++ engine helpers) is that shape. AI_RESEARCH §3 reached the same conclusion.

### 3.2 Build-order search and opening books

- **BOSS** (UAlbertaBot) [many]: given a goal set of units and buildings, it runs a branch-and-bound search for a
  near-optimal order, "assuming no interference from the enemy". It re-plans when things change. It solves the
  *opening*, not the decision of *what* to build [one].
- **Opening books.** Fixed scripted openings are the norm in the bots and in AoE2 scripts [many]. Bots leave the book
  when scouting shows a threat. Steamhammer's author lists "deciding when to leave the opening book" as an open
  problem [one].
- **For EE** [inference]: the Prehistoric and Stone Ages have only a few decisions:
  - how many citizens before saving;
  - where the settlements and towers go;
  - when to stop for the epoch.

  A small hand-written book per map type plus one parameter (the citizen count to save at) is enough. A search, or a
  simulation like the model in §4.1, can tune that parameter.

### 3.3 Strategy selection, adaptation and opponent modelling

- **PurpleWave** (StarCraft; 1st at CIG 2019, 2nd at AIIDE 2017 and 2019) [one each]:
  - it picks strategies from a graph of openings, mid-game transitions and late-game compositions;
  - the choice depends on past results against the same opponent, race, map and number of start positions;
  - it fingerprints the enemy's strategy (pool timing, expansion timing, tech) and stores it with its own choices.
- **Steamhammer**: its game records keep "the events of the game, not only the outcome". The aim is to "see an
  opponent's strategy in the first game and counter it in the second". With no single best counter, it plans a
  game-theoretic mix of openings [one].
- **tscmoo** (AIIDE 2015 winner): many hand-coded strategies, chosen per opponent by online learning [one].
- **Inside one game**, adaptation means the defence manager sizing a response to the enemy army seen (Petra), and
  counter-unit choice from scouting [many].
- **For this project** [inference]: learning across games is out of scope for lockstep play and saves. Inside one game
  the useful parts are cheap:
  - fingerprint the opponent's early behaviour (a Prehistoric raid seen? a tower near my food? cavalry in the Copper
    Age?);
  - switch between the boom, balanced and rush scripts' parameters to answer it.

### 3.4 Economy decisions

- **Worker targets.** Petra sets a target number of workers by difficulty and population cap [one, the source code]:
  - `targetNumWorkers` = min(120, maxPop/3) from Medium up, min(60, maxPop/2) at Easy, at most 40 below Easy;
  - a `supportRatio` (non-combat share) of 0.35, rising to 0.4-0.5 at lower levels;
  - phase-up population targets of 38 / 65 / 80, scaled by difficulty.
- **Saturation.**
  - UAlbertaBot: 3 workers per gas refinery, idle workers to minerals, workers reassigned when a patch runs out [one].
  - StarCraft human analysis: 2 per patch optimal, 3 maximal [many].
  - EE: 6 per site [many].
  - Grille's Age of AI notes: the game itself puts 6 gatherers on a mine [one].
- **Drop distance.** The AoE2 scripters' first fix is to cap how far a villager may walk to a resource: 20-30 tiles
  early [one]. In EE terms: a site without a nearby settlement is not worked; build the settlement first.
- **Expansion timing.** Human and bot practice is to expand when the current sites are saturated and the army can
  hold the new site [many]. In EE an expansion is a settlement at a site; on Small maps the sites are 10-18 tiles out
  [data].
- **The epoch reserve.** Grille's own measurements are the most directly relevant evidence anywhere [one, measured]:
  - the shipped `copper_plan` (hold the cost of both early epochs from 40 citizens) was "the most harmful feature";
  - `copper_plan=next_epoch` (hold only the next epoch's cost) gave more workers early (+5 to +13 at 20-30 minutes);
  - `food_plan=3` gave +8.7 workers at 30 minutes and the epoch 7.7 minutes earlier;
  - "four soldiers in epoch 0" was the worst variant;
  - early towers, tower bait and careful militia helped most against the cheating base AI.

  This project's 8.38 diagnosis found the same brake: the Stone Age at 38 minutes instead of 24, because gold and iron
  for the Copper Age were gathered before the Stone Age's food [data].

### 3.5 Army control

- **Fight or retreat from a combat estimate.** Each UAlbertaBot squad runs a combat simulation (SparCraft): it keeps
  attacking on a predicted win and retreats on a predicted loss [many]. Simpler bots use a strength ratio with
  hysteresis, for example attack at 1.3x and retreat below 0.8x [inference; common practice]. Petra computes a
  per-unit "strength" for its choices [one].
- **Target selection** [many]:
  - workers and units that can shoot back before buildings;
  - when raiding, gatherers that are far from defences;
  - never the wall segments once inside (an AoE2 weakness, §3.6).

  UAlbertaBot's attack order: a known visible enemy base, then visible enemy units, then remembered buildings, then
  exploration [one].
- **Influence and threat maps.** Dave Mark's "Modular Tactical Influence Maps" (Game AI Pro 2, ch. 30) [one, widely used]:
  - layered grids of proximity and threat per side, combined into working maps;
  - used "to determine the relative threat an agent is under" and "safe locations to evade to or withdraw to";
  - the map "only provides information"; the decision layer acts on it.

  A threat grid makes these questions cheap:
  - which sites are safe to gather;
  - where a raid route avoids towers;
  - where citizens flee.
- **Threat-aware pathing.** Raiders route around known towers ("go around the back", Random Map Guide). Grille's AI
  "attacks as one army round the defences it has seen" [one each].
- **Groups.** Petra launches attacks only with population headroom and limits how many run at once:
  min(2, 1 + maxPop/100) [one, the source code]. It skips rushes against an enemy with more than 6 defensive structures
  [one, the source code]. **That is a good rule to copy for EE's tower-heavy early game.**

### 3.6 Common AI weaknesses humans exploit

**Human against bot results** [many]:
- In 2015 the top three StarCraft bots played Djem5 in exhibition and did not win a game. He ran past the bot's
  sunken colonies into its main base and killed the workers, where "most bots would have attacked the defenses".
- The organisers: once humans "have played one or two games against the bots they are then easily able to detect and
  exploit small mistakes", and bots "do not do well in best-of-n".
- Jay Scott (Steamhammer) [one]:
  - bots are "indecisive";
  - they cannot stay out of unnecessary fire;
  - one passing enemy distracts them;
  - they send workers to fight and die;
  - "Bots in 2015 are only able to react sensibly to expected situations".

**AoE2 DE Extreme AI weaknesses** reported by players [one thread, plus several confirmations]:
- units dive into castles and towers and die to arrows;
- rushing units attack houses and buildings for minutes instead of villagers;
- after breaching a wall the AI attacks other wall segments instead of entering the base.

Other threads report that the AI throws many villagers at a forward tower. Players differ on whether walls and towers
are "exploits" or intended play [one each].

**This project's AIs already show some of these** [data]:
- citizens on a camped patch;
- the engine's militia reflex sending citizens to their deaths (8.38);
- raid answers that pull the force back and forth (80-135 a game).

### 3.7 How difficulty is made fair

- **Scale the AI's own targets and pace, not its income.** Petra's levels [one, the source code]:
  - Sandbox, Very Easy, Easy: `popScaling` 0.5 / 0.7, which shrinks the phase-up and barracks populations;
  - worker targets of 40 / 60 / 120;
  - below Easy, the city phase is never reached;
  - 0-1 sentry towers below Medium, 1 at Medium, 2 at Hard and above;
  - rush counts tied to an "aggressive" personality value (up to 3 rushes of 16 / 20 / 24 units).
- **AoE2 DE.** The forum consensus is that the DE AI gets no resource bonus, while the old HD and Conquerors AIs did.
  The higher levels keep some information advantage, and one player reports the AI developer saying Extreme was made
  *worse* at micro to be less annoying [one each, forum posts, not official].
- **Transparency.** Soren Johnson: "With cheating, perception becomes reality, so transparency is the antidote to
  suspicion and distrust". Players accept small, steady, declared bonuses and hate visible discontinuities (free units
  under fog, instant wonders). "When the question is one of fairness, the player is always right" [one, widely cited].
- **Mechanical limits.** AlphaStar's Nature-version agent (2019) had latency, a camera view and an APM cap with peaks
  "substantially lower than those of humans", after the January 2019 demo drew criticism for superhuman control. It
  still reached Grandmaster, above 99.8% of ranked players [many].
- **For this project** [inference]:
  - difficulty = the epoch-pacing parameter, the citizen target, the home-guard size, the attack timings, the reaction
    delay to raids, and whether the AI uses towers well;
  - keep any remaining help declared in `gameoptions.yaml`, as it already is.

  This also fits the owner's goals: Medium is an even match, Hard usually wins, Easy is a handicapped Medium.

### 3.8 What makes an AI feel human

[inference, from many of the sources above]:
- **Plans that can be read.** One army that gathers, marches and attacks. Not a constant trickle, and not a defence
  that flickers back and forth.
- **Visible reaction.** The AI notices a raid, pulls its citizens back and answers it, and does not leave citizens on
  a camped patch.
- **Mistakes of the human kind.** A slightly late epoch is human-like; walking into a tower is not.
- **No superhuman timing**: no instant reaction across the whole map, no perfect focus fire on every unit. A research
  paper on human-like StarCraft bots names millisecond action timing as the giveaway [one, abstract].
- **Variety.** Personalities that change the opening, not just the numbers (Petra's aggressive, defensive and
  cooperative traits; this project's boom, balanced and rush).

### 3.9 AlphaStar's lessons for a scripted AI

- **League training.** *Exploiter* agents train only to beat the main agent and find its weaknesses: "playing to win
  is insufficient" [many]. In scripted terms: **bench each AI against small, narrow exploit scripts** (a Prehistoric
  tower rush, a clubman raid at 6 minutes, a horse raid on gatherers in the Copper Age), not only against the classic AI.
- **Non-transitivity.** No single strategy beats all others, so a pool of opponents is needed to measure progress
  [many]. A head-to-head win rate against one opponent can mislead.
- **Interface limits** are what make results credible to human players (§3.7).

---

## 4. Synthesis and recommendations for the Lua AIs

### 4.1 Target epoch timings for Standard, Small

No community source gives Standard timings, so this is a model [inference].

**The model:**
- start with 5 citizens and 500 food (Standard Low);
- one producer training a 50-food citizen every 28 s (or 46 s if Standard's work-time factor 1.66 applies to
  training, which needs a check);
- 80% of the citizens on food at r food a minute each, the project's measured 5-6 a minute walking included, up to
  about 8 with settlements beside the sites;
- train to N citizens, then save 1,122 food for the Stone Age.

| Food rate per gatherer | Best Stone time (citizens) | Stone at 30 citizens |
|---|---|---|
| 6 / min, 28 s training | 17.2 min (18) | 18.2 min |
| 8 / min, 28 s training | 13.4 min (16) | 15.0 min |
| 6 / min, 46 s training | 18.4 min (15) | 21.3 min |
| 8 / min, 46 s training | 14.4 min (13) | 19.2 min |

What the model shows:
- **The optimum is flat.** Going from about 16-18 to about 30 citizens costs 1-3 minutes of Stone timing, and it nearly
  doubles the economy for the Copper Age, which costs 3,272 resources.
- **This backs the owner's "all food into citizens, then advance".** It also says "then" can be as late as about 30
  citizens without much loss.
- A second producer (a settlement upgraded to a town centre) moves the optimum higher. The model only has one.

**The Copper Age:**
- 1,584 food + 844 gold + 844 iron;
- 35-40 gatherers at about 5 a minute bring in about 175-200 a minute;
- that is about 17-19 minutes of saving after the Stone Age, before any spending on the army.

**Targets for the bench** [inference, to be checked against the classic AI's numbers in the sim]:
- the Stone Age at 16-20 minutes;
- the Copper Age at 38-45 minutes;
- 25-30 citizens at the Stone Age; 45-55 at the Copper Age.

For comparison [data]:
- the classic AI: the Stone Age at 26, the Copper Age at 63, and 175 citizens at the end;
- the c10 checkpoint: the Stone Age at 24 from 34 citizens.

The c10 checkpoint shows the Stone target is reachable with this engine's rates. The binding constraints are behaviour,
not the economy model: the epoch reserve, the wrong resources gathered, and raid losses.

### 4.2 Prioritised recommendations

**1. Train citizens without a break in the Prehistoric, and save only for the next epoch.** [strong: many sources,
plus the model and Grille's and this project's measurements]
- Keep the Capitol (and every producer) training whenever food ≥ 50. Measure idle-producer seconds.
- Before the Stone Age, hold only the Stone cost (1,122 food), from about 25-30 citizens. Never hold the Copper cost
  then. In `ageofai` that means `copper_plan=next_epoch` or an equivalent; upstream measured the default as its worst
  brake.
- Before the Stone Age, put gold and iron gatherers only on what towers and a token army need. In the human build
  logic, resources for a later goal are not gathered before the earlier goal (AoE2: 2 on gold at the Feudal click).
- Spend any food float above about 150 that the plan does not hold on citizens.

**2. Use towers as the Prehistoric defence, and keep the army out of it.** [strong: many EE sources, data, Grille's
results]
- Spend the starting stone (630 on Standard Low) on 2-3 towers early:
  - one at the main food site, one at the wood or the first settlement;
  - staggered so that they cover each other;
  - range upgrade early if affordable.
- The citizens' raid answer, in this order:
  1. stop gathering where a raider hits;
  2. flee to the nearest tower or town building;
  3. come back when the threat map clears.
  The citizen militia charges only with clear local superiority (upstream: 3x the enemy's strength, 3 citizens per
  enemy, at most 6 enemies) and under a tower when possible.
- No soldiers in the Prehistoric, unless an enemy tower is being built beside a worked site. Then about 4-6 citizens
  pull to stop the build, the community's answer to the citizen-tower rush. Grille: four soldiers in epoch 0 was the
  worst variant.
- Research the citizens' attack upgrade if it is available early. One strong player credited it with holding a 2v2
  rush [one].

**3. In the Stone Age, a home guard of about 15-20 under the towers, mixed by the raid seen, then back to
citizens.** [medium: the owner, the Medium AI thread (about 20 slingers), the Large Islands guide (20 spearmen)]
- Spearmen beat clubmen and cavalry. **Slingers beat spearmen**, and clubmen beat slingers.
  - Default: about two-thirds spearmen and one-third slingers.
  - Against a slinger or rock-thrower raid, add clubmen.
  - Against horsemen in the Copper Age, spearmen, plus macemen or javelin throwers.
  This is a change to the owner's "~20 spearmen" (see §4.7).
- Slingers cost wood and gold, not food, so they do not compete with citizens or the Copper Age's food [inference].
- The guard waits at home: at the worked sites under the towers, on guard stance. It does not chase past the tower
  line. It answers a raid only when the combat estimate favours it.
- Once the guard is at strength, every producer goes back to citizens. The economy then saves for the Copper Age.

**4. Attack on timings, with a combat estimate, and raid workers.** [medium: many RTS sources, the AI design
literature]
- Default attack windows:
  - **just after an epoch advance**, with the new units;
  - **after a failed enemy attack**, the Random Map Guide's moment;
  - when the scouted enemy army is away or much smaller.
- Don't attack into more than about 6 towers without siege (Petra's rule; EE's tower strength).
- Raids target gatherers far from towers and route round known towers. Retreat when the estimate turns, with
  hysteresis (for example go at 1.3x, leave below 0.8x). Never stop to hit walls or houses while citizens are in reach.
- Attack with one army and frequent reinforcement waves. Don't trickle single units.

**5. Gatherer allocation rules.** [strong: many sources]
- At most 6 per forage, mine or hill site, or the engine's room if a civ power or bonus raises it. Spread across sites
  before crowding one.
- A site is worked only with a drop site (settlement, town centre or Capitol) within a few tiles. Build the settlement
  first, the owner's first move and Gollum's openings. Cap walking distance as the AoE2 scripters do.
- Prehistoric split: food first (the guides' 3/5 forage, 2/5 wood at the start; 6 on food then the rest on wood, from
  PC Gamer), wood for the settlements and houses, stone already in hand for towers.
- Stone Age: food-heavy; enough iron for the guard; gold or wood for slingers.
- From the Stone Age, open gold and iron for the Copper Age.
- Reassign a gatherer within seconds when a site empties or becomes dangerous. AoE2's default 15 s idle limit is the
  classic mistake.
- Populate town centres at the main mines when there are spare citizens (+25% from populating, one guide), and
  upgrade a settlement to a town centre for a second citizen producer.

**6. Read the opponent in-game and switch scripts.** [medium: the bot literature, inference]
- Cheap fingerprints:
  - an enemy soldier or citizen near my sites before minute 10;
  - an enemy tower within N tiles of a worked site;
  - the enemy's epoch;
  - the enemy army's family mix.
- Use them to move between the boom, balanced and rush parameters, and to pick the home guard's counter-mix.

**7. Make difficulty a set of declared handicaps.** [medium: Petra, Soren Johnson, the owner's goals]
- Scale:
  - the citizen target;
  - the epoch-saving threshold;
  - the home-guard size;
  - the first attack window;
  - the raid reaction delay (for example 1 s / 3 s / 6 s);
  - the number of towers (Petra: 0-1 / 1 / 2).
- Keep any cheat visible in `gameoptions.yaml`.

### 4.3 The early-raid defence pattern, in one place

1. **Minute 0-2.** Build a settlement at the first food site. Train citizens without a break. 2-3 towers from the
   starting stone, at the worked sites. A dog if the map is hidden.
2. **When a raider hits a citizen.** Mark the site dangerous on the threat map. Its gatherers go to the nearest tower
   or the Capitol, and the site is released. Other sites keep working; don't stop the whole economy.
3. **If the raiders stand under a tower**, let the tower work. If they are few and away from towers, a militia of 3
   citizens per raider, at most 6 raiders, only with 3x strength.
4. **If an enemy tower goes up beside a worked site**, pull 4-6 citizens to kill the builders before it finishes, or
   abandon the site and build a tower that covers it.
5. **When the threat is gone**, return the gatherers and keep training citizens. The rusher has paid for the raid with
   his own economy.
6. **Stone Age**: the 15-20 home guard (§4.2, item 3) waits under the towers. Then back to citizens.
7. **Copper Age**: answer horse raids with spearmen and macemen on guard at the sites, plus towers and houses.

### 4.4 When to attack, in one place

- Not before the Stone Age guard is complete and the Copper Age saving is underway. The exception is the rush
  personality, whose script is deliberate.
- First windows: at the Copper Age advance (new units), or after a failed enemy attack.
- Go when the estimate is ≥ 1.3x against what was last seen in the target area, and the target has ≤ 6 towers or the
  force has siege.
- Target order: gatherers away from towers > soldiers attacking us > producers > the rest.
- Retreat below 0.8x. Reinforce in waves of 4-5, as the Medium AI thread did.

### 4.5 What to measure in benches

Add these to the scorecard; most are cheap from the trace [inference]:

| Metric | Why |
|---|---|
| Idle-producer seconds per 5 minutes (Capitol, town centres) | the first macro rule; the main source of a fair AI's economic lag |
| Citizens at 5, 10, 15, 20, 30 minutes | build-order fidelity; compare with the model and the classic |
| Food stock while not saving | floating |
| Time to the Stone Age and Copper Age (median and spread) | the main target |
| Citizens lost per 5 minutes, and per raid | the early-raid problem |
| Gatherers on sites above room (6) | over-crowding; the owner's rule |
| Seconds a citizen stays on a site under attack | the safety answer |
| Raid answers and "losing" retreats per game | dithering (8.38 counted 80-135 answers) |
| Soldiers lost to towers | the AoE2 weakness of diving into towers |
| Results against exploiter scripts | the AlphaStar lesson: a Prehistoric tower rush, a 6-minute clubman raid, a Copper horse raid |
| Spread across seeds and seats | repeatable play, the "same minute every game" criterion |

### 4.6 Where the Lua AIs stand against this

[data, from AI_RESEARCH §8.37-8.38]:
- `ageofai` at f8: the Stone Age at 48.5 minutes against the improved classic; about 40-42 citizens flat from minute 25
  to 45 (the reserve brake); 80-135 raid answers a game.
- The c10 settings: the Stone Age at 27.7.

The findings above name three things to fix:
- hold only the next epoch;
- don't gather Copper's gold and iron before the Stone Age;
- answer raids with towers and fleeing, not with the whole force at every sighting.

### 4.7 Where the evidence contradicts or qualifies the owner's notes

| Owner's note | Evidence | Verdict |
|---|---|---|
| Prehistoric: no army, all food into citizens, then advance | Supported for Standard by every economy-first source and the model. In **Tournament** play many guides build a barracks at once (Random Map Guide, Gollum's age-1 rushes), because the epoch is cheap. | **Agrees** for this project's Standard games. Add towers, which every defence source uses. |
| Stone Age: about 20 spearmen, towers, then citizens, then Copper | Medium AI thread: about 20 *slingers*; Large Islands: 20 spearmen in the Copper Age. The counter tables: **slingers beat spearmen**. | **Qualify**: a pure spearman guard is weak to a slinger or rock-thrower raid. Mix in slingers (no food cost) or clubmen by what is scouted. |
| First move: a settlement next to a resource | Gollum's openings, the tutorial, Large Islands. The Random Map Guide sends citizens to forage at the Capitol first. | **Agrees.** On maps with forage right by the Capitol the settlement can wait; that is minor. |
| Gatherers never above the site room (6 per forage) | Many EE sources say "up to six"; StarCraft's saturation logic agrees in spirit. | **Agrees.** |
| Raid defence = more economy, a waiting spearman army, towers | The EE threads (Clay, Ferrari_Warrior, Twins of DOOM) say the same, plus pulling citizens under towers, the citizen attack upgrade, and fighting only near towers. | **Agrees.** Add the fleeing citizen and "fight under the tower". |

### 4.8 Where the evidence is thin

- **No published Standard-variant timings** for good EE play. §4.1 is a model built on assumptions:
  - the training time under Standard's work-time factor (1.66: what it scales should be checked in the code);
  - the food rate with settlements beside the sites.
- **The EE sources are few and old.** One strong guide (Gollum and Kroucrouch), one map guide, a handful of 2001-2003
  forum threads, and one long multiplayer guide whose author says much of it is untested. The fan wiki pages were not
  readable directly (payment wall); their claims come from search snippets.
- **No community rating of AoC civ powers** was found.
- **Tower numbers** come from a generated damage table (byrdie/empire_earth), not from a game manual. They agree with
  what players say about towers.
- **AoE2 DE "no cheat" claims** are forum consensus, not developer statements.
- **Bot-competition lessons** come from StarCraft, where speed of control dominates more than in EE. The economy and
  architecture lessons transfer better than the micro ones.

---

## Sources

Empire Earth (community, guides, data):
- RS_Gollum and NW_Kroucrouch, "Empire Earth Guide", EE Heaven: https://ee.heavengames.com/eeh/strategies/article/empire-earth-guide/
- The same guide's forum thread: https://ee.heavengames.com/cgi-bin/forums/display.cgi?action=st&fn=1&tn=36100
- introspection, "Empire Earth - Random Map Guide", EE Heaven (2002): https://ee.heavengames.com/eeh/strategies/article/random-map-guide/
- "stopping a rush???????" thread, EE Heaven: https://ee.heavengames.com/cgi-bin/forums/display.cgi?action=ct&f=7,2309,120,all
- "Prehistoric rush techniques?" thread, EE Heaven: https://ee.heavengames.com/cgi-bin/forums/display.cgi?action=st&fn=7&tn=2125
- "Good strategy for beating medium skill level" thread, EE Heaven: https://ee.heavengames.com/cgi-bin/forums/display.cgi?action=st&fn=1&tn=4041
- "Copper Age - how to fend off the horsemen?" thread, EE Heaven: https://ee.heavengames.com/cgi-bin/forums/display.cgi?action=st&fn=7&tn=1906
- EE Heaven, Stone Age units (costs, train times, HP): https://ee.heavengames.com/eeh/gameinfo/units/stone/
- EE Heaven, Copper Age units: https://ee.heavengames.com/eeh/gameinfo/units/copper/
- sinful_666 (Suehyb Alkhatib) et al., "Empire Earth Multi-Player Strategies" v1.2 (2002-2003), hosted by Cyberly: https://www.cyberly.org/game-cheats/pc-walkthroughs/empire-earth-multi-player-strategies.html
- PC Gamer, "Strategy -- Empire Earth" (2002), archived: https://discmaster.textfiles.com/file/24073/2002-03_Disc_7.13.bin/Software/Extras/empireearth/empireearth_strategy.htm
- Empire Earth Fandom wiki, Large Islands (search snippet; page behind a payment wall): https://empireearth.fandom.com/wiki/Large_Islands
- Empire Earth Fandom wiki, Populating (search snippet): https://empireearth.fandom.com/wiki/Populating
- Empire Earth Fandom wiki, Pikeman (search snippet): https://empireearth.fandom.com/wiki/Pikeman_(Empire_Earth)
- Empire Earth Fandom wiki, Stone Age (docks, search snippet): https://empireearth.fandom.com/wiki/Stone_Age_(Empire_Earth)
- Save-EE wiki, Settings (search snippet; the site refused the connection): http://www.wiki.save-ee.com/view/Settings
- GameFAQs Empire Earth unit FAQ (v1.0, Dec 2002; counter system, via search snippet): https://gopherproxy.meulie.net/tilde.pink/0/~bencollver/gamefaqs/text/pc/empire-earth/empire-earth-faqs-20608.txt
- byrdie/empire_earth, generated Stone Age land unit relationships: https://empire-earth.readthedocs.io/en/latest/_autosummary/empire_earth.units.relationships.epochs.epoch_02_stone.land.html
- Wikipedia, Empire Earth: The Art of Conquest: https://en.wikipedia.org/wiki/Empire_Earth:_The_Art_of_Conquest
- Wikipedia, Empire Earth (video game): https://en.wikipedia.org/wiki/Empire_Earth_(video_game)
- GameSpot, "Empire Earth: The Art of Conquest Preview": https://www.gamespot.com/articles/empire-earth-the-art-of-conquest-preview/1100-2875181/
- GameSurge review (variant-dependent epoch costs, search snippet): https://www.gamesurge.com/pc/reviews/empire.shtml
- Steam store, Empire Earth Gold (Standard and Tournament modes, search snippet): https://store.steampowered.com/app/254760
- tl.net, "Empire Earth 1 strats" (search snippet): https://tl.net/forum/games/189514-empire-earth-1-strats
- Grille, Empire-Earth-Age-of-AI, README: https://github.com/Grille/Empire-Earth-Age-of-AI
- Grille, Empire-Earth-Age-of-AI, docs/economy.md: https://github.com/Grille/Empire-Earth-Age-of-AI/blob/main/docs/economy.md

General RTS strategy:
- AOE2 Guide, Build orders: https://aoe2-guide.pages.dev/en/learn/02-build-orders
- AOE2 Guide, Resources and villagers: https://aoe2-guide.pages.dev/en/learn/01-resources
- Expert Beacon, AoE2 flush build order (22 pop, Feudal about 10:40): https://expertbeacon.com/what-is-the-build-order-for-the-flush-aoe-2/
- Gameplay.tips, AoE IV roadmap for beginners: https://gameplay.tips/guides/age-of-empires-iv-roadmap-for-beginners-and-complete-noobs.html
- Age of Empires Series wiki, Early Economy (AoE IV): https://ageofempires.fandom.com/wiki/Early_Economy_(Age_of_Empires_IV)
- AoE IV villager count article (mirror, unverified): https://parrot-spinach-nd46.squarespace.com/articles/age-of-empires-iv-how-many-villagers-should-you-make
- TL.net, "SC2 Optimal worker saturation": https://tl.net/forum/starcraft-2/113577-sc2-optimal-worker-saturation?page=4
- TL.net, "In depth mining analysis: the magic number 8": https://tl.net/forum/starcraft-2/197826-in-depth-mining-analysis-the-magic-number-8?view=all
- TL.net, "A treatise on the economy of SCII": https://www.teamliquid.net/forum/legacy-of-the-void/482775-a-treatise-on-the-economy-of-scii?view=all
- Wikipedia, Turtling (gameplay): https://en.wikipedia.org/wiki/Turtling_(gameplay)
- Wikipedia, Rush (video games): https://en.wikipedia.org/wiki/Rush_(video_games)
- "How to be good at real-time strategy games" (Turtle < Expand < Rush): https://medium.com/how-to-be-good-at-real-time-strategy-games/how-to-be-good-at-real-time-strategy-games-pt-1-41012b02eb52
- AoE forums, "Meta boom" (AoE4 triangle debate): https://forums.ageofempires.com/t/meta-boom/234898?page=5

RTS AI design:
- UAlbertaBot wiki: https://github.com/davechurchill/ualbertabot/wiki
- UAlbertaBot, Design and Architecture: https://github.com/davechurchill/ualbertabot/wiki/Design-and-Architecture
- Churchill and Buro, "Incorporating Search Algorithms into RTS Game Agents": https://ojs.aaai.org/index.php/AIIDE/article/view/12548
- Jay Scott, StarCraft bots overview: https://www.satirist.org/learn-game/systems/starcraft/
- Jay Scott, "Steamhammer opponent modeling goals": https://satirist.org/ai/starcraft/blog/archives/346-Steamhammer-opponent-modeling-goals.html
- Jay Scott, "AIIDE 2020: what bots wrote" (PurpleWave's learning files): https://www.satirist.org/ai/starcraft/blog/archives/985-AIIDE-2020-what-bots-wrote-data.html
- AIIDE 2019 StarCraft AI Competition results: https://www.cs.mun.ca/~dchurchill/starcraftaicomp/2019/AIIDE2019_StarcraftAICompetition
- AIIDE 2017 StarCraft AI Competition report: https://www.cs.mun.ca/~dchurchill/starcraftaicomp/2017/AIIDE2017_StarcraftAICompetition.pdf
- IEEE Spectrum, "Custom AI programs take on top-ranked humans in StarCraft" (2015): https://spectrum.ieee.org/custom-ai-programs-take-on-top-ranked-humans-in-starcraft
- Ontañón et al., "A Survey of Real-Time Strategy Game AI Research and Competition in StarCraft", IEEE TCIAIG 5(4), 2013: https://www.wi.uni-muenster.de/publication/105500
- 0 A.D. Petra, config.js (difficulty settings): https://docs.wildfiregames.com/javascript/petra/config.js.html
- 0 A.D. Petra, attackManager.js: https://docs.wildfiregames.com/javascript/petra/attackManager.js.html
- 0 A.D. Petra, headquarters.js: https://docs.wildfiregames.com/javascript/petra/headquarters.js.html
- 0 A.D. AegisBot wiki (headquarters and queue manager): https://trac.wildfiregames.com/wiki/AegisBot
- 0 A.D. PetraBot wiki: https://trac.wildfiregames.com/wiki/PetraBot?version=12
- Wildfire Games forum, "AI difficulty levels": https://wildfiregames.com/forum/topic/18905-ai-difficulty-levels/page/3/
- airef, "The Seventeen Strategic Number Settings You Must Change Immediately": https://airef.github.io/resources/articles/sn-defaults-to-change.html
- airef, Strategic Numbers index: https://airef.github.io/strategic-numbers/sn-index.html
- AoE forums, "Barbarian AI Questions": https://forums.ageofempires.com/t/barbarian-ai-questions/198987
- AoE forums, "Does the AI cheat?": https://forums.ageofempires.com/t/does-the-ai-cheat/119262
- AoE forums, "Extreme heavy cheating of AI on higher difficulties": https://forums.ageofempires.com/t/extreme-heavy-cheating-of-ai-on-higher-difficulties/132280
- AoE forums, "AI units running into enemy Castles/Kreposts/Towers (and other Extreme AI weaknesses)": https://forums.ageofempires.com/t/ai-units-running-into-enemy-castles-kreposts-towers-and-other-extreme-ai-weaknesses/110809
- AoE forums, "Zycat AI Lite mod - Also state of the AI post" (AoE IV): https://forums.ageofempires.com/t/zycat-ai-lite-mod-also-state-of-the-ai-post/214837
- Dave Mark, "Modular Tactical Influence Maps", Game AI Pro 2, ch. 30: https://www.gameaipro.com/GameAIPro2/GameAIPro2_Chapter30_Modular_Tactical_Influence_Maps.pdf
- Soren Johnson, "GD Column 7: Our Cheatin' Hearts": https://www.designer-notes.com/?p=132
- Game Developer, "Analysis: Game AI & Our Cheatin' Hearts": https://www.gamedeveloper.com/game-platforms/analysis-game-ai-our-cheatin-hearts
- Human-like RTS bot paper (abstract seen via search; full text refused): https://core.ac.uk/download/233150197.pdf
- DeepMind, "AlphaStar: Grandmaster level in StarCraft II using multi-agent reinforcement learning" (2019): https://deepmind.google/discover/blog/alphastar-grandmaster-level-in-starcraft-ii-using-multi-agent-reinforcement-learning/
- Wikipedia, AlphaStar (software): https://en.wikipedia.org/wiki/AlphaStar_(software)
- "A Robust and Opponent-Aware League Training Method for StarCraft II" (NeurIPS 2023): https://papers.neurips.cc/paper_files/paper/2023/file/94796017d01c5a171bdac520c199d9ed-Paper-Conference.pdf

This repository:
- `docs/AI_RESEARCH.md` §8.22 (rates, the epoch reserve), §8.37-8.38 (the Lua AI benches, the copper-plan diagnosis)
- `build/open_data/tree/Data/db/dbgamevariant.csv`, `dbstartingresources.csv`, `civpowers.yaml` (variant cost factors,
  starting resources, AoC powers)
