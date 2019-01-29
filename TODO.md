# Spoony TODO #

The current goal is to write several key important features, rename the project
to ROSA, and then implement any additional features needed for my projects. It's
not a very general-use application.

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
* Allow for a more computer-friendly JSON output format.
* Add automatic twin seed resolutions. This requires computing multiple seeds in
  parallel and somehow deciding the best way to resolve the twin issues.
* Update the defaults for the tuning options to correspond to what's generally
  used in the real world. Most tuning options should probably just go away, as
  an optimal solver makes this redundant.
* Allow for sequential encounter searches. The current code simply lumps them
  all into one bucket and ensures that the correct number of each is
  encountered, but doesn't enforce the ordering. No vanilla route would use this
  feature at this time, but it's possible a route for a romhack would. To be
  truly useful, it would also need to allow for specifying a new party after
  each fight.
* Handle the extra penalty for single extra steps done at save points.
* Handle the ability to take three extra steps in Cecil's room with only a two
  tile penalty.
* See if some seeds could benefit from a save/reset cycle instead of endlessly
  pacing until they find the grind fight. The problem is that your reset seed is
  effectively random, so it'd be completely up to chance.
