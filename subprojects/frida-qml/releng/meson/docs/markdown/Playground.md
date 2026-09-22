# playground

This page is *not* part of official documentation. It exists merely
for testing new stuff for the wiki.

## Ref manual reformat

The current format is not very readable. We should have something more
like what
[glib](https://developer.gnome.org/glib/stable/glib-Hash-Tables.html)
or [Python](https://docs.python.org/3/library/os.html) do.

Here's a first proposal.

    project(<project name>,
            <languages to use, comma separated>,
            version         : <project version>,
            subproject_dir  : <alternative directory to store subprojects>,
            meson_version   : <required version of Meson>,
            license         : <string or array of licenses>,
            default_options : <default values for project options>,

Longer descriptions of arguments go here.

Take two:

## project

    <project name>
    <languages to use, comma separated>
    version         : <project version>
    subproject_dir  : <alternative directory to store subprojects>
    meson_version   : <required version of Meson>
    license         : <string or array of licenses>
    default_options : <default values for project options>
