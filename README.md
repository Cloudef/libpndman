libpndman
=========

Package management library for PND files.
----------------------------------------
[usage][]

#### BUILDING:

    mkdir target && cd target                # - create build target directory
    cmake -DCMAKE_INSTALL_PREFIX=build ..    # - run CMake, set install directory
    make                                     # - compile

#### DONE:
*  CLI client milkyhelper

#### TODO:
*  man pages
*  pnd utils for data extraction.
*  ncurses client(?)

####  Projects using libpndman:
*  [qtpndman][] - Qt wrapper for libpndman
*  [panorama][] - Qt frontend for libpndman
*  [pypndman][] - Python wrapper for libpndman

[panorama]: https://github.com/bzar/panorama
[qtpndman]: https://github.com/bzar/qtpndman
[pypndman]: https://github.com/Tempel/pypndman

[usage]: https://github.com/Cloudef/libpndman/blob/master/test/sample.c
