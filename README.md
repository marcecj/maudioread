# The maudioread Mex extension
Marc Joliet <marcec@gmx.de>

Copyright (C) 2009 Bastian Bechtold
Copyright (C) 2013 Marc Joliet

## Introduction

maudioread is a simple MATLAB Mex file that uses [FFmpeg](http://FFmpeg.org/)
(or [libav](http://libav.org/)) for reading audio files, through which it
supports all sorts of audio files.

maudioread was originally written by [Bastian Bechtold](http://bastibe.de/), I
merely updated it to be compatible with current FFmpeg.

## Installation

To compile maudioread use the `compileAndTest.m` script.  Since FFmpeg/libav use
C99 features in their APIs, you must use a C99-compliant compiler such as GCC.
On Windows it is recommended to use gnumex instead of one of the provided
mex-compilers.  maudioread is meant to be linked against libavformat and
libavcodec, which are part of FFmpeg.

## Related Software

An alternative to maudioread is [msndfile](https://github.com/marcecj/msndfile),
which uses libsndfile instead of FFmpeg/libav.  Therefor it cannot read formats
such as mp3.  However, for that it is mostly API-compatible with `wavread` and
offers special blockwise reading functionality.
