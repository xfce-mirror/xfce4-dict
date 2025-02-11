dict_sources = [
  'popup_plugin.c',
  'popup_plugin.h',
  'xfce4-dict.c',
]

dict = executable(
  'xfce4-dict',
  dict_sources,
  sources: xfce_revision_h,
  include_directories: [
    include_directories('..', '..' / 'lib'),
  ],
  dependencies: [
    glib,
    gthread,
    gtk,
    libxfce4util,
    libxfce4ui,
  ],
  link_with: [
    libdict,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('bindir'),
)

i18n.merge_file(
    input: 'xfce4-dict.desktop.in',
    output: 'xfce4-dict.desktop',
    po_dir: '..' / 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / 'applications',
)
