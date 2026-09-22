# Using the WrapDB

The Wrap database is a web service that provides Meson build
definitions to projects that do not have it natively. Using it is
simple. The service can be found
[here](https://wrapdb.mesonbuild.com).

The front page lists all projects that are on the service. Select the
one you want and click it. The detail page lists available versions by
branch and revision id. The branch names come from upstream releases
and revision ids are version numbers internal to the database.
Whenever the packaging is updated a new revision is released to the
service a new revision with a bigger revision id is added. Usually you
want to select the newest branch with the highest revision id.

You can get the actual wrap file which tells Meson how to fetch the
project by clicking on the download link on the page. As an example,
the wrap file for [zlib-1.2.8, revision
4](https://wrapdb.mesonbuild.com/v1/projects/zlib/1.2.8/4/get_wrap)
looks like this. You can find detailed documentation about it in [the
Wrap manual](Wrap-dependency-system-manual.md).

    [wrap-file]
    directory = zlib-1.2.8

    source_url = http://zlib.net/zlib-1.2.8.tar.gz
    source_filename = zlib-1.2.8.tar.gz
    source_hash = 36658cb768a54c1d4dec43c3116c27ed893e88b02ecfcb44f2166f9c0b7f2a0d

    patch_url = https://wrapdb.mesonbuild.com/v1/projects/zlib/1.2.8/4/get_zip
    patch_filename = zlib-1.2.8-4-wrap.zip
    patch_hash = 2327a42c8f73a4289ee8c9cd4abc43b324d0decc28d6e609e927f0a50321af4a

Add this file to your project with the name `subprojects/zlib.wrap`.
Then you can use it in your `meson.build` file with this directive:

    zproj = subproject('zlib')

When Meson encounters this it will automatically download, unpack and
patch the source files.

## Contributing build definitions

The contents of the Wrap database are tracked in git repos of the
[Mesonbuild project](https://github.com/mesonbuild). The actual
process is simple and described in [submission
documentation](Adding-new-projects-to-wrapdb.md).
