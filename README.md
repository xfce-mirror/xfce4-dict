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

---

## "xfce4-dict" command

If the panel plugin is loaded, the xfce4-dict command just opens the already loaded main window of the panel plugin (replacement for the former "xfce4-popup-dict" command).
If the panel plugin is not loaded, xfce4-dict opens the application normally.

xfce4-dict understand a few command line options, for details read its manpage or call: "xfce4-dict --help".

It can also be used to bind the dict plugin actions to a keyboard shortcut.
You can add a new keyboard command with the Keyboard settings plugin in Xfce's settings manager and assign "xfce4-dict" as the action command.

#### Please note
There is a limitation of max. 12 characters in passing a search term to xfce4-dict when the panel plugin is loaded.
That is, if you pass a search term as command line argument(s) to xfce4-dict which is longer than 12 characters, it is truncated.
To be exact, the limit is 12 bytes so if the search term contains any non-ASCII characters it might be even less than 12 characters.
To work around this limitation, you can add the command line '-i' so that xfce4-dict will start a single stand-alone application.
Then all passed text is used as search term.

## Panel Plugin

With the panel plugin enabled, you can also easily select a word in an email or on a web page and drag it onto the dict icon in your panel, then
the plugin begins to search and shows you the results.

The plugin also provides a text field within the panel to directly enter text to search for. To start the search simply press the Enter key in the text field.

## Query a Dict server

You can query a dictionary server(see RFC 2229) to search for the translation or explanation of a word. You can also choose a dictionary offered by the server to improve your search results.
There are two special dictionaries:
- `*` - use this dictionary to search in all available dictionaries on the server
- `!` - use this dictionary to search in all available dictionaries on the server but stop searching after the first match.

This program was mainly tested with the server "dict.org" but should work with any other servers which implement the DICT protocol defined in RFC 2229.

### Search result highlighting

Sometimes definitions in certain dictionaries contain special markups to give additional information. Two of them are:
- Cross-references
  - These are some kind of links in definitions to link to other definitions.
  - Xfce4-dict will highlight these definitions and make them click-able to easily jump (by searching) to this highlighted definition.
- Phonetic hints
  - These are mostly phonetic spellings found in translation dictionaries to illustrate how a word is pronounced.
  - Xfce4-dict will highlight these with a green colour.

### Local dictionary server

Instead of using remote dictionary servers like dict.org which always need a network connection and then still might be slow, you can also run your own dictionary server on your machine.
This way you always have fast access and you can install all the dictionaries you need.

Most distributions provide a package called "dictd" which contains the server.
Additionally you should install some dictionaries you need like WordNet, Jargon or some translation dictionaries.
On Debian and Ubuntu-like systems these packages are called "dict-wn" (WordNet), dict-jargon, dict-freedict-deu-eng, dict-freedict-eng-fra or similar (on other distributions the dictionary packages might be prefixed with "dictd-").
Basically it is enough to install the "dictd" packages along with some dictionaries to get a running local server.
In the Xfce4-dict preferences dialog, simply use "localhost" as the server name.

If you need more information about setting up a local dictionary server,
please see http://docs.kde.org/kde3/en/kdenetwork/kdict/dictd-mini-howto.html.

## Web-based dictionaries

Searching web-based dictionary services like dict.leo.org or other ones is also supported by passing the search word as a URL argument and opening the URL in your web browser.
The URL can be configured in the preferences dialog.

A note on the started web browser:
xfce4-dict will open the configured URL with the "xdg-open" command which will open the configured default browser.
If that doesn't work other known browsers are tried.
But it's better to set a default browser using "Preferred Applications" in the Xfce settings manager.

## Spell checking based search

It is also possible to verify the spelling of word using spell checking programs like enchant or aspell (or its predecessor ispell).
To get this working you need to have the enchant, aspell or ispell binary in your binary search path and at least one dictionary working.
If you have multiple dictionaries installed, you can select the one to use in the preferences dialog.

## Speed Reader

With the builtin Speed Reader you can train and improve your skills in fast reading texts.
You just specify a text, define how many words should be displayed at once (word grouping) and how many words should be displayed per minute (speed rate). Then the words of the specified text are displayed so you can read them.
The higher the speed rate is the faster you can read.

## Large Language Model

It is possible to retrieve definitions from Large Language Models (LLMs) running via [Ollama](https://ollama.com/) or any other implementation that supports its HTTP API.
The quality of the results may vary depending on the model used.
In general, larger models require more memory and are slower to respond, but they tend to produce more accurate results.
