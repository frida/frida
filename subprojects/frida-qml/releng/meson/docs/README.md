# Meson Documentation

## Build dependencies

Meson uses itself and [hotdoc](https://github.com/hotdoc/hotdoc) for generating documentation.

Minimum required version of hotdoc is *0.8.9*.

Instructions on how to install hotdoc are [here](https://hotdoc.github.io/installing.html).

Our custom hotdoc extensions require:
- [chevron](https://pypi.org/project/chevron)
- [strictyaml](https://pypi.org/project/strictyaml)

## Building the documentation

From the Meson repository root dir:
```
$ cd docs/
$ meson setup built_docs/
$ ninja -C built_docs/
```
Now you should be able to open the documentation locally
```
built_docs/Meson documentation-doc/html/index.html
```

## Upload

Meson uses the git-upload hotdoc plugin which basically
removes the html pages and replaces with the new content.

You can simply run:
```
$ ninja -C built_docs/ upload
```
