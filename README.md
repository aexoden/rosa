# Rosa

Rosa is a tool to generate step routes for the SNES/SFC release of Final Fantasy
IV. Step routes are specific walking patterns designed to manipulate the timing
and details of random encounters. They are primarily used for speedrunning the
game. Additional details can be found at [ff4kb.aexoden.com](https://ff4kb.aexoden.com).

![GitHub release (latest SemVer)](https://img.shields.io/github/v/release/aexoden/rosa) ![C++](https://github.com/aexoden/rosa/workflows/C++/badge.svg)

## Prerequisites

Rosa requires meson, Boost and LMDB to be installed on your system.

Currently, if building from a tarball, submodules will not be available and will
need to be copied to the `external` directory manually. If building directly
from git, you should ensure submodules are initialized before building. The
following third-party submodules are required:

* [CLI11](https://github.com/CLIUtils/CLI11), a C++11 command line parser,
  released under a 3-clause BSD license.

* [cpp-peglib](https://github.com/yhirose/cpp-peglib), a C++17 PEG library
  released under the MIT license.

* [LMDB++](https://github.com/hoytech/lmdbxx), a C++17 wrapper for LMDB released
  under the Unlicense.

* [tsl::sparse_map](https://github.com/Tessil/sparse-map), a memory efficient
  replacement for std::unordered_map released under the MIT license.

## Building

Rosa makes use of C++17 (and potentially beyond) features, so it requires a
modern compiler to build. It is currently tested with g++ 8 and clang++ 10.
Slightly older versions of clang++ may work as well.

To build the executable, enter that main directory and execute the following:

```sh
mkdir build
meson build
cd build
ninja
```

To run the executable, you should return to the main directory as your working
directory, as Rosa expects data files to be in certain locations.

## Usage

`src/rosa [OPTION...]`

No options are strictly required, as there are sane defaults for all options,
but some minimal options should be set to get any sort of real use out of the
software.

### Options

#### `-r, --route`

Specifies the route definition for which to generate a step route. This file
should be named and located in the form `data/routes/%ROUTE%.txt` where %ROUTE%
corresponds to the given route name. More details about the format of this file
are available later in this README.

#### `-s, --seed`

Specifies the seed for which to generate the step route. This should be an
integer from 0 to 255 (inclusive).

#### `-m, --maximum-steps`

Specifies the maximum number of extra steps a route can take in a given segment.
Zero is a slightly special value that disables all optimizations.

#### `-n, --maximum-step-segments`

Specifying this parameter enables a special mode where the number of segments
where extra steps can be taken is restricted to the given number. This will
generally lower overall performance, especially once you start getting into the
10+ range. However, it is faster for smaller values, and any persistent cache is
reusable across runs, regardless of how the parameter is changed. This allows
you to generate routes quickly that use a few locations, and then refine those
into more complicated routes. Do note, however, that the total time taken by
multiple runs in this fashion is slower than a single run. It is, however,
faster to use a preexisting cache from a faster run than to go entirely from
scratch.

#### `-v, --variables`

Specifies values for the given variable indices. The format is `a:b c:d...`
where `a` and `c` are indices and `b` and `d` are values. In addition, a range
of values can be specified by appending a hyphen and an additional number (e.g.
`a:b-c`).

#### `-t, --tas-mode`

Adjusts the optimization to use TAS-specific features. This currently only uses
the minimum encounter timings instead of the average, but additional features
are planned.

#### `-p, --prefer-fewer-locations`

When the `maximum-step-segments` option is configured, this option enforces that
routes will always use the fewest locations possible to take steps that still
gives the optimum result. The default behavior is instead to prefer taking every
step as late as possible. Enabling this option will increase runtime to some
degree, as it requires calculating additional states.

#### `-c, --cache-type`

Sets the type of cache used. There are two options available: `dynamic` or
`persistent`. The default is `dynamic`, which will cache all states in memory,
discarding the data when complete.

The second option is `persistent`, which, in addition to an in-memory cache,
saves the data in a persistent database on disk. Rosa makes no attempt to manage
the lifetime of this cache, and you are expected to know what you are doing. The
primary purpose of this option is to persist the database for use in repeated
runs (potentially fixing only the first variables in a route). Performance is
too degraded to recommend for any other purpose, especially if the entire
database cannot fit in memory. When using this cache, if the route definition
changes or parameters are modified, using an existing cache can result in
suboptimal generated routes. The current maximum size of this database is 128GB.
While none of the default routes should exceed this, the `no64-excalbur` route
is close to the boundary.

#### `-l,--cache-location`

If using a persistent cache, controls the directory where the cache is located.
The default is `cache/`. Each route/seed combination will automatically use its
own individual cache. There is not expected to be significant overlap between
seeds. If you wish to start with a fresh cache, you should delete the relevant
directory manually.

#### `-f,--cache-filename`

If using a persistent cache, directly specify the name of the directory where
the cache will be located. The directory will be created if it already exists.
As with the previous option, this location will be created it if it doesn't
exist, and will never be deleted automatically. If specified, this option
overrides the previous option.

#### `-x,--cache-size`

If using a persistent cache, determines the size of the in-memory cache. The
default is system-dependent, but should be at least 2^31 - 1, which will most
likely result in the database being kept entirely in memory. Values are written
to memory and the persistent cache simultaneously, and keeping the in-memory
cache is a performance optimization.

## File Formats

### Field Definitions

There are a few fields that are mostly free-form strings, but the code may make
assumptions about their values and format. These are defined below. Deviating
from these formats may or may not work, and the ultimate behavior is undefined.

#### Party Specification

A party is indicated by a 20-character string, with each character representing
the following:

1. The number of party members in the front row. Must be either `2` or `3`.

2. An indication of whether or not the party current has GP. Should be `-` if
   not or `G` if they do.

3. An indication of whether or not this party is on a world map. Should be `-`
   if not and `+` if they are.

4. Characters 4-18 consist of 5 sets of 3 characters. The first character in
   each set is any letter from A-Z, with a capital letter designating an alive
   character and a lowercase letter designating a dead one. The following two
   characters are the agility of the character in decimal form.

5. The last two characters are the average party level, in decimal form.

#### Variable Name

Variable names are hexadecimal numbers designed to be used partially as if they
were strings. Any hexadecimal number is technically a valid variable name, but
the included routes are standardized on the following format:

The first digit should be a letter from A to E, specifying the type of variable.
The only currently used letters are `C` for a choice or `E` for extra steps.

The next four digits correspond to the related map number, as defined in the map
definition.

The final two digits are an index, in case that particular map has more than one
variable.

### Encounter Formation Specification

This file defines the encounter formations and their groups. The only currently
available data is for the USA release of Final Fantasy II. Custom data sets can
be created by the user, however.

These files live in the `data/encounters` directory, and consist of
tab-delimited lines. Note that all fields must be filled. If a field needs to be
empty, a `-` should be used. The two types of lines are:

#### ENCOUNT

This line defines a particular encounter formation. The six fields are:

1. `ENCOUNT`

2. A number from 0 to 511 indicating the formation number.

3. A textual description of the encounter. This can be omitted if this
   formation number has already been assigned a description earlier in the file.

4. The party for which this particular line provides data.

5. The average duration of an encounter in frames, specified as a decimal with
   three digits after the decimal point.

6. The minimum duration of an encounter in frames, again specified as a decimal
   with three digits after the decimal point. In this case, those three digits should be zero.

#### GROUP

This line defines a group of formations. The fields are:

1. `GROUP`

2. The number of the group, ranging from 0 to 255.

3. A tab-delimited list of eight numbers indicating the formation numbers that
   are in this group. They need not be unique.

### Map Specification

This file provides data about the maps available in the game. Similar to the
encounter formation data, this file currently contains data for the USA release
of Final Fantasy II, but custom data sets can be created by the user.

The file is located in the `data/maps` directory and consists of many
tab-delimited lines, with lines conforming to the below format:

#### MAP

This line defines a map. The fields are:

1. `MAP`

2. The map number, consisting of four hexadecimal numbers. For dungeon maps,
   this should be `3` followed by the three-digit map number. For overworld,
   underworld, and lunar maps, the first digit should be `0`, `1` or `2`
   respectively. The final three digits should refer to unique map regions.

3. The encounter rate of the map/region.

4. The encounter group of the map/region.

5. The title of the map. This string is currently unused in the software, but is
   intended to be the on-screen description of the map when entering.

6. The description of the map. This should be a complete textual description of
   the map, sufficient to adequately distinguish it from any other map.

### Route Definition

Route definition files live in `data/routes` and are named according to the
short name for a route (e.g. `paladin.txt` for the Paladin% route).

They consist of many lines which provide information about the route. Like the
other files, each line is a tab-delimited line. In addition, the file is limited
to a maximum of 65536 lines (not including empty lines and comments). The
program is not incredibly robust, and if your route description is malformed in
some way, the program will likely crash.

There are numerous different lines, which will be described in the following
sections.

#### Header

##### ROUTE

This line provides the name of the route, and consists of `ROUTE` followed by
the name.

##### VERSION

This line provides the route version. This should be `VERSION` followed by an
integer, ideally incremented after every change.

##### DATA

This line determines which data set to use. The default and standard is `ff2us`,
which describes the maps and encounters of Final Fantasy II (USA). Custom data
sets can be created by creating the appropriate files in the data directories.

#### Standard Types

##### NOTE

This is a line which will generate extra output, usually to help describe a
location or indicate the position of a boss battle. The two fields are:

1. `NOTE`

2. A string with the desired extra output.

##### PARTY

This line changes the currently active party. The two fields are:

1. `PARTY`

2. The new party, as a 20 character string as defined above.

##### PATH

This is the primary line type. It consists of the following fields:

1. `PATH`

2. A 7-digit hexadecimal number representing the variable name for this segment.
   If extra steps are not allowed in this segment, this should be `-`.

3. The 4-digit map number, in hexadecimal.

4. The number of tiles traversed on this segment.

5. The number of required steps on this segment.

6. The number of optional steps on this segment.

7. The number of transitions from map to map on this segment.

8. A `+` if single extra steps are available, and a `-` if they are not.

9. A `+` if pairs of extra steps are available, and a `-` if they are not.

10. A `+` if this segment can take an extra steps for free when saving, and a
    `-` if not. This is a TAS-specific feature that currently has no relevance
    in the software.

11. A number of frames (can be floating point) to apply as a penalty to the
    first encounter in this segment. Must be determined experimentally for the
    desired segment.

#### Choices

These three lines are used to define branching in the route. (For instance, to
enter a side room and take extra steps. There is experimental support for
nesting these, but this feature has not been thoroughly tested.

##### CHOICE

This line indicates that there are multiple options on how to proceed. The line
has the following fields:

1. `CHOICE`

2. A 7-digit hexadecimal number assigning the relevant variable name. Assigning
   a variable name of `-` will essentially make it impossible to reach any
   option other than the first one.

3. The number of options available.

##### OPTION

This line indicates a new option begins (and any previous one ends). It has the
following fields:

1. `OPTION`

2. A free-form string describing the option in as much detail as necessary.

##### END

This line signifies that the final option is ending, and normal non-optional
segments begin.

#### Encounter Searching

This lines provide a facility to search for a particular encounter or set of
encounters (e.g. for a grind fight).

##### SEARCH

This line defines that an encounter search is beginning. It has the following
fields:

1. `SEARCH`

2. A string with a description of the search.

3. An expression listing the desired encounter numbers to search for. The
   expression can contain up to 48 terms. The terms can be separated by any of
   three operators: `+` will require that both operands occur. `|` will require
   that one of the two operands occurs. `>` will require that the first operand
   occurs followed by the second operand. The operators can be used in any
   combination. The order of operations is `+`, `|`, followed by `>`. However,
   it is also possible to group them in any desired pattern with parentheses.
   For example, `(2+3)|(5>7)` would require either both encounters 2 and 3 or
   encounter 5 followed by encounter 7. Due to the way these searches are
   processed, certain combinations of values where `+` has operands consisting
   of groups with `|` will not operate as expected. It is recommended to
   refactor these to ensure the `+` operator is the innermost.

4. The new party formation after the conclusion of the fight. This is somewhat
   limited, in that in the case of multiple fights, it might be desirable to
   list a new formation after each. However, this is not currently possible.

##### WAIT

This line signifies that the encounter search is over. This allows a search to
span multiple maps. The final map in a search must have an associated variable
name, as if the encounter has not been found, extra steps will be forced until
it is.

## Authors

* Jason Lynch (Aexoden) <jason@calindora.com>

## Acknowledgements

Many of the original ideas for Rosa came from work done by the_roth and
Myself086 with their own step routing work. This work was originally synthesized
into a previous version of this tool, Spoony.

Eventually, various limitations of the Spoony architecture (primarily its
inability to generate routes that were guaranteed optimal) came to light. Rosa
is the culmination of a near-complete refactoring of the Spoony codebase to
support an optimal solver. Early experiments with the new solver were undertaken
under a separate Rosa project that is now defunct. The optimal solver
incorporates ideas from fcoughlin's independent implementation of a step route
optimizer, ff4step. Additional features to the route definition format have
since been added, necessitating this release of Rosa.

The entire FF4 speedrunning community has been incredibly supportive, not only
by providing information and by helping with research, but also by actually
using the generated routes in runs to help prove their viability and accuracy.
