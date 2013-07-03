function [ varargin ] = audioread( varargout )
%AUDIOREAD reads audio files
%   Y=AUDIOREAD(FILENAME) reads a the file specified by FILENAME and returns
%	the sampled audio data in Y. Amplitude values are in the range [-1,+1].
%
%	note that in files with more than one stream, the output matrix will be
%	three-dimensional.
%
%	[Y,FS,NBITS]=AUDIOREAD(...) returns the sample rate (FS) in Hz and the
%	average number of bits per sample (NBITS) used to encode the data in the
%	file. Since both values may be different in different streams, these are
%	arrays that contain one value for each stream.
%
%	[Y,FS,NBITS,OPTS]=AUDIOREAD(...) returns a structure OPTS with some
%	additional information about the streams.
%
%	[...]=AUDIOREAD(FILE,N) only reads the first N samples of each channel.
%
%	SIZE=AUDIOREAD(FILE,'size') returns the number of streams, channels and
%	samples in the file. Due to some limitations of ffmpeg, the number of
%	samples that can be obtained before actually reading the whole file may
%	differ slightly from the actual number of samples. A difference of up to two
%	percent should be expected in compressed formats.

% Written by Bastian Bechtold, 2009 for the University of Applied Sciences
% Oldenburg, Ostfriesland, Wilhelmshaven, licensed as GPLv2 (see the file
% LICENSE).
