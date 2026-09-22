These are the spec tests for TOML used by @iarna/toml.

The errors folder contains TOML files that should cause a parser to report an error.

The values folder contains TOML files and paired YAML or JSON files.  The
YAML files should parse to a structure that's deeply equal to the TOML
structure.  The JSON files match the patterns found in [BurntSushi 0.4 TOML
tests](https://github.com/BurntSushi/toml-test#json-encoding).

We introduce the following new types to match TOML 0.5.0:

* _datetime-local_ - A datetime without a timezone. Floating.
* _date_ - A date without any time component
* _time_ - A time without any date component
