# Tamagotchi P1: documentation of the first generation (rev_2)

English | [Español](tutorial_p1_rev_2.es.md)

Taken from the `.so` of Tamagotchi Classic (Android). Each section has a
piece of simplified pseudocode and the file in [doc/code](code) it comes
from, with the address of each function in the `.so`.

The names in capital letters (`HUNGER_06`, `SICK_TIMER_0d`...) are the pet's
data, `m_para`: a list of numbers that the game keeps in the saved game.
What each one is: [doc/code/m_para.h](code/m_para.h). The timers count
minutes (the egg, seconds) and `-1` means stopped.

## The characters

[setParaTimer.c](code/setParaTimer.c),
[InitCharacterData.c](code/InitCharacterData.c)

Each character has a card in `DATA_PARAMETER` (9 numbers) and its schedule
in `DT_SLEEP_TIME`. These are the values exactly as they are in the game:

| id | Name | Min. weight | Evolution | Hunger / happiness | Sickness scale | Sickness div | Injections | Unused | Starting discipline | Discipline timer | Wakes | Sleeps |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| 0 | Egg | 1 | 2 | -1 | -1 | -1 | 0 | 0 | 0 | -1 | -1 | -1 |
| 1 | Babytchi | 5 | 60 | 7 | 60 | 1 | 2 | 0 | 0 | -1 | -1 | -1 |
| 2 | Marutchi | 10 | 2 | 55 | 2 | 1 | 2 | 100 | 0 | 190 | 9 | 20 |
| 3 | Tamatchi | 20 | 3 | 80 | 3 | 1 | 2 | 50 | 2 | 480 | 9 | 21 |
| 4 | Tamatchi | 20 | 3 | 80 | 3 | 1 | 2 | 100 | 0 | 206 | 9 | 21 |
| 5 | Kuchitamatchi | 20 | 2 | 80 | 2 | 2 | 2 | 50 | 2 | 480 | 9 | 21 |
| 6 | Kuchitamatchi | 20 | 2 | 80 | 2 | 2 | 2 | 100 | 0 | 206 | 9 | 21 |
| 7 | Mametchi | 30 | 6 | 86 | 6 | 1 | 1 | 0 | 4 | -1 | 9 | 22 |
| 8 | Ginjirotchi | 30 | 4 | 86 | 4 | 1 | 1 | 50 | 2 | 520 | 9 | 22 |
| 9 | Maskutchi | 30 | 4 | 60 | 4 | 1 | 1 | 100 | 0 | 206 | 11 | 23 |
| 10 | Kuchipatchi | 20 | 2 | 65 | 2 | 1 | 2 | 0 | 4 | -1 | 9 | 22 |
| 11 | Nyorotchi | 10 | 1 | 65 | 1 | 2 | 3 | 50 | 2 | 206 | 9 | 22 |
| 12 | Tarakotchi | 20 | 2 | 48 | 2 | 2 | 2 | 100 | 0 | 520 | 10 | 22 |
| 13 | Bill (US) / Oyajitchi (Japan) | 30 | 6 | 86 | 6 | 1 | 1 | 0 | 4 | -1 | 9 | 22 |

What each field is:

- **Min. weight**: the weight never goes below this.
- **Evolution**: the egg, in seconds; Babytchi, in minutes; from the child
  on, in days.
- **Hunger / happiness**: every how many minutes it loses a hunger heart,
  and every how many it loses a happiness heart (the same number).
- **Sickness scale** and **div**: to work out when it gets sick (see "When
  it gets sick").
- **Injections**: doses of medicine to get well.
- **Unused**: 0, 50 or 100. The game never reads it: none of the
  functions that use the card touch this field (byte `0x18` of the row).
  Its values are the inverse of the starting discipline (`100 - discipline
  * 25`: 100 with 0, 50 with 2, 0 with 4), so it was probably a percentage
  tied to discipline in the original Tamagotchi.
- **Starting discipline**: the discipline level the stage starts with,
  from 0 to 4. A 2 stays at 2 or goes up to 3 at random:

  ```
  DISCIPLINE_09 = starting discipline
  if it is 2: DISCIPLINE_09 = (random() <= 0.5) ? 3 : 2    // 3 with 51 %
  ```
- **Discipline timer**: how often it misbehaves; the real value is between
  half and the full value (see "When it misbehaves"). `-1`: never.
- **Wakes** and **Sleeps**: the phone's hour, from 0 to 23.

Also:

- The two Tamatchi (3 and 4) and the two Kuchitamatchi (5 and 6) only
  differ in discipline: the ones with an odd id start with 2 or 3 and
  misbehave every 240–480 min; the ones with an even id start with 0 and
  misbehave every 103–206 min.
- Bill and Oyajitchi are the same character (id 13): one in the US
  version and the other in the Japanese one.

## Time

[doControlIvent/clock.c](code/doControlIvent/clock.c)

While playing, the game checks the time about 16 times per second and moves
the pet's clock forward in steps: one step per minute (the egg, one per
second). On each step all its timers go down.

```
while pet_clock < real_time:
    pet_clock += 1 minute              // the egg: 1 second
    each running timer: timer -= 1     // when it reaches 0, its thing happens
```

- With the game closed, time keeps counting: when you open it, it runs
  all the missing steps at once, with no animations or sounds. If it was
  closed for 3 hours, that is 180 steps in a row.
- The wake up and sleep schedule uses the time of the phone's clock, with
  its time zone.

## The egg and Babytchi

They are the tutorial characters: they follow a fixed pattern.

**The egg** hatches 2 seconds after you start playing: while you are on
the title screen, its counter goes back to 2
([doControlIvent/clock.c](code/doControlIvent/clock.c)).

In Tamagotchi L.i.f.e. v1.0 and v1.01 it takes 2 minutes: the counter is the
same (`TRANSFORM_0e`, which starts at 2), but the egg does not count in
seconds but in minutes, like the other characters. It goes down by one at
each minute change of the clock, so it hatches between 1 and 2 minutes after
you start playing (measured: between 94 and 125 s). From L.i.f.e. v1.02 on
it is 2 seconds again.

**Babytchi** ([InitCharacterData.c](code/InitCharacterData.c)):

```
when born:
    HUNGER_TIMER_0a = 0           // loses a hunger heart right away
    HAPPY_TIMER_0b  = 2           // and a happiness heart after 2 minutes
    POOP_TIMER_0c   = 15          // a poop after 15 minutes
    SICK_TIMER_0d   = 45          // gets sick once, after 45 minutes
    TRANSFORM_0e    = 60          // evolves after 60 minutes
```

After that it loses a heart every 7 minutes. Also:

- Its calls close by themselves after 2 minutes (hunger, happiness and
  discipline) or after 5 (the light one), but they **do not count as a
  mistake**, so they do not affect its evolution
  ([setMissCnt.c](code/setMissCnt.c)):

  ```
  if a call closes unanswered and the character is not Babytchi:
      one more mistake
  ```
- It takes a nap between minutes 40 and 44 of its stage, and when it
  wakes up it makes a poop
  ([doControlIvent/awake.c](code/doControlIvent/awake.c),
  [doControlIvent/sleeping.c](code/doControlIvent/sleeping.c)).
- Eating and playing do not change its weight, and snacks do not count
  for it ([setCare.c](code/setCare.c)).
- It gets sick only once: after that, its sickness timer stays stopped
  ([setParaTimer.c](code/setParaTimer.c)). Timers only run while awake,
  so with the nap it gets sick around minute 50.
- It does not die ([doControlIvent/awake.c](code/doControlIvent/awake.c)).

What follows applies from the child on.

## What is reset to 0 when evolving

[InitCharacterData.c](code/InitCharacterData.c)

```
SICK_COUNT_12      = 0    // times it has been sick
CANDY_22           = 0    // snack counter
SNACKS_1f          = 0    // snacks towards that counter
CARE_MISS_18       = 0    // care mistakes
DISCIPLINE_MISS_19 = 0    // discipline mistakes
STAGE_DAYS_21      = 0    // days in the stage
NO_FOOD_COUNT_13   = 0    // times without eating (adults)
STAGE_MINUTES_1e   = 0    // minutes in the stage
call deadlines = -1
```

## Calls

[doControlIvent/calls.c](code/doControlIvent/calls.c),
[setMissCnt.c](code/setMissCnt.c)

| Call | When | How to answer it | Deadline | If not answered |
|---|---|---|---|---|
| Hunger | `HUNGER_06` reaches 0 | a meal | 15 min | care mistake |
| Happiness | `HAPPY_07` reaches 0 | a snack or winning the minigame | 15 min | care mistake |
| Light | it falls asleep | turn off the light | 60 min | care mistake |
| Discipline | `DISCIPLINE_TIMER_0f` runs out | scold it | 30 min | discipline mistake |

```
when a call's deadline runs out:
    the call closes
    one more mistake (care or discipline)
```

One exception, only once in the whole game: the first time a hunger call
closes, answered or not, the happiness call also closes if it was open,
without counting a mistake for it.

```
when a hunger call closes:
    if it has never happened yet (flag 0x400 is missing):
        the happiness call also closes, with no mistake
        flag 0x400                      // it does not happen again
```

The flag `0x400` is not cleared when evolving, only when starting a new
game ([InitCharacterData.c](code/InitCharacterData.c)).

- Missing the deadline with hunger at 0 only counts the mistake: it does
  not make it sick.
- When it falls asleep, the open hunger, happiness and discipline calls
  close and count as a mistake
  ([doControlIvent/awake.c](code/doControlIvent/awake.c)).

## Feeding

[setCare.c](code/setCare.c)

```
meal:
    if it is misbehaving, is sick or HUNGER_06 == 4: refuses it
    HUNGER_06 += 1
    WEIGHT_02 += 1                  // up to 99
    the hunger timer starts again
    STARVE_TIMER_10 = -1            // no longer dies of hunger
    closes the hunger call

snack:                              // never refuses it
    HAPPY_07 += 1                   // up to 4
    SNACKS_1f += 1
    if SNACKS_1f == 3: SNACKS_1f = 0, CANDY_22 += 1
    WEIGHT_02 += 2                  // up to 99
    the happiness timer starts again
    closes the happiness call
```

## When it loses a heart

[setParaTimer.c](code/setParaTimer.c),
[doControlIvent/calls.c](code/doControlIvent/calls.c)

```
HUNGER_TIMER_0a = HAPPY_TIMER_0b = "hunger / happiness" from its card
when it reaches 0:
    HUNGER_06 -= 1  (or HAPPY_07 -= 1)
    if it reaches 0: hunger call (or happiness call)
    the timer starts again
```

When it runs out of hunger hearts, `STARVE_TIMER_10` starts: 12 hours
to die of hunger.

## Playing

[setCarePlayResult.c](code/setCarePlayResult.c)

```
win:
    WEIGHT_02 -= 1                  // without going below the min. weight
    HAPPY_07 += 1                   // up to 4
    the happiness timer starts again
    closes the happiness call
lose:
    WEIGHT_02 -= 1                  // without going below the min. weight
```

## The light

[setCare.c](code/setCare.c),
[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c)

```
when it wakes up: light on
turning off the light with the light call open: the call is answered
```

## Medicine

[setCare.c](code/setCare.c)

```
if it is sick:
    MEDICINE_05 -= 1
    if it reaches 0: it gets well, and SICK_DEATH_TIMER_11 = -1
if not: refuses it
```

When it gets sick, `MEDICINE_05` is set to the "injections" of its card.

## When it gets sick

[setParaTimer.c](code/setParaTimer.c),
[setUntchAdd.c](code/setUntchAdd.c),
[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c),
[doControlIvent/helpers.c](code/doControlIvent/helpers.c)

For three reasons.

**1. With 8 poops on screen** ([setUntchAdd.c](code/setUntchAdd.c)).

**2. Over time**: when `SICK_TIMER_0d` runs out, it gets sick and the timer
starts again ([setParaTimer.c](code/setParaTimer.c)):

```
awake_minutes = (sleeps - wakes) * 60              // awake in a day
base          = awake_minutes * scale / div
SICK_TIMER_0d = base - base/4 * random() - base/4 * random()
```

### How to choose scale and div

**`scale` is the days and `div`, how many parts that time is split into.**
The result, `scale ÷ div`, is how often it gets sick in days, at most; the
randomness makes it shorter, down to half.

| scale | div | scale ÷ div | With 12 h awake per day | Gets sick |
|---|---|---|---|---|
| 6 | 1 | 6 days | every 36 to 72 h | every 3 to 6 days |
| 8 | 2 | 4 days | every 24 to 48 h | every 2 to 4 days |
| 4 | 2 | 2 days | every 12 to 24 h | every 1 to 2 days |
| 6 | 3 | 2 days | every 12 to 24 h | every 1 to 2 days |
| 2 | 2 | 1 day | every 6 to 12 h | 1 or 2 times a day |
| 1 | 2 | 1 day in 2 parts | every 3 to 6 h | 2 to 4 times a day |
| 1 | 3 | 1 day in 3 parts | every 2 to 4 h | 3 to 6 times a day |

- Days only count the awake hours, because the timer stops at
  night.

With each character's values, in awake hours (`awake hours per
day × scale ÷ div`):

| Character | Awake per day | scale | div | Calculation | Gets sick every |
|---|---|---|---|---|---|
| Marutchi | 11 h | 2 | 1 | 11 × 2 ÷ 1 = 22 h | between 11 h and 22 h |
| Tamatchi (3 and 4) | 12 h | 3 | 1 | 12 × 3 ÷ 1 = 36 h | between 18 h and 36 h |
| Kuchitamatchi (5 and 6) | 12 h | 2 | 2 | 12 × 2 ÷ 2 = 12 h | between 6 h and 12 h |
| Mametchi | 13 h | 6 | 1 | 13 × 6 ÷ 1 = 78 h | between 39 h and 78 h |
| Ginjirotchi | 13 h | 4 | 1 | 13 × 4 ÷ 1 = 52 h | between 26 h and 52 h |
| Maskutchi | 12 h | 4 | 1 | 12 × 4 ÷ 1 = 48 h | between 24 h and 48 h |
| Kuchipatchi | 13 h | 2 | 1 | 13 × 2 ÷ 1 = 26 h | between 13 h and 26 h |
| Nyorotchi | 13 h | 1 | 2 | 13 × 1 ÷ 2 = 6 h 30 min | between 3 h 15 min and 6 h 30 min |
| Tarakotchi | 12 h | 2 | 2 | 12 × 2 ÷ 2 = 12 h | between 6 h and 12 h |
| Bill / Oyajitchi | 13 h | 6 | 1 | 13 × 6 ÷ 1 = 78 h | between 39 h and 78 h |

The egg and Babytchi do not have it (Babytchi gets sick only once, see "The
egg and Babytchi").

**3. Because of snacks**, when it wakes up
([doControlIvent/sleeping.c](code/doControlIvent/sleeping.c)):

```
if random() * 100 < CANDY_22 * 10:
    CANDY_SICK_23 = a random minute of the awake hours,
                    more than 60 minutes away from SICK_TIMER_0d
CANDY_22 = 0
```

- `CANDY_22` goes up 1 every 3 snacks ([setCare.c](code/setCare.c)), and
  each point is a 10 % chance: 3 snacks, 10 %; 30 snacks, 100 %.
- It is checked once a day, when it wakes up, and the counter goes back to
  0 whatever the result: one day's snacks only count for the next day.
- If it hits, the game picks at what time of day it will get sick: a
  random minute, counted from when it wakes up. If that minute falls less
  than 60 minutes from the moment `SICK_TIMER_0d` runs out (reason 2),
  before or after, it picks another one. So it never gets sick twice less
  than an hour apart.

  For example, Marutchi wakes up at 9:00 and its `SICK_TIMER_0d` runs out
  at 14:00. It hits, so it will get sick because of snacks:

  ```
  minute 270 comes up, 13:30   -> 30 min from 14:00: not valid
  minute 500 comes up, 17:20   -> more than 60 min away: valid
  ```

  That day it gets sick twice: at 14:00 because of the timer and at 17:20
  because of the snacks.

When it gets sick:

```
MEDICINE_05         = injections from its card
SICK_COUNT_12      += 1                 // with 3, it dies
SICK_DEATH_TIMER_11 = 360               // 6 hours to get well
```

## When it misbehaves

[setParaTimer.c](code/setParaTimer.c),
[doControlIvent/calls.c](code/doControlIvent/calls.c)

Every so often, the pet misbehaves: it calls the player without needing
anything. That time comes from the "discipline timer" of its card:

```
t = discipline timer from its card
DISCIPLINE_TIMER_0f = t - t/2 * random()    // random() from 0 to 1
```

It is a random number between half of `t` and `t`. What this means:

- `t` is the longest it can go without misbehaving.
- The randomness makes it not always misbehave at the same time: so you
  cannot know when it will happen.
- But never before half of `t`: so it does not misbehave twice almost in
  a row.

To make a card: the smaller `t`, the more often it misbehaves.

For example, Tamatchi (3) has `t = 480` awake minutes:

```
random() = 0    ->  480 - 240 * 0    = 480 minutes  (8 hours)
random() = 0.5  ->  480 - 240 * 0.5  = 360 minutes  (6 hours)
random() = 1    ->  480 - 240 * 1    = 240 minutes  (4 hours)
```

It never goes more than 8 hours without misbehaving, nor less than 4.

When the timer reaches 0:

1. It calls the player: this is the discipline call.
2. The timer starts again with another random value, answered or not.
3. There are 30 minutes to scold it. If not, a discipline mistake counts.
4. While the call stays open, it refuses food.

The characters with `t = -1` on their card (the egg, Babytchi, Mametchi,
Kuchipatchi and Bill / Oyajitchi) never misbehave.

## Scolding

[setCare.c](code/setCare.c)

```
if it is misbehaving:
    closes the discipline call
    DISCIPLINE_09 += 1                  // up to 4
if not: nothing happens
```

## When it poops

[doControlIvent/awake.c](code/doControlIvent/awake.c),
[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c),
[InitCharacterData.c](code/InitCharacterData.c),
[setUntchAdd.c](code/setUntchAdd.c),
[setCare.c](code/setCare.c)

The pet leaves poops on the screen, and they stay there until they are
cleaned. The game counts them in `POOP_08`. When it poops depends on the
stage.

### The egg and Babytchi

The egg does not poop. Babytchi makes two, at fixed times, to teach how
to clean:

1. 15 minutes after being born.
2. When it wakes up from the nap, at minute 45.

```
when born:                    POOP_TIMER_0c = 15
after 15 minutes:             poop, and the timer stops (-1)
when it wakes up from nap:    poop
```

After that it does not make any more until it evolves.

### From the child on

It poops **every 3 awake hours**, always exact: there is no randomness
here.

```
when reaching the stage:  POOP_TIMER_0c = 180       // minutes: 3 hours
when it reaches 0:        poop, and the timer goes back to 180
```

The timer starts counting when it evolves, not at a fixed time of day. And
since it stops at night, the poop time changes from one day to the next.
For example, a Marutchi (awake from 9:00 to 20:00) that evolved at 10:00:

```
10:00  evolves: the timer starts at 180
13:00  poop
16:00  poop
19:00  poop            180 minutes left until the next one
20:00  falls asleep    the timer stops with 120 minutes
 9:00  wakes up        continues with 120 minutes
11:00  poop            and so on, every 3 awake hours
```

### Cleaning

Cleaning removes all the poops at once (`POOP_08 = 0`), but it does not
change the timer: the next one comes at its time anyway.

If **8 poops pile up without cleaning, it gets sick** (see "When it gets
sick").

## Causes of death

[doControlIvent/awake.c](code/doControlIvent/awake.c),
[setDay.c](code/setDay.c)

```
STARVE_TIMER_10 reaches 0      // 12 hours with no hunger hearts
SICK_DEATH_TIMER_11 reaches 0  // 6 hours sick without getting well
SICK_COUNT_12 > 2              // 3 sicknesses in the same stage
NO_FOOD_COUNT_13 > 4           // adults: 5 times without eating
```

`NO_FOOD_COUNT_13` only goes up for adults (id 7 and up) that have no days
left to evolve, each time they run out of hunger hearts
([doControlIvent/calls.c](code/doControlIvent/calls.c)).

Snacks do not kill directly: they make it sick, and sickness can
kill.

## Sleeping and waking up

[doControlIvent/awake.c](code/doControlIvent/awake.c),
[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c),
[setCare.c](code/setCare.c)

Each character has a fixed schedule: the time it wakes up and the time it
falls asleep, by the phone's clock. While it sleeps, the game almost stops
for it.

### The schedule

[doControlIvent/helpers.c](code/doControlIvent/helpers.c)

It is not on the card (`DATA_PARAMETER`), but in another separate table,
`DT_SLEEP_TIME` (`@0xa3a88`). It has one row per character, in the same
id order, with two numbers: the wake up hour and the sleep hour, in whole
hours from 0 to 23. They are the **Wakes** and **Sleeps** columns of the
table in "The characters".

There is no formula or randomness: they are fixed values, the same every
day. They only change when evolving, because each character has its own
row. The egg and Babytchi have `-1`: they have no schedule.

To know if it is awake, the game compares the phone's hour
(`DAY_HOUR_1d`, only the hour, no minutes) with its row:

```
awake = wakes <= hour < sleeps
```

For example, Marutchi has `9` and `20`:

```
hour 8   ->  8 < 9          asleep
hour 9   ->  9 <= 9 < 20    awake
hour 19  ->  19 < 20        awake
hour 20  ->  20 < 20 no     asleep
```

So it is awake from 9:00 to 19:59 (11 hours a day) and falls asleep
right at 20:00. The awake hours per day, `sleeps - wakes`, are the ones
used by the sickness and evolution calculations.

### The egg and Babytchi

The egg does not sleep. Babytchi has no schedule: it takes a short nap,
between minutes 40 and 44 of its stage, to teach how to turn off the
light. There are 5 minutes to do it, and when it wakes up it makes a poop.

### When it falls asleep

When its bedtime comes. For example, Marutchi sleeps from 20:00 to 9:00.

At that moment:

1. The hunger, happiness and discipline calls that were still open
   close and count as a mistake: you have to answer them before it falls
   asleep.
2. It calls for the light to be turned off. There are 60 minutes to do it;
   if not, a care mistake counts.

```
when bedtime comes:
    open calls -> close and count as a mistake
    LIGHT_CALL_17 = 60                  // minutes to turn off the light
```

### While it sleeps

All its timers stop: it does not lose hearts, does not poop, does not get
sick, does not misbehave and does not move towards evolution. The only
thing that runs is the deadline to turn off the light. That is why all
the times in this document are "awake hours".

### When it wakes up

When its wake up time comes. Then a new day starts:

```
when wake up time comes:
    the light turns on
    AGE_01 += 1                         // one more day of age, up to 99
    STAGE_DAYS_21 += 1                  // one more day in this stage
    EVOL_DAYS_20 -= 1                   // one day less to evolve;
                                        // with 0, it evolves today
    checks if it gets sick today because of snacks (see "When it gets sick")
```

### Example

A Marutchi that has just evolved, with 2 days to evolve:

```
day 1   9:00   wakes up: age +1, days in the stage 1, 1 left to evolve
        20:00  falls asleep: open calls = mistakes; 60 min to turn off the light
day 2   9:00   wakes up: age +1, days in the stage 2, 0 left: evolves today
```
## When it evolves

[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c),
[doControlIvent/transform.c](code/doControlIvent/transform.c)

When it wakes up on the day its evolution days run out:

```
TRANSFORM_0e = 120 + random() * ((sleeps - wakes) * 60 - 121)
```

That is, between 2 hours after waking up and one minute before sleeping.
When it reaches 0, the tree decides what it turns into.

## Evolution tree

[setTransform.c](code/setTransform.c)

"Care" is the care mistakes (`CARE_MISS_18`) and "discipline", the
discipline mistakes (`DISCIPLINE_MISS_19`) of the stage that ends.

| From | Condition | To |
|---|---|---|
| Egg (0) | — | Babytchi (1) |
| Babytchi (1) | — | Marutchi (2) |
| Marutchi (2) | care < 3, discipline < 2 | Tamatchi (3) |
| Marutchi (2) | care < 3, discipline > 1 | Tamatchi (4) |
| Marutchi (2) | care > 2, discipline < 2 | Kuchitamatchi (5) |
| Marutchi (2) | care > 2, discipline > 1 | Kuchitamatchi (6) |
| Tamatchi (3) | care < 3, discipline 0 | Mametchi (7) |
| Tamatchi (3) | care < 3, discipline 1 | Ginjirotchi (8) |
| Tamatchi (3) | care < 3, discipline > 1 | Maskutchi (9) |
| Tamatchi (3) | care > 2, discipline 0 or 1 | Kuchipatchi (10) |
| Tamatchi (3) | care > 2, discipline 2 or 3 | Nyorotchi (11) |
| Tamatchi (3) | care > 2, discipline > 3 | Tarakotchi (12) |
| Tamatchi (4) | care < 4, discipline < 2 | Ginjirotchi (8) |
| Tamatchi (4) | care < 4, discipline > 1 | Maskutchi (9) |
| Tamatchi (4) | care > 3, discipline < 8 | Nyorotchi (11) |
| Tamatchi (4) | care > 3, discipline > 7 | Tarakotchi (12) |
| Kuchitamatchi (5) | care < 3, discipline 0 or 1 | Kuchipatchi (10) |
| Kuchitamatchi (5) | care < 3, discipline 2 | Nyorotchi (11) |
| Kuchitamatchi (5) | care < 3, discipline > 2 | Tarakotchi (12) |
| Kuchitamatchi (5) | care > 2, discipline < 4 | Nyorotchi (11) |
| Kuchitamatchi (5) | care > 2, discipline > 3 | Tarakotchi (12) |
| Kuchitamatchi (6) | care < 3, discipline 0 | Kuchipatchi (10) |
| Kuchitamatchi (6) | care < 3, discipline 1 | Nyorotchi (11) |
| Kuchitamatchi (6) | care < 3, discipline > 1 | Tarakotchi (12) |
| Kuchitamatchi (6) | care > 2, discipline < 6 | Nyorotchi (11) |
| Kuchitamatchi (6) | care > 2, discipline > 5 | Tarakotchi (12) |
| Maskutchi (9) | care < 6, discipline 0, 7 days or more in the stage | Bill / Oyajitchi (13) |

The other adults do not change.

## The gaps in m_para

[doControlIvent/sleeping.c](code/doControlIvent/sleeping.c),
[getters.c](code/getters.c), [m_para.h](code/m_para.h)

### Flag 0x8: death with another animation

Mametchi, Ginjirotchi and Maskutchi (7, 8 and 9) set it on the day their
evolution days run out. Neither of the first two evolves, so they carry it
from that day until the end. Maskutchi loses it if it turns into
Bill / Oyajitchi, because evolving clears it.

It is only read in one place, `controlGame` (`@0x3a784`), on the death
screen. `setDay` (`@0x38160`) shows the death with animation `0x12` and
writes down the time in `m_dayTimer`. After one minute:

```
if the animation is still 0x12 (death):
    with flag 0x8:     goes to animation 0x13
    without it:        waits half a second and goes to section 9
if not (it is already 0x13):
    goes to section 9
```

That is, these three characters have one more animation when they die,
after the usual one. What is shown in `0x13` and what section 9 is has
not been checked on screen yet.

### Positions 0x03 and 0x29 to 0x2f

No code in the game reads or writes them one by one. They only move when
the whole `m_para` is copied:

- when saving and loading the game (`0x00` to `0x2f`, see
  [saveRecord.c](code/saveRecord.c));
- `FutureScanner` (`@0x3dc38`), which prepares the Android notifications,
  copies `0x00` to `0x25` (`0x98` bytes) to simulate what will happen;
- `GetParam` and `SetMParam` accept any index up to `0x25`.

So neither `FutureScanner` nor `GetParam` reach `0x29` to `0x2f`: they are
saved space with no use. `0x03` does go into those copies, but nobody
reads it afterwards.

