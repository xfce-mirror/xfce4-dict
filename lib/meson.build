libdict_sources = [
  'common.c',
  'common.h',
  'dictd.c',
  'dictd.h',
  'gui.c',
  'gui.h',
  'libdict.h',
  'prefs.c',
  'prefs.h',
  'speedreader.c',
  'speedreader.h',
  'spell.c',
  'spell.h',
]

if get_option('llm')
  libdict_sources += [
    'llm.c',
    'llm.h',
  ]
endif

libdict_sources += gnome.gdbus_codegen(
  'dbus',
  sources: 'dbus.xml',
  interface_prefix : 'org.xfce',
)

libdict_sources += gnome.compile_resources(
  'resources',
  'resources.xml',
  c_name: 'dict',
  export: false,
  source_dir: '..' / 'icons' / 'scalable'
)

libdict = static_library(
  'dict',
  libdict_sources,
  sources: xfce_revision_h,
  include_directories: [
    include_directories('..'),
  ],
  dependencies: [
    glib,
    gthread,
    gtk,
    libxfce4ui,
    libxfce4util,
  ],
  install: false,
)
