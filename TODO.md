# Rosa TODO

## 1.1.0

* The dynamic programming cache currently requires large amounts of memory for
  more complicated routes. Optionally allow either a fixed-size cache or to
  store the cache on disk.

## 1.2.0

* Add the necessary features to support TAS routes. In particular, handle a
  potential save and reset on each step on the world map, as well as at any save
  points.

## Future

* Make the reading of data files much safer. It's not very relevant as long as
  I'm the only one writing data files, but it'd be a nice-to-have thing. Whether
  this means simply enforcing data types on the existing files or defining a
  more robust file format, I leave as an exercise to the reader.

* Allow for a more computer-friendly JSON output format.

* Add automatic twin seed resolutions. This requires computing multiple seeds in
  parallel and somehow deciding the best way to resolve the twin issues.

* Allow for sequential encounter searches. The current code simply lumps them
  all into one bucket and ensures that the correct number of each is
  encountered, but doesn't enforce the ordering. No vanilla route would use this
  feature at this time, but it's possible a route for a romhack would. To be
  truly useful, it would also need to allow for specifying a new party after
  each fight.

* Handle the extra penalty for single extra steps done at save points.

* Handle the ability to take three extra steps in Cecil's room with only a two
  tile penalty. This is similar to the potential single extra step before
  entering Bab-il, which can be done with a one tile penalty, versus most single
  extra steps which require a 2 tile penalty.

* See if some seeds could benefit from a save/reset cycle instead of endlessly
  pacing until they find the grind fight. The problem is that your reset seed is
  effectively random, so it'd be completely up to chance.
