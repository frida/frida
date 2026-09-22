# I18n module

This module provides internationalisation and localisation functionality.

## Usage

To use this module, just do: **`i18n = import('i18n')`**. The
following functions will then be available as methods on the object
with the name `i18n`. You can, of course, replace the name `i18n` with
anything else.

### i18n.gettext()

Sets up gettext localisation so that translations are built and placed
into their proper locations during install. Takes one positional
argument which is the name of the gettext module.

* `args`: list of extra arguments to pass to `xgettext` when
  generating the pot file
* `data_dirs`: (*Added 0.36.0*) list of directories to be set for
  `GETTEXTDATADIRS` env var (Requires gettext 0.19.8+), used for local
  its files
* `languages`: list of languages that are to be generated. As of
  0.37.0 this is optional and the
  [LINGUAS](https://www.gnu.org/software/gettext/manual/html_node/po_002fLINGUAS.html)
  file is read.
* `preset`: (*Added 0.37.0*) name of a preset list of arguments,
  current option is `'glib'`, see
  [source](https://github.com/mesonbuild/meson/blob/master/mesonbuild/modules/i18n.py)
  for their value
* `install`: (*Added 0.43.0*) if false, do not install the built translations.
* `install_dir`: (*Added 0.50.0*) override default install location, default is `localedir`

This function also defines targets for maintainers to use:
**Note**: These output to the source directory

* `<project_id>-pot`: runs `xgettext` to regenerate the pot file
* `<project_id>-update-po`: regenerates the `.po` files from current `.pot` file
* `<project_id>-gmo`: builds the translations without installing

(*since 0.60.0*) Returns a list containing:
* a list of built `.mo` files
* the maintainer `-pot` target
* the maintainer `-update-po` target

### i18n.merge_file()

This merges translations into a text file using `msgfmt`. See
[[custom_target]]
for normal keywords. In addition it accepts these keywords:

* `output`: same as `custom_target` but only accepts one item
* `install_dir`: same as `custom_target` but only accepts one item
* `install_tag`: same as `custom_target` but only accepts one item
* `data_dirs`: (*Added 0.41.0*) list of directories for its files (See
  also `i18n.gettext()`)
* `po_dir`: directory containing translations, relative to current directory
* `type`: type of file, valid options are `'xml'` (default) and `'desktop'`
* `args`: (*Added 0.51.0*) list of extra arguments to pass to `msgfmt`

*Added 0.37.0*

### i18n.itstool_join()

This joins translations into a XML file using `itstool`. See
[[custom_target]]
for normal keywords. In addition it accepts these keywords:

* `output`: same as `custom_target` but only accepts one item
* `install_dir`: same as `custom_target` but only accepts one item
* `install_tag`: same as `custom_target` but only accepts one item
* `its_files`: filenames of ITS files that should be used explicitly
  (XML translation rules are autodetected otherwise).
* `mo_targets` *required*: mo file generation targets as returned by `i18n.gettext()`.

*Added 0.62.0*
