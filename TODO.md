# Spoony Experimental TODO List

* Reduce or eliminate dependency on Glib.

* Rewrite route definition format as needed. Break useful segments into reusable
  units. (May reduce or eliminate the need for the python route generator.) All
  variables should be defined with consistent names. Decouple allowing the
  optimizer from taking extra steps in an area from allowing extra steps in that
  area whatsoever. (Extra steps should be allowed everywhere. We may choose to
  restrict where the optimizer can take steps when generating routes.)

* Implement a first-pass optimizer to combine segments with identical statistics
  (either both having an encounter rate of 0, or having the same encounter rate
  and same encounter group).

* Update encounter data to include different timings for different party levels
  as required. Research the possibility of allowing for different timings for
  different party constituencies. (This portion may be best solved by allowing
  routes to specify a timing set, and we can simply define separate timing sets
  for alternate routes where necessary.)

* Implement a new optimizer based on dynamic programming. May or may not
  actually be feasible without relaxing constraints.

* Ensure single extra steps at save points are treated as more costly (due to
  the ensuing text box).

* Determine a feasible way to handle taking three extra steps at the cost of two
  (a move possible at the very least in Cecil's bedroom, and perhaps in other
  places).

* Determine if some seeds could benefit by not forcing a grind fight, but
  instead planning a deliberate save/reset cycle at some point to get a better
  seed.

* Implement constraints on variables. (This could potentially be used to solve
  the identical seeds problem).
