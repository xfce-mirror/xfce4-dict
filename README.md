[![License](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://gitlab.xfce.org/apps/xfce4-dict/-/blob/master/COPYING)

# Xfce4 Dictionary

A client program to query different dictionaries.

This program allows you to search different kinds of dictionary services for words or phrases and shows you the result.
Currently you can query a "Dict" server(RFC 2229), any online dictionary service by opening a web browser or search for words using a spell check program like aspell, ispell or enchant.
It's also possible to query Large Language Models running via Ollama.

xfce4-dict contains a stand-alone application called "xfce4-dict" and a panel plugin for the Xfce panel.

----

### Homepage

[Xfce4 Dictionary documentation](https://docs.xfce.org/apps/xfce4-dict/start)

### Changelog

See [NEWS](https://gitlab.xfce.org/apps/xfce4-dict/-/blob/master/NEWS) for details on changes and fixes made in the current release.


### Required Packages 

Xfce4 Dictionary depends on the following packages:

- [GLib](https://wiki.gnome.org/Projects/GLib) >= 2.66.0
- [GTK](https://www.gtk.org) >= 3.24.0
- [libxfce4ui](https://gitlab.xfce.org/xfce/libxfce4ui) >= 4.18.0
- [libxfce4util](https://gitlab.xfce.org/xfce/libxfce4util) >= 4.18.0
- [libxfce4panel](https://gitlab.xfce.org/xfce/xfce4-panel) >= 4.18.0

### Source Code Repository

[Xfce4 Dictionary source code](https://gitlab.xfce.org/apps/xfce4-dict)

### Download a Release Tarball

[Xfce4 Dictionary archive](https://archive.xfce.org/src/apps/xfce4-dict)
    or
[Xfce4 Dictionary tags](https://gitlab.xfce.org/apps/xfce4-dict/-/tags)

### Installation

From source code repository: 

    % cd xfce4-dict
    % meson setup build
    % meson compile -C build
    % meson install -C build

From release tarball:

    % tar xf xfce4-dict-<version>.tar.xz
    % cd xfce4-dict-<version>
    % meson setup build
    % meson compile -C build
    % meson install -C build

### Reporting Bugs

Visit the [reporting bugs](https://docs.xfce.org/apps/xfce4-dict/bugs) page to view currently open bug reports and instructions on reporting new bugs or submitting bugfixes.
