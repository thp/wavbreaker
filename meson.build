project('wavbreaker', 'c',
  version : '0.16',
  default_options: [
    'c_std=c99',
    'warning_level=1',
  ],
)

subdir('po')

prefix = get_option('prefix')
localedir = get_option('localedir')

glib = dependency('glib-2.0')
gtk3 = dependency('gtk+-3.0', version : '>= 3.22')
ao = dependency('ao')
libcue = dependency('libcue')

cc = meson.get_compiler('c')
libm = cc.find_library('m')

core_deps = [glib, libm, libcue]
gui_deps = [gtk3]
ao_deps = [ao]
format_deps = []

have_mpg123 = false
if get_option('mp3')
  mpg123 = dependency('libmpg123', required : false)
  if mpg123.found()
    have_mpg123 = true
    format_deps += mpg123
  endif
endif

have_vorbisfile = false
if get_option('ogg_vorbis')
  vorbisfile = dependency('vorbisfile', required : false)
  if vorbisfile.found()
    have_vorbisfile = true
    format_deps += vorbisfile
  endif
endif

shared_sources = [
  'src/appinfo.c',
  'src/aoaudio.c',
  'src/sample.c',

  'src/list.c',
  'src/track_break.c',
  'src/appconfig.c',
  'src/cue.c',
  'src/toc.c',
  'src/txt.c',

  'src/format.c',
  'src/format_wav.c',
  'src/format_cdda_raw.c',
  'src/format_mp3.c',
  'src/format_ogg_vorbis.c',
]

gui_sources = [
  'src/about.c',
  'src/appconfig_gtk.c',
  'src/autosplit.c',
  'src/draw.c',
  'src/guimerge.c',
  'src/moodbar.c',
  'src/overwritedialog.c',
  'src/popupmessage.c',
  'src/reallyquit.c',
  'src/saveas.c',
  'src/wavbreaker.c',
]

cli_sources = [
  'src/cli.c',
  'src/wavmerge.c',
  'src/wavgen.c',
  'src/wavinfo.c',
]

if get_option('windows_app')
  windows = import('windows')
  gui_sources += windows.compile_resources('data/wavbreaker.rc')
endif

add_project_arguments(['-D_FILE_OFFSET_BITS=64', '-D_LARGEFILE_SOURCE',
  '-DGTK_DISABLE_SINGLE_INCLUDES', '-DGSEAL_ENABLE',
  '-DGDK_DISABLE_DEPRECATED', '-DGTK_DISABLE_DEPRECATED'], language : 'c')

conf = configuration_data()
# Surround the version in quotes to make it a C string
conf.set_quoted('VERSION', meson.project_version())
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('LOCALEDIR', join_paths(prefix, localedir))
conf.set('WANT_MOODBAR', get_option('moodbar'))
conf.set('HAVE_MPG123', have_mpg123)
conf.set('HAVE_VORBISFILE', have_vorbisfile)
configure_file(output : 'config.h',
               configuration : conf)

meta = configuration_data()
meta.set('VERSION', meson.project_version())
info_plist = configure_file(output : 'Info.plist',
                            input : 'data/Info.plist.in',
                            configuration : meta)

if get_option('macos_app')
  install_data('data/wavbreaker.icns', install_dir : 'Contents/Resources')
  install_data(info_plist, install_dir : 'Contents')
else
  install_data('data/net.sourceforge.wavbreaker.desktop', install_dir: get_option('datadir') + '/applications')
  install_data('data/net.sourceforge.wavbreaker.appdata.xml', install_dir: get_option('datadir') + '/metainfo')
  icon_dir = get_option('datadir') + '/icons/hicolor'
  install_data('data/net.sourceforge.wavbreaker.svg', install_dir: icon_dir + '/scalable/apps')
endif

install_man('man/wavbreaker.1')
install_man('man/wavcli.1')

# For now this is a static library for internal linking with the GUI and CLI.
# Once the API/ABI is stable, we can turn it into a installed shared library.
libwavbreaker = static_library('wavbreaker',
           shared_sources,
           dependencies : core_deps + ao_deps + format_deps)

executable('wavbreaker',
           gui_sources,
           dependencies : core_deps + gui_deps,
           link_with : libwavbreaker,
           gui_app : true,
           install : true)

executable('wavcli',
           cli_sources,
           dependencies : core_deps,
           link_with : libwavbreaker,
           install : true)
