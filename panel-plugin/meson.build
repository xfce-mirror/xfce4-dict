plugin_sources = [
  'xfce4-dict-plugin.c'
]

plugin_install_subdir = 'xfce4' / 'panel' / 'plugins'

shared_module(
  'xfce4dict',
  plugin_sources,
  gnu_symbol_visibility: 'hidden',
  include_directories: [
    include_directories('..', '..' / 'lib'),
  ],
  dependencies: [
    glib,
    gthread,
    gtk,
    libxfce4util,
    libxfce4ui,
    libxfce4panel,
  ],
  link_with: [
    libdict,
  ],
  install: true,
  install_dir: get_option('prefix') / get_option('libdir') / plugin_install_subdir,
)

i18n.merge_file(
    input: 'xfce4-dict-plugin.desktop.in',
    output: 'xfce4-dict-plugin.desktop',
    po_dir: '..' / 'po',
    type: 'desktop',
    install: true,
    install_dir: get_option('prefix') / get_option('datadir') / plugin_install_subdir,
)
