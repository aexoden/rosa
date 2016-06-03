# Spoony

Spoony is an application to generate step routes for the SNES/SFC release of
Final Fantasy IV. The included data currently only supports the original U.S.
release (released as Final Fantasy II), though it should be able to support any
game using the FF4 engine with relative ease.

## Prequisites

Spoony may or may not build in your particular build environment. The code
should be standard C++14 and should compile on any standards-compliant compiler.
However, the build is currently only tested on Clang 3.8 with the Gold linker,
and the included build machinery assumes as much.

In addition, a few third-party libraries are required:

* [boost](http://www.boost.org)
* [gtkmm](http://www.gtkmm.org)
* [tclap](http://tclap.sourceforge.net)

## Building

TODO

## Usage

TODO

## Authors

* Jason Lynch (Aexoden) <jason@calindora.com>

## Acknowledgements

Many of the original ideas for this application came from work done by Myself086
with his own FF4 step routing work. In addition, several ideas on implementing
a better optimizer have come from both Myself086 and fcoughlin.

The entire FF4 speed running community has been nothing but supportive by
providing information and helping with research.

## License

Spoony is licensed under the [MIT License](https://opensource.org/licenses/MIT).
