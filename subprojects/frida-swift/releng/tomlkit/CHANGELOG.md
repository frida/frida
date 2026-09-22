# Change Log

## [0.12.4] - 2024-02-27

### Fixed

- Support `|` and `|=` operator for tables, and support `+` and `+=` operator for arrays. ([#331](https://github.com/sdispater/tomlkit/issues/331))
- Fix an index error when setting dotted keys in a table. ([#332](https://github.com/sdispater/tomlkit/issues/332))

## [0.12.3] - 2023-11-15

### Fixed

- Improve the performance when parsing a table with nested dotted keys. ([#193](https://github.com/sdispater/tomlkit/issues/193))
- Keep the newlines when replacing a table. ([#323](https://github.com/sdispater/tomlkit/issues/323))

## [0.12.2] - 2023-11-02

### Fixed

- Fixed a bug that overwriting a sub table with a plain value raises an error. ([#313](https://github.com/sdispater/tomlkit/issues/313))
- Correct the return type of integer division. ([#312](https://github.com/sdispater/tomlkit/issues/312))

## [0.12.1] - 2023-07-27

### Fixed

- Make float and int hashable.

## [0.12.0] - 2023-07-27

### Added

- Allow users to specify encoders for custom types. ([#296](https://github.com/sdispater/tomlkit/issues/296))

### Fixed

- Fix the incorrect sort when building a table with dotted keys.
- Complete the methods required for integer and float items. ([#307](https://github.com/sdispater/tomlkit/issues/307))
- Replace the deprecated usage of `datetime.utcnow()`. ([#308](https://github.com/sdispater/tomlkit/issues/308))
- Minor performance improvements when iterating over the escape sequences. ([#304](https://github.com/sdispater/tomlkit/issues/304))

## [0.11.8] - 2023-04-27

### Fixed

- Remove the extra indentations added when parsing nested sub-tables. ([#256](https://github.com/sdispater/tomlkit/issues/256))
- Ignore the CRLF immediately following a multiple basic string opening. ([#262](https://github.com/sdispater/tomlkit/issues/262))
- Stringifying subtables and nested tables in arrays of tables. ([#283](https://github.com/sdispater/tomlkit/issues/283))
- Messed table structure when building a table with dotted keys. ([#284](https://github.com/sdispater/tomlkit/issues/284))

## [0.11.7] - 2023-03-27

### Fixed

- Parse empty table name if it is quoted. ([#258](https://github.com/sdispater/tomlkit/issues/258))
- Fix a bug that remove last element of an Inline Table leaves a comma. ([#259](https://github.com/sdispater/tomlkit/issues/259))
- Parse datetime when it is followed by a space. ([#260](https://github.com/sdispater/tomlkit/issues/260))
- Fix the `unwrap()` method for `Container` children values which sometimes returns an internal object if the table is an out-of-order table. ([#264](https://github.com/sdispater/tomlkit/issues/264))
- Fix the wrong return type when doing arithmetic operations between integers and floats. ([#270](https://github.com/sdispater/tomlkit/issues/270))

## [0.11.6] - 2022-10-27

### Fixed

- Allow broader type for toml file path value ([#243](https://github.com/sdispater/tomlkit/issues/243))
- Auto-determine if a table is a super table if not specified explicitly. ([#245](https://github.com/sdispater/tomlkit/issues/245))

## [0.11.5] - 2022-09-28

### Fixed

- Fix the type annotation of `unwrap()` and datetime parsing. ([#229](https://github.com/sdispater/tomlkit/issues/229))
- Clear the existing table header when it is adding to another table. ([#230](https://github.com/sdispater/tomlkit/issues/230))
- Fix a bug that escape chars are lost after concat with another string. ([#235](https://github.com/sdispater/tomlkit/issues/235))
- Fix a rendering issue of tables inside arrays or inline tables. ([#236](https://github.com/sdispater/tomlkit/issues/236))

## [0.11.4] - 2022-08-12

### Fixed

- Fix a memory leak caused by `lru_cache` on methods. ([#227](https://github.com/sdispater/tomlkit/issues/227))

## [0.11.3] - 2022-08-10

### Fixed

- Fix a regression issue that copying an array results in extra `None` items. ([#221](https://github.com/sdispater/tomlkit/issues/221))
- Fix a regression of `array.add_line` that it incorrectly adds a comma to non-value lines. ([#223](https://github.com/sdispater/tomlkit/issues/223))

## [0.11.2] - 2022-08-08

### Fixed

- Fix adding float to an integer value. ([#215](https://github.com/sdispater/tomlkit/issues/215))
- Keep the end-of-array style when adding items to or removing items from an array. ([#213](https://github.com/sdispater/tomlkit/issues/213), [#216](https://github.com/sdispater/tomlkit/issues/216))
- Fix a bug of redundant table header shown when removing children from a super table. ([#217](https://github.com/sdispater/tomlkit/issues/219))

## [0.11.1] - 2022-07-07

### Changed

- Keep consistent line endings when changing files. ([#201](https://github.com/sdispater/tomlkit/issues/201))
- Make `KeyAlreadyPresent` and `InvalidStringError` subclasses of `ParseError`. ([#202](https://github.com/sdispater/tomlkit/issues/202))
- Remove empty table from `OutOfOrderTableProxy` when deleting items. ([#204](https://github.com/sdispater/tomlkit/issues/204))
- Raise errors when trying to access unsupported methods on `OutOfOrderTableProxy`. ([#205](https://github.com/sdispater/tomlkit/issues/205))

### Fixed

- Fix `unwrap()` for String values to remove the quotes. ([#199](https://github.com/sdispater/tomlkit/issues/199))

## [0.11.0] - 2022-05-24

### Added

- Add `unwrap` methods that return tomlkit objects recursively converted to plain python objects. ([#43](https://github.com/sdispater/tomlkit/issues/43))

## [0.10.2] - 2022-04-24

### Fixed

- Use the plain python string representation of `Key` in `KeyAlreadyPresent` error message. ([#185](https://github.com/sdispater/tomlkit/issues/185))
- Fix the `astimezone()` and `replace()` methods of datetime objects. ([#188](https://github.com/sdispater/tomlkit/issues/188))
- Add type definitions for `items()` function. ([#190](https://github.com/sdispater/tomlkit/issues/190))

## [0.10.1] - 2022-03-27

### Fixed

- Preserve the newlines before super tables when rendering. ([#178](https://github.com/sdispater/tomlkit/issues/178))
- Fix the bug that comments are appended with comma when rendering a multiline array. ([#181](https://github.com/sdispater/tomlkit/issues/181))

## [0.10.0] - 2022-02-18

### Fixed

- Fix the only child detection when creating tables. ([#175](https://github.com/sdispater/tomlkit/issues/175))
- Include the `docs/` directory and `CHANGELOG.md` in sdist tarball. ([#176](https://github.com/sdispater/tomlkit/issues/176))

### Added

- Add keyword arguments to `string` API to allow selecting the representation type. ([#177](https://github.com/sdispater/tomlkit/pull/177))

## [0.9.2] - 2022-02-08

### Changed

- When a table's only child is a table or array of table, it is created as a super table. ([#175](https://github.com/sdispater/tomlkit/issues/175))

## [0.9.1] - 2022-02-07

### Fixed

- Fix a bug of separators not being kept when replacing the value. ([#170](https://github.com/sdispater/tomlkit/issues/170))
- Tuples should be dumped as TOML arrays. ([#171](https://github.com/sdispater/tomlkit/issues/171))

## [0.9.0] - 2022-02-01

### Added

- Add a new argument to `table` API to allow it to be a super table. ([#159](https://github.com/sdispater/tomlkit/pull/159))
- Support adding item to `Table` and `Container` with dotted key. ([#160](https://github.com/sdispater/tomlkit/pull/160))

### Fixed

- Fix a bug of `value()` API that parses string incompletely. ([#168](https://github.com/sdispater/tomlkit/pull/168))

## [0.8.0] - 2021-12-20

### Changed

- Drop support for Python<3.6. ([#151](https://github.com/sdispater/tomlkit/pull/151))
- Comply with TOML v1.0.0. ([#154](https://github.com/sdispater/tomlkit/pull/154))

### Fixed

- Support copy protocols for table items. ([#65](https://github.com/sdispater/tomlkit/issues/65))
- Escape characters in double quoted key string. ([#136](https://github.com/sdispater/tomlkit/issues/136))
- Fix the invalid dumping output of multiline array when it is empty. ([#139](https://github.com/sdispater/tomlkit/issues/139))
- Fix a bug that tomlkit accepts an invalid table with missing `=`. ([#141](https://github.com/sdispater/tomlkit/issues/141))
- Fix the invalid dumping output when the key is empty. ([#143](https://github.com/sdispater/tomlkit/issues/143))
- Fix incorrect string returned by dumps when moving/renaming table. ([#144](https://github.com/sdispater/tomlkit/issues/144))
- Fix inconsistent dumps when replacing existing item with nested table. ([#145](https://github.com/sdispater/tomlkit/issues/145))
- Fix invalid dumps output when appending to a multiline array. ([#146](https://github.com/sdispater/tomlkit/issues/146))
- Fix the `KeyAlreadyPresent` when the table is separated into multiple parts. ([#148](https://github.com/sdispater/tomlkit/issues/148))
- Preserve the line endings in `TOMLFile`. ([#149](https://github.com/sdispater/tomlkit/issues/149))

## [0.7.2] - 2021-05-20

### Fixed

- Fixed an error where container's data were lost when copying. ([#126](https://github.com/sdispater/tomlkit/pull/126))
- Fixed missing tests in the source distribution of the package. ([#127](https://github.com/sdispater/tomlkit/pull/127))

## [0.7.1] - 2021-05-19

### Fixed

- Fixed an error with indent for nested table elements when updating. ([#122](https://github.com/sdispater/tomlkit/pull/122))
- Fixed various issues with dict behavior compliance for containers. ([#122](https://github.com/sdispater/tomlkit/pull/122))
- Fixed an internal error when empty tables were present after existing ones. ([#122](https://github.com/sdispater/tomlkit/pull/122))
- Fixed table representation for dotted keys. ([#122](https://github.com/sdispater/tomlkit/pull/122))
- Fixed an error in top level keys handling when building documents programmatically. ([#122](https://github.com/sdispater/tomlkit/pull/122))
- Fixed compliance with mypy by adding a `py.typed` file. ([#109](https://github.com/sdispater/tomlkit/pull/109))

## [0.7.0] - 2020-07-31

### Added

- Added support for sorting keys when dumping raw dictionaries by passing `sort_keys=True` to `dumps()` ([#103](https://github.com/sdispater/tomlkit/pull/103)).

### Changed

- Keys are not longer sorted by default when dumping a raw dictionary but the original order will be preserved ([#103](https://github.com/sdispater/tomlkit/pull/103)).

### Fixed

- Fixed compliance with the 1.0.0rc1 TOML specification ([#102](https://github.com/sdispater/tomlkit/pull/102)).

## [0.6.0] - 2020-04-15

### Added

- Added support for heterogeneous arrays ([#92](https://github.com/sdispater/tomlkit/pull/92)).

## [0.5.11] - 2020-02-29

### Fixed

- Fix containers and our of order tables dictionary behavior ([#82](https://github.com/sdispater/tomlkit/pull/82)))

## [0.5.10] - 2020-02-28

### Fixed

- Fixed out of order tables not behaving properly ([#79](https://github.com/sdispater/tomlkit/pull/79))

## [0.5.9] - 2020-02-28

### Fixed

- Fixed the behavior for out of order tables ([#68](https://github.com/sdispater/tomlkit/pull/68)).
- Fixed parsing errors when single quotes are present in a table name ([#71](https://github.com/sdispater/tomlkit/pull/71)).
- Fixed parsing errors when parsing some table names ([#76](https://github.com/sdispater/tomlkit/pull/76)).

## [0.5.8] - 2019-10-11

### Added

- Added support for producing multiline arrays

## [0.5.7] - 2019-10-04

### Fixed

- Fixed handling of inline tables.

## [0.5.6] - 2019-10-04

### Fixed

- Fixed boolean comparison.
- Fixed appending inline tables to tables.

## [0.5.5] - 2019-07-01

### Fixed

- Fixed display of inline tables after element deletion.

## [0.5.4] - 2019-06-30

### Fixed

- Fixed the handling of inline tables.
- Fixed date, datetime and time handling on Python 3.8.
- Fixed behavior for sub table declaration with intermediate tables.
- Fixed behavior of `setdefault()` on containers (Thanks to [@AndyKluger](https://github.com/AndyKluger)).
- Fixed tables string representation.

## [0.5.3] - 2018-11-19

### Fixed

- Fixed copy of TOML documents.
- Fixed behavior on PyPy3.

## [0.5.2] - 2018-11-09

### Fixed

- Fixed table header missing when replacing a super table's sub table with a single item.
- Fixed comments being displayed in inline tables.
- Fixed string with non-scalar unicode code points not raising an error.

## [0.5.1] - 2018-11-08

### Fixed

- Fixed deletion and replacement of sub tables declared after other tables.

## [0.5.0] - 2018-11-06

### Changed

- Improved distinction between date(time)s and numbers.

### Fixed

- Fixed comma handling when parsing arrays. (Thanks to [@njalerikson](https://github.com/njalerikson))
- Fixed comma handling when parsing inline tables. (Thanks to [@njalerikson](https://github.com/njalerikson))
- Fixed a `KeyAlreadyPresent` error when declaring a sub table after other tables.

## [0.4.6] - 2018-10-16

### Fixed

- Fixed string parsing behavior.

## [0.4.5] - 2018-10-12

### Fixed

- Fixed trailing commas not raising an error for key/value.
- Fixed key comparison.
- Fixed an error when using pickle on TOML documents.

## [0.4.4] - 2018-09-01

### Fixed

- Fixed performances issues while parsing on Python 2.7.

## [0.4.3] - 2018-08-28

### Fixed

- Fixed handling of characters that need escaping when inserting/modifying a string element.
- Fixed missing newline after table header.
- Fixed dict-like behavior for tables and documents.

## [0.4.2] - 2018-08-06

### Fixed

- Fixed insertion of an element after deletion.

## [0.4.1] - 2018-08-06

### Fixed

- Fixed adding an element after another element without a new line.
- Fixed parsing of dotted keys inside tables.
- Fixed parsing of array of tables with same prefix.

## [0.4.0] - 2018-07-23

### Added

- `dumps()` now also accepts a raw dictionary.

### Changed

- `add()`/`append()`/`remove()` now return the current `Container`/`Table` to provide a fluent interface.
- Most items not behave like their native counterparts.

### Fixed

- Fixed potential new lines inside an inline table.

## [0.3.0] - 2018-07-20

### Changed

- Make new dicts automatically sorted when dumped.
- Improved new elements placement when building.
- Automatically convert lists of dicts to arrays of tables.
- No longer add a new line before standalone tables.
- Make arrays behave (mostly) like lists.

### Fixed

- Fixed string parsing when before last char is a backslash character.
- Fixed handling of array of tables after sub tables.
- Fixed table display order.
- Fixed handling of super tables with different sections.
- Fixed raw strings escaping.

[unreleased]: https://github.com/sdispater/tomlkit/compare/0.12.4...master
[0.12.4]: https://github.com/sdispater/tomlkit/releases/tag/0.12.4
[0.12.3]: https://github.com/sdispater/tomlkit/releases/tag/0.12.3
[0.12.2]: https://github.com/sdispater/tomlkit/releases/tag/0.12.2
[0.12.1]: https://github.com/sdispater/tomlkit/releases/tag/0.12.1
[0.12.0]: https://github.com/sdispater/tomlkit/releases/tag/0.12.0
[0.11.8]: https://github.com/sdispater/tomlkit/releases/tag/0.11.8
[0.11.7]: https://github.com/sdispater/tomlkit/releases/tag/0.11.7
[0.11.6]: https://github.com/sdispater/tomlkit/releases/tag/0.11.6
[0.11.5]: https://github.com/sdispater/tomlkit/releases/tag/0.11.5
[0.11.4]: https://github.com/sdispater/tomlkit/releases/tag/0.11.4
[0.11.3]: https://github.com/sdispater/tomlkit/releases/tag/0.11.3
[0.11.2]: https://github.com/sdispater/tomlkit/releases/tag/0.11.2
[0.11.1]: https://github.com/sdispater/tomlkit/releases/tag/0.11.1
[0.11.0]: https://github.com/sdispater/tomlkit/releases/tag/0.11.0
[0.10.2]: https://github.com/sdispater/tomlkit/releases/tag/0.10.2
[0.10.1]: https://github.com/sdispater/tomlkit/releases/tag/0.10.1
[0.10.0]: https://github.com/sdispater/tomlkit/releases/tag/0.10.0
[0.9.2]: https://github.com/sdispater/tomlkit/releases/tag/0.9.2
[0.9.1]: https://github.com/sdispater/tomlkit/releases/tag/0.9.1
[0.9.0]: https://github.com/sdispater/tomlkit/releases/tag/0.9.0
[0.8.0]: https://github.com/sdispater/tomlkit/releases/tag/0.8.0
[0.7.2]: https://github.com/sdispater/tomlkit/releases/tag/0.7.2
[0.7.1]: https://github.com/sdispater/tomlkit/releases/tag/0.7.1
[0.7.0]: https://github.com/sdispater/tomlkit/releases/tag/0.7.0
[0.6.0]: https://github.com/sdispater/tomlkit/releases/tag/0.6.0
[0.5.11]: https://github.com/sdispater/tomlkit/releases/tag/0.5.11
[0.5.10]: https://github.com/sdispater/tomlkit/releases/tag/0.5.10
[0.5.9]: https://github.com/sdispater/tomlkit/releases/tag/0.5.9
[0.5.8]: https://github.com/sdispater/tomlkit/releases/tag/0.5.8
[0.5.7]: https://github.com/sdispater/tomlkit/releases/tag/0.5.7
[0.5.6]: https://github.com/sdispater/tomlkit/releases/tag/0.5.6
[0.5.5]: https://github.com/sdispater/tomlkit/releases/tag/0.5.5
[0.5.4]: https://github.com/sdispater/tomlkit/releases/tag/0.5.4
[0.5.3]: https://github.com/sdispater/tomlkit/releases/tag/0.5.3
[0.5.2]: https://github.com/sdispater/tomlkit/releases/tag/0.5.2
[0.5.1]: https://github.com/sdispater/tomlkit/releases/tag/0.5.1
[0.5.0]: https://github.com/sdispater/tomlkit/releases/tag/0.5.0
[0.4.6]: https://github.com/sdispater/tomlkit/releases/tag/0.4.6
[0.4.5]: https://github.com/sdispater/tomlkit/releases/tag/0.4.5
[0.4.4]: https://github.com/sdispater/tomlkit/releases/tag/0.4.4
[0.4.3]: https://github.com/sdispater/tomlkit/releases/tag/0.4.3
[0.4.2]: https://github.com/sdispater/tomlkit/releases/tag/0.4.2
[0.4.1]: https://github.com/sdispater/tomlkit/releases/tag/0.4.1
[0.4.0]: https://github.com/sdispater/tomlkit/releases/tag/0.4.0
[0.3.0]: https://github.com/sdispater/tomlkit/releases/tag/0.3.0
[0.2.0]: https://github.com/sdispater/tomlkit/releases/tag/0.2.0
