Minetest Mapper C++
===================

.. image:: https://github.com/minetest/minetestmapper/workflows/build/badge.svg
    :target: https://github.com/minetest/minetestmapper/actions/workflows/build.yml

Minetestmapper generates a top-down overview image from a Luanti map.

A port of minetestmapper.py to C++ from `the obsolete Python script
<https://github.com/minetest/minetest/tree/0.4.17/util>`_.
This version is both faster and provides more features.

Minetestmapper ships with a colors.txt file suitable for Minetest Game,
if you use a different game or have mods installed you should generate a
matching colors.txt for better results (colors will be missing otherwise).
The `generate_colorstxt.py script 
<./util/generate_colorstxt.py>`_ in the util folder exists for this purpose,
detailed instructions can be found within.

Requirements
------------

* C++ compiler, zlib, zstd
* libgd
* sqlite3
* LevelDB (optional)
* hiredis (optional)
* Postgres libraries (optional)

on Debian/Ubuntu:
^^^^^^^^^^^^^^^^^

``sudo apt install cmake libgd-dev libhiredis-dev libleveldb-dev libpq-dev libsqlite3-dev zlib1g-dev libzstd-dev``

on openSUSE:
^^^^^^^^^^^^

``sudo zypper install gd-devel hiredis-devel leveldb-devel postgresql-devel sqlite3-devel zlib-devel libzstd-devel``

for Windows:
^^^^^^^^^^^^
Minetestmapper for Windows can be downloaded `from the Releases section
<https://github.com/minetest/minetestmapper/releases>`_.

After extracting the archive, it can be invoked from cmd.exe or PowerShell:

.. code-block:: dos

	cd C:\Users\yourname\Desktop\example\path
	minetestmapper.exe --help

Compilation
-----------

.. code-block:: bash

    cmake . -DENABLE_LEVELDB=1
    make -j$(nproc)

Usage
-----

``minetestmapper`` has two mandatory paremeters, ``-i`` (input world path)
and ``-o`` (output image path).

::

    ./minetestmapper -i ~/.minetest/worlds/my_world/ -o map.png


Parameters
^^^^^^^^^^

bgcolor:
    Background color of image, e.g. ``--bgcolor '#ffffff'``

scalecolor:
    Color of scale marks and text, e.g. ``--scalecolor '#000000'``

playercolor:
    Color of player indicators, e.g. ``--playercolor '#ff0000'``

origincolor:
    Color of origin indicator, e.g. ``--origincolor '#ff0000'``

drawscale:
    Draw scale(s) with tick marks and numbers, ``--drawscale``

drawplayers:
    Draw player indicators with name, ``--drawplayers``

draworigin:
    Draw origin indicator, ``--draworigin``

drawalpha:
    Allow nodes to be drawn with transparency (such as water), ``--drawalpha``

extent:
    Don't output any imagery, just print the extent of the full map, ``--extent``

noshading:
    Don't draw shading on nodes, ``--noshading``

noemptyimage:
    Don't output anything when the image would be empty, ``--noemptyimage``

verbose:
    Enable verbose log putput, ``--verbose``

min-y:
    Don't draw nodes below this Y value, e.g. ``--min-y -25``

max-y:
    Don't draw nodes above this Y value, e.g. ``--max-y 75``

backend:
    Override auto-detected map backend; supported: *sqlite3*, *leveldb*, *redis*, *postgresql*, e.g. ``--backend leveldb``

geometry:
    Limit area to specific geometry (*x:z+w+h* where x and z specify the lower left corner), e.g. ``--geometry -800:-800+1600+1600``

    The coordinates are specified with the same axes as in-game. The Z axis becomes Y when projected on the image.

zoom:
    Zoom the image by using more than one pixel per node, e.g. ``--zoom 4``

colors:
    Override auto-detected path to colors.txt, e.g. ``--colors ../world/mycolors.txt``

scales:
    Draw scales on specified image edges (letters *t b l r* meaning top, bottom, left and right), e.g. ``--scales tbr``

exhaustive:
    Select if database should be traversed exhaustively or using range queries, available: *never*, *y*, *full*, *auto*

    Defaults to *auto*. You shouldn't need to change this, as minetestmapper tries to automatically picks the best option.

dumpblock:
    Instead of rendering anything try to load the block at the given position (*x,y,z*) and print its raw data as hexadecimal.
