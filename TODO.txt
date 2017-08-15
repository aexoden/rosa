# Spoony TODO #

At this time, I intend for spoony to ultimately be superseded by ROSA, a newer
route generator based around a new optimal solver. However, spoony appears to
have some life left in it. Implementing the following will help to extend that
life.

## Major Changes ##

* Implement a new solver using a dynamic programming paradigm. The current
  engine structure makes this difficult.
* Reorganize the code to eliminate some of the oddities (route scores being
  calculated in multiple places, determining when to output modified routes,
  etc.)

## Minor Changes ##

* Finish adding the encounter timing data and convert all useful routes to use
  that data.
* Add automatic twin seed resolutions. This requires computing multiple seeds in
  parallel and somehow deciding the best way to resolve the twin issues.
* Add the ability to name variables, so the web data can easily use the routes
  without having to parse the text output.
