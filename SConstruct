# vim:ft=python
# TODO: Test Mac.

import os
import platform

Help(
"""This build system compiles the audioread Mex files.  To compile, use one of
the following build targets:
    audioread    -> compile audioread (default)
    makezip      -> create a zip file
    install      -> install audioread to directory "audioread"
    all          -> runs both audioread and makezip
"""
)

## environment variables and options

# modifiable environment variables
env_vars = Variables()
env_vars.AddVariables(
    ('CC', 'The C compiler'),
    BoolVariable('debug', 'Build debug code', False),
    ('DESTDIR', 'The install destination', 'audioread'),
)

AddOption('--force-mingw',
          dest='forcemingw',
          default=False,
          action='store_true',
          help='Force the use of mingw on Windows platforms.'
         )

## set the environment and extra builders

# the Matlab tool automatically sets various environment variables
if os.name == 'nt' and GetOption('forcemingw'):
    env = Environment(tools = ['mingw', 'filesystem', 'zip', 'packaging', 'matlab'],
                      variables = env_vars)
else:
    env = Environment(tools = ['default', 'packaging', 'matlab'],
                      variables = env_vars)

## platform dependent environment configuration

# assume a GCC-compatible compiler on Unix like platforms
if env['PLATFORM'] in ("posix", "darwin"):

    env.Append(CCFLAGS   = "-DNDEBUG -std=c99 -O2 -pedantic -Wall -Wextra",
               # LINKFLAGS = "-Wl,-O1 -Wl,--no-copy-dt-needed-entries -Wl,--as-needed -Wl,--no-undefined")
               LINKFLAGS = "-Wl,-O1 -Wl,--no-copy-dt-needed-entries -Wl,--as-needed")

    # activate optimizations in GCC 4.5
    if env['CC'] == 'gcc' and env['CCVERSION'] >= '4.5':
        env.Append(CCFLAGS=[
            "-ftree-vectorize",
            # "-ftree-vectorizer-verbose=2",
            "-floop-interchange",
            "-floop-strip-mine",
            "-floop-block",
            "-fno-reorder-blocks", # Matlab crashes without this!
        ])

    # if the system is 64 bit and Matlab is 32 bit, compile for 32 bit; since
    # Matlab currently only runs on x86 architectures, checking for x86_64
    # should suffice
    if platform.machine() == "x86_64" \
       and not env['MATLAB']['ARCH'].endswith('64'):
        env.Append(
            CCFLAGS    = "-m32",
            LINKFLAGS  = "-m32",
            CPPDEFINES = "_FILE_OFFSET_BITS=64"
        )

elif env['PLATFORM'] == "win32":

    # enforce searching in the top-level Win directory
    win_path = os.sep.join([os.path.abspath(os.path.curdir), 'Win'])
    env.Append(LIBPATH = win_path, CPPPATH = win_path)

    env.Replace(WINDOWS_INSERT_DEF = True)

else:

    exit("Oops, not a supported platform.")

## check the system for required features

if not (GetOption('clean') or GetOption('help')):
    conf = env.Configure()

    if not conf.CheckLibWithHeader('avcodec', 'libavcodec/avcodec.h', 'c'):
        exit("error: you need to install ffmpeg(-dev)")
    if not conf.CheckLibWithHeader('avformat', 'libavformat/avformat.h', 'c'):
        exit("error: you need to install ffmpeg(-dev)")
    if not conf.CheckLibWithHeader('avutil', 'libavutil/avutil.h', 'c'):
        exit("error: you need to install ffmpeg(-dev)")

    env = conf.Finish()

## compilation targets
#
# These are: audioread (build and debug variants), the corresponding m-Files, and
# a visual studio project (if the current platform is Windows and MinGW is not
# used).


if env['debug']:
    if env['PLATFORM'] == "win32" and 'mingw' not in env['TOOLS']:
        env.AppendUnique(CCFLAGS='/Zi', LINKFLAGS='/DEBUG')
    else:
        env.MergeFlags("-g -O0")

audioread = env.Mex(['audioread.c'])
mfiles    = env.Glob('*.m', source=False)

# define the package sources and corresponding install targets
pkg_src = audioread + mfiles
audioread_inst = env.Install(env['DESTDIR'], pkg_src)

audioread_pkg = env.Package(
    NAME        = "audioread",
    VERSION     = "0.1",
    PACKAGETYPE = "zip"
)

## miscellanea

# define some useful aliases
Alias("makezip", audioread_pkg)
Alias("install", audioread_inst)
Alias("audioread", audioread)
Alias("all", audioread + audioread_pkg)

# set the default target
Default("audioread")

Help(
"""\n
The following options may be set:
    --force-mingw   ->  Force the use of mingw (Windows only).

The following environment variables can be overridden by passing them *after*
the call to scons, e.g., "scons CC=gcc":
"""
+ env_vars.GenerateHelpText(env)
)
