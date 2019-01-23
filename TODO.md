# Spoony TODO #

The current goal is to write several key important features, rename the project
to ROSA, and then implement any additional features needed for my projects. It's
not a very general-use application.

## 1.0.0 Requirements ##

### Major Changes ###

* Implement a new solver using a dynamic programming paradigm. The current
  engine structure makes this difficult. It may not even be entirely possible,
  but it makes sense in my head at the moment.
* Refactor the code to eliminate oddities (route scores being calculated in
  multiple places, determining when to output modified routes, etc.)

### Minor Changes ###

* Add explicitly named variables, to make referencing a particular variable more
  easy than with the current automatically-indexed ones.

## Future Changes ##

* Add the necessary features to support TAS routes. In particular, it needs to
  be able to handle a potential save and reset on each step on the world map, as
  well as at any save points.
* Store the dynamic programming cache in an SQLite database in order to
  facilitate long-duration runs that will almost certainly be necessary to
  calculate an optimal TAS route. (Even so, it may be infeasible to calculate.)
* Make the reading of data files much safer. It's not very relevant as long as
  I'm the only one writing data files, but it'd be a nice-to-have thing. Whether
  this means simply enforcing data types on the existing files or defining a
  more robust file format, I leave as an exercise to the reader.
* Add automatic twin seed resolutions. This requires computing multiple seeds in
  parallel and somehow deciding the best way to resolve the twin issues.
* Update the defaults for the tuning options to correspond to what's generally
  used in the real world. Most tuning options should probably just go away, as
  an optimal solver makes this redundant.
