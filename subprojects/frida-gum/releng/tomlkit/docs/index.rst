.. TOMLKit documentation master file, created by
   sphinx-quickstart on Fri Dec 24 09:31:46 2021.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

TOML Kit
========

    **Style-preserving TOML library for Python**

.. image:: https://img.shields.io/pypi/v/tomlkit.svg?logo=python&logoColor=white
    :target: https://pypi.org/project/tomlkit/
.. image:: https://img.shields.io/pypi/pyversions/tomlkit.svg?logo=python&logoColor=white
    :target: https://pypi.org/project/tomlkit/
.. image:: https://img.shields.io/github/license/sdispater/tomlkit.svg?logo=github&logoColor=white
   :target: https://github.com/sdispater/tomlkit/blob/master/LICENSE
.. image:: https://img.shields.io/badge/TOML-1.0.0-9c4221
   :target: https://toml.io/en/v1.0.0

TOML Kit is a **1.0.0-compliant** `TOML <https://toml.io/>`_ library.

It includes a parser that preserves all comments, indentations, whitespace and internal element ordering,
and makes them accessible and editable via an intuitive API.

You can also create new TOML documents from scratch using the provided helpers.

Part of the implementation as been adapted, improved and fixed from `Molten <https://github.com/LeopoldArkham/Molten>`_.

Installation
------------

If you are using `Poetry <https://poetry.eustace.io>`_,
add ``tomlkit`` to your ``pyproject.toml`` file by using::

    poetry add tomlkit

If not, you can use ``pip``::

    pip install tomlkit


Contents
--------

.. toctree::
   :maxdepth: 2

   quickstart
   api


Indices and tables
==================

* :ref:`genindex`
* :ref:`modindex`
* :ref:`search`
