# Spoony

Spoony is a tool to generate step routes for Final Fantasy IV. Step routes are
specific walking patterns designed to manipulate the timing and details of
random encounters. They are primarily used for speedrunning the game. Additional
details can be found at [ff4.aexoden.com](https://ff4.aexoden.com).

## Usage

`src/spoony [OPTION...]`

No options are strictly required, as there are sane defaults for all options,
but some minimal options should be set to get any sort of real use out of the
program.

### Options

#### `-r, --route`

Specifies the route definition to generate a step route for. This should be a
file of the form `data/routes/%ROUTE%.txt` where %ROUTE% corresponds to the
given route name.

#### `-s, --seed`

Specifies which seed to generate the step route for. This should be an integer
from 0 to 255 (inclusive).

#### `-m, --maximum-steps`

Specifies the maximum number of extra steps a route can take at a given node.

#### `-a, --algorithm`

Specifies the algorithm used to optimize the route. While there are a few
algorithms available, most of it is historical in nature, and there is almost
no good reason at this time to use any algorithm other than `ils+pair` or `none`
(if no optimization is required).

#### `-o, --output-directory`

Specifies where the generated route will be outputted to. The default is
`output/routes`.

#### `-d, --output-result`

Always output the result to standard output.

#### `-v, --variables`

Specifies values for the given variable indices. The format is `a:b c:d...`
where `a` and `c` are indices and `b` and `d` are values.

#### `-f, --full-optimization`

Normally, if `-v` is specified, the optimizer will only optimize the variables
after the last given index. With this option, the entire route is optimized
using the given variables as a starting point.

#### `-l, --load-existing-variables`

Load existing values from the output location. This allows the optimizer to
resume optimizing an already existing route.

#### `-i, --maximum-iterations`

The maximum number of iterations before the program will exit.

#### `-z, --pairwise-shift`

Alters the `pair` algorithm (both alone and when used in conjunction with others
such as in `ils+pair`) to shift steps around rather than testing all
possibilities. While this does reduce its efficacy somewhat, it's also
currently impractical to use the `pair` algorithm without it.

#### `-p, --perturbation-strength`

When using ILS, this determines the strength of the perturbations performed. A
recommended value is approximately 64.

#### `-w, --perturbation-wobble`

When using ILS, this determines how much the total number of steps can be
adjusted during the perturbation phase. A recommended value is 128.

#### `-x, --step-output`

Outputs detailed information per step.

#### `-t, --tas-mode`

Adjusts the calculations to be appropriate for TAS routing. This option is
currently not very useful and is primarily intended for future use.

#### `-c, --maximum-comparisons`

Controls how many variables away the `pair` algorithm will search.

## Building

Spoony is primarily designed to be run from within the main repository
directory. To build the executable, enter that main directory and execute
`./build`. It expects certain data text files to be in particular locations
relative to the current working directory.
