project('4jcraft-chucklegrounds', ['cpp', 'c'],
  version : '0.1.0',
  default_options : [
    'buildtype=debug', # for now
    'unity=on', # merge source files per target
    'unity_size=8', # TODO: mess around with this
    'b_pch=true', # precompiled headers
  ],
)

pymod = import('python')
python = pymod.find_installation('python3', required: true)

cc = meson.get_compiler('cpp')

# system deps
if host_machine.system() == 'emscripten'
    # These are handled in Emscripten via -sUSE_<whatever>
    gl_dep  = declare_dependency()
    glu_dep = declare_dependency()
    sdl2_dep = declare_dependency()
    thread_dep = declare_dependency()
    zlib_dep   = declare_dependency()
else
    gl_dep  = dependency('gl')
    glu_dep = dependency('glu')
    sdl2_dep   = dependency('sdl2') # Yes.. i know sdl3 is out, but there's not point upgrading right now except when
                                # someone is gonna ask me "Hey juicey can you make it SDL3?" and i'd be like fuck you and still do it.
    thread_dep = dependency('threads')
    zlib_dep = dependency('zlib')
endif

# compile flags (chagne ts juicey)
global_cpp_args = [
  '-fpermissive',
  '-Wshift-count-overflow',
  '-pipe',             # use pipes instead of temp files between compiler stages
]

# global ccp defs type shi
global_cpp_defs = [
  '-DSPLIT_SAVES',
  '-D_LARGE_WORLDS',
  '-D_EXTENDED_ACHIEVEMENTS',
  '-D_DEBUG_MENUS_ENABLED',
  '-D_DEBUG',
  '-DDEBUG',
]

if host_machine.system() == 'emscripten'
  add_project_arguments(
    '-pthread',
    language: 'cpp'
  )

  add_project_link_arguments(
    '-sUSE_SDL=2',
    '-sUSE_WEBGL2=1',
    '-sLEGACY_GL_EMULATION=1',
    '-sALLOW_MEMORY_GROWTH=1',
    '-sPTHREAD_POOL_SIZE=4',
    '-sERROR_ON_UNDEFINED_SYMBOLS=0',
    '--use-port=zlib',
    language: 'cpp'
  )
endif

if get_option('enable_java_guis')
  global_cpp_defs += '-DENABLE_JAVA_GUIS'
endif

if get_option('enable_vsync')
  global_cpp_defs += '-DENABLE_VSYNC'
endif

# DecalOverdose: yes i know it's bad to do this but I don't know a better way to do this.
if host_machine.system() == 'linux' or host_machine.system() == 'emscripten'
  global_cpp_defs += [
    '-Dlinux',
    '-D__linux',
    '-D__linux__',
  ]
endif

subdir('4J.Render')
subdir('4J.Input')
subdir('4J.Profile')
subdir('4J.Storage')
subdir('Minecraft.Assets')
subdir('Minecraft.World')
subdir('Minecraft.Client')
