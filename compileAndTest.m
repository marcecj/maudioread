clear;
close all;

if ismac % -L../Mac/lib
	mex -L../Mac/lib -lavcodec -lavformat -lavutil -lfaad -I../Mac/include audioread.c
elseif ispc
	% this is for gnumex:
	mex -L..\Windows\lib -lavcodec-51 -lavformat-52 -I..\Windows\include audioread.c -output audioread
elseif isunix
	% this does not work.
	mex -L../Linux/lib -lavutil -lavcodec -lavformat -I../Linux/include audioread.c
end


tic
[y fs bit opt]=audioread('../Testfiles/Test.wav');
toc

fs
bit
disp(opt)

subplot(2,1,1);
plot(y(1,:));

subplot(2,1,2);
plot(y(2,:));
