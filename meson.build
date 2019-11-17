project('wavbreaker', 'c',
  version : '0.12',
  default_options : ['warning_level=1'])

subdir('po')

prefix = get_option('prefix')
localedir = get_option('localedir')

glib = dependency('glib-2.0')
gtk3 = dependency('gtk+-3.0', version : '>= 3.22')
ao = dependency('ao')

cc = meson.get_compiler('c')
libm = cc.find_library('m')

deps = [glib, gtk3, ao]

have_mpg123 = false
if get_option('mp3')
  mpg123 = dependency('libmpg123', required : false)
  if mpg123.found()
    have_mpg123 = true
    deps += mpg123
  endif
endif

wavbreaker_sources = [
  'src/about.c',
  'src/aoaudio.c',
  'src/appconfig.c',
  'src/autosplit.c',
  'src/cdda.c',
  'src/cue.c',
  'src/draw.c',
  'src/guimerge.c',
  'src/moodbar.c',
  'src/overwritedialog.c',
  'src/popupmessage.c',
  'src/reallyquit.c',
  'src/sample.c',
  'src/saveas.c',
  'src/toc.c',
  'src/wav.c',
  'src/wavbreaker.c',
]

if get_option('windows_app')
  windows = import('windows')
  wavbreaker_sources += windows.compile_resources('data/wavbreaker.rc')
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
configure_file(output : 'config.h',
               configuration : conf)

if get_option('macos_app')
  install_data('data/wavbreaker.icns', install_dir : 'Contents/Resources')
  install_data('data/Info.plist', install_dir : 'Contents')
else
  install_data('data/wavbreaker.desktop', install_dir: get_option('datadir') + '/applications')
  icon_dir = get_option('datadir') + '/icons/hicolor'
  install_data('data/wavbreaker.svg', install_dir: icon_dir + '/scalable/apps')
  install_data('data/48x48/wavbreaker.png', install_dir: icon_dir + '/48x48/apps')
  install_data('data/24x24/wavbreaker.png', install_dir: icon_dir + '/24x24/apps')
  install_data('data/22x22/wavbreaker.png', install_dir: icon_dir + '/22x22/apps')
  install_data('data/16x16/wavbreaker.png', install_dir: icon_dir + '/16x16/apps')
endif

install_man('man/wavbreaker.1')
install_man('man/wavinfo.1')
install_man('man/wavmerge.1')

executable('wavbreaker',
           wavbreaker_sources,
           dependencies : deps,
           gui_app : true,
           install : true)

executable('wavmerge',
           'src/wavmerge.c',
           'src/wav.c',
           dependencies : [glib],
           install : true)

executable('wavgen',
           'src/wavgen.c',
           'src/wav.c',
           dependencies : [glib, libm],
           install : true)

executable('wavinfo',
           'src/wavinfo.c',
           'src/wav.c',
           dependencies : [glib],
           install : true)
