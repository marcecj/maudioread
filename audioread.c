/* Audioread.c is a mex-file that reads in arbitrary audio files much like the
 * built-in function wavread of Matlab. For the full documentation of its usage,
 * please refer to the attached m-file audioread.m.
 *
 * To compile this program, be sure to use GCC. Also, it *should* be possible to
 * compile it with any other C99-compliant compiler. On Windows, it is
 * recommended to use gnumex instead of one of the provided mex-compilers.
 * This file was meant to be linked against libavformat and libavcodec, which
 * are part of FFMPEG (from 09-02-2009).
 *
 * written by Bastian Bechtold, 2009 for the University of Applied Sciences
 * Oldenburg, Ostfriesland, Wilhelmshaven, licensed as GPLv2.
 * See the end of this file for the full license text.
 */

#include <stdio.h>
#include <stdlib.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/mem.h"
#include "libavutil/dict.h"
#include <string.h>
#include <mex.h>
#include <matrix.h>
#include <limits.h>
#include <math.h>

mxArray *create_opt_info_struct( AVFormatContext *pFormatContext,
                                AVCodecContext **pCodecContext,
                                AVCodec **pCodec, int num_streams )
{
    mxArray *pOutArray;
    int idx;
    char **fieldnames;
    mxArray *pTagStruct;
    mxArray *pStreamsStruct;

    /* ==========   build the main struct   ========== */

    /* set the field names */
    fieldnames = calloc( 6, sizeof(char*) );
    fieldnames[0] = "file_name";
    fieldnames[1] = "container_name";
    fieldnames[2] = "duration";
    fieldnames[3] = "file_size";
    fieldnames[4] = "tag_info";
    fieldnames[5] = "streams";

    /* create the actual struct */
    pOutArray = mxCreateStructMatrix( 1, 1, 6, (const char**) fieldnames );

    free( fieldnames );

    /* fill the struct */
    mxSetField( pOutArray, 0, "file_name",
        mxCreateString( pFormatContext->filename ) );
    mxSetField( pOutArray, 0, "container_name",
        mxCreateString( pFormatContext->iformat->long_name ) );
    mxSetField( pOutArray, 0, "duration",
        mxCreateDoubleScalar( (double)pFormatContext->duration / AV_TIME_BASE));
    mxSetField( pOutArray, 0, "file_size",
        mxCreateDoubleScalar( (double) pFormatContext->file_size ) );

    /* ==========   build the tag_info struct   ========== */

    /* set the field names */
    fieldnames = calloc( 8, sizeof(char*) );
    fieldnames[0] = "title";
    fieldnames[1] = "author";
    fieldnames[2] = "copyright";
    fieldnames[3] = "comment";
    fieldnames[4] = "album";
    fieldnames[5] = "year";
    fieldnames[6] = "track";
    fieldnames[7] = "genre";

    /* create the actual struct */
    pTagStruct = mxCreateStructMatrix( 1, 1, 8, (const char**) fieldnames );

    /* fill the struct */
    for( idx = 0; idx < 8; idx++ ) {
        AVDictionaryEntry* entry = av_dict_get(pFormatContext->metadata, fieldnames[idx], NULL, 0);

        if( entry == NULL )
            continue;

        mxSetField( pTagStruct, 0, fieldnames[idx], mxCreateString( entry->value ) );
    }
    free( fieldnames );

    /* put the tag_info struct in the main struct */
    mxSetField( pOutArray, 0, "tag_info", pTagStruct );

    /* ==========   build the streams struct   ========== */

    /* set the field names */
    fieldnames = calloc( 9, sizeof(char*) );
    fieldnames[0] = "codec_name";
    fieldnames[1] = "channels";
    fieldnames[2] = "sample_rate";
    fieldnames[3] = "bit_rate";
    fieldnames[4] = "bit_rate_tolerance";
    fieldnames[5] = "frame_size";
    fieldnames[6] = "min_quantizer";
    fieldnames[7] = "max_quantizer";
    fieldnames[8] = "quantizer_noise_shaping";

    /* create the actual struct */
    pStreamsStruct =
        mxCreateStructMatrix( 1, num_streams, 9, (const char**) fieldnames );
    free( fieldnames );

    for( idx = 0; idx < num_streams; idx++ ) {
        /* fill the struct */
        mxSetField( pStreamsStruct, idx, "codec_name",
            mxCreateString( pCodec[idx]->long_name ) );
        mxSetField( pStreamsStruct, idx, "channels",
            mxCreateDoubleScalar( (double) pCodecContext[idx]->channels ) );
        mxSetField( pStreamsStruct, idx, "sample_rate",
            mxCreateDoubleScalar( (double) pCodecContext[idx]->sample_rate ) );
        mxSetField( pStreamsStruct, idx, "bit_rate",
            mxCreateDoubleScalar( (double) pCodecContext[idx]->bit_rate ) );
        mxSetField( pStreamsStruct, idx, "bit_rate_tolerance",
            mxCreateDoubleScalar( (double)
            pCodecContext[idx]->bit_rate_tolerance ) );
        mxSetField( pStreamsStruct, idx, "frame_size",
            mxCreateDoubleScalar( (double) pCodecContext[idx]->frame_size ) );
        mxSetField( pStreamsStruct, idx, "min_quantizer",
            mxCreateDoubleScalar( (double) pCodecContext[idx]->qmin ) );
        mxSetField( pStreamsStruct, idx, "max_quantizer",
            mxCreateDoubleScalar( (double) pCodecContext[idx]->qmax ) );
        mxSetField( pStreamsStruct, idx, "quantizer_noise_shaping",
            mxCreateDoubleScalar( (double)
            pCodecContext[idx]->quantizer_noise_shaping) );
    }

    /* put the streams struct in the main struct */
    mxSetField( pOutArray, 0, "streams", pStreamsStruct );

    return( pOutArray );
}

void mexFunction( int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[] )
{
    /* ---------------------------------------------------------------------- *
     * error check function arguments                                         *
     * ---------------------------------------------------------------------- */

    int getSizeOnly = 0;
    /* the max number of samples */
    long getNumSamples = LONG_MAX;

    const char *szFileName;

    /* ==========   error check input arguments   ========== */

    if( nrhs != 1 && nrhs != 2 ) {
        mexErrMsgTxt( "audioread: Wrong number of input arguments. Must be 1 or 2." );
        return;
    }
    if( !mxIsChar( prhs[0] ) ) {
        mexErrMsgTxt( "audioread: wrong kind of first input argument. Must be a string" );
        return;
    }
    if( nrhs == 2 ) {
        if( !mxIsNumeric( prhs[1] ) && !mxIsChar( prhs[1] ) ) {
            mexErrMsgTxt( "audioread: wrong second input argument. "
                          "Must be either a scalar or 'string'" );
            return;
        }
        if( mxIsNumeric( prhs[1] ) ) {
            if( mxGetM( prhs[1] ) != 1 || mxGetN( prhs[1] ) != 1 ) {
                mexErrMsgTxt( "audioread: wrong second input argument. "
                              "Must be either a scalar or 'string'" );
                return;
            }
            getNumSamples = mxGetScalar( prhs[1] );
        }
        if( mxIsChar( prhs[1] ) ) {
            if( strcmp( mxArrayToString( prhs[1] ), "size" ) ) {
                mexErrMsgTxt( "audioread: wrong second input argument. "
                              "Must be either a scalar or 'string'" );
                return;
            }
            getSizeOnly = 1;
        }
    }

    /* ==========   error check output arguments   ========== */

    if( nlhs != 0 && nlhs != 1 && nlhs != 3 && nlhs != 4 ) {
        mexErrMsgTxt( "audioread: wrong number of output arguments. "
                      "Must be 1 or 3 or 4." );
        return;
    }
    if( nlhs > 1 && getSizeOnly ) {
       mexErrMsgTxt( "audioread: wrong number of output arguments. "
                     "You can only have one ouput argument while asking for the size." );
       return;
    }

    /* at this point, all input parameters are supposed to be valid */

    szFileName = mxArrayToString( prhs[0] );

    /* ---------------------------------------------------------------------- *
     * initialize AVCodec and AVFormat                                        *
     * ---------------------------------------------------------------------- */

    AVFormatContext *pFormatContext = NULL;
    AVCodec         **pCodec = NULL;
    AVCodecContext  **pCodecContext = NULL;

    int idx,
        *stream_idx = NULL,
        num_streams = 0;
    int *num_channels;

    int max_channels = 0;
    long max_samples = 0;
    int est_num_of_samples;

    double *pContent;

    /* ==========   get the format context   ========== */

    avcodec_register_all();
    av_register_all();

    /* open file */
    if ( av_open_input_file( &pFormatContext, szFileName, NULL, 0, NULL) != 0 ) {
        mexErrMsgTxt("could not retrieve codec context");
        return;
    }

    /* retrieve stream */
    if( av_find_stream_info(pFormatContext) < 0 ) {
        /* free memory and exit */
        av_close_input_file( pFormatContext );
        mexErrMsgTxt("could not retrieve stream");
        return;
    }

    /* find all audio streams */
    for( idx = 0; idx < pFormatContext->nb_streams; idx++ ) {
        if( pFormatContext->streams[idx]->codec->codec_type == CODEC_TYPE_AUDIO ) {
            stream_idx = (int*) realloc( stream_idx, ++num_streams*sizeof(int));
            stream_idx[num_streams-1] = idx;
        }
    }
    if( num_streams == 0 ) {
        /* free memory and exit */
        av_close_input_file( pFormatContext );
        mexErrMsgTxt("no audio stream found");
        return;
    }

    /* =======   for each stream, get the codecs ans their contexts   ======= */

    pCodecContext = (AVCodecContext**) malloc( num_streams*sizeof(AVCodecContext*) );
    pCodec = (AVCodec**) malloc( num_streams*sizeof(AVCodec*) );
    num_channels = malloc( num_streams*sizeof(int) );

    for( idx = 0; idx < num_streams; ++idx ) {
        /* set a pointer to the codec context for the stream */
        pCodecContext[idx] = pFormatContext->streams[stream_idx[idx]]->codec;

        /* find decoder for the stream */
        pCodec[idx] = avcodec_find_decoder( pCodecContext[idx]->codec_id );
        if( pCodec[idx] == NULL ) {
            /* free memory and exit */
            av_close_input_file( pFormatContext );
            num_streams = idx;
            for( idx = 0; idx < num_streams; ++idx )
                avcodec_close( pCodecContext[idx] );
            free( pCodecContext );
            free( num_channels );
            free( pCodec );
            mexErrMsgTxt("codec not found");
            return;
        }

        /* open the codec for the decoder */
        if (avcodec_open(pCodecContext[idx], pCodec[idx]) < 0) {
            /* free memory and exit */
            av_close_input_file( pFormatContext );
            num_streams = idx;
            for( idx = 0; idx < num_streams; ++idx )
                avcodec_close( pCodecContext[idx] );
            free( pCodecContext );
            free( num_channels );
            free( pCodec );
            mexErrMsgTxt("could not allocate codec context");
            return;
        }

        num_channels[idx] = pCodecContext[idx]->channels;
    }

    /* ==========   calculate max number of channels and samples   ========== */

    for( idx = 0; idx < num_streams; idx++ ) {
        if( num_channels[idx] > max_channels )
            max_channels = num_channels[idx];
        /* I can only estimate the number of samples from the metadata */
        est_num_of_samples = (double) pCodecContext[idx]->sample_rate *
            pFormatContext->streams[stream_idx[idx]]->duration *
            pFormatContext->streams[stream_idx[idx]]->time_base.num /
            pFormatContext->streams[stream_idx[idx]]->time_base.den;
        if( est_num_of_samples > max_samples )
            max_samples = est_num_of_samples;
    }

    /* =======   if only the size is asked, calculate it and return   ======= */

    if( getSizeOnly ) {

        /* create the output matrix */
        plhs[0] = mxCreateDoubleMatrix( 1, 3, mxREAL );
        pContent = mxGetPr( plhs[0] );
        pContent[0] = num_streams;
        pContent[1] = max_channels;
        pContent[2] = max_samples;
        mxSetPr( plhs[0], pContent );

        /* free memory and exit */
        av_close_input_file( pFormatContext );
        for( idx = 0; idx < num_streams; ++idx )
            avcodec_close( pCodecContext[idx] );
        free( pCodecContext );
        free( num_channels );
        free( pCodec );

        return;
    }

    /* ---------------------------------------------------------------------- *
     * read and decode actual audio data                                      *
     * ---------------------------------------------------------------------- */

    int16_t *block_buf = NULL;      // buffer for the samples in one audio block
    int block_len;                  // length of one block
    int16_t **audio_buf;            // buffer for the whole audio stream
    long *audio_buf_len;            // length of the whole audio stream
    long *actual_buf_size;          // length of the whole audio buffer
    int alloc_increment = 65536;    // allocate this much samples at buffer
    int read_bytes, n;              // counter variables
    AVPacket packet;                // one block of encoded audio
    static int bytes_remaining = 0;
    static uint8_t *raw_data;       // a block of raw, encoded data

    int max_samples_per_channel = 0;

    int idx2;
    long idx3;
    double max_uint16_quot = 1 / pow(2,15);


    audio_buf = (int16_t**) calloc( num_streams, sizeof(int16_t*) );
    audio_buf_len = (long*) calloc( num_streams, sizeof(long) );
    actual_buf_size = (long*) calloc( num_streams, sizeof(long) );
    /* preallocate the audio buffer. This is completely unnecessary on Unix
       but absolutely paramount on Windows. */
    for( idx = 0; idx < num_streams; idx++ ) {
        actual_buf_size[idx] = max_samples*max_channels;
        audio_buf[idx] = (int16_t*) malloc( actual_buf_size[idx] * sizeof(int16_t) );
    }

    /* read frames and concatenate them in audio_out */
    while( av_read_frame(pFormatContext, &packet) >= 0 ) {

        /* go through all valid streams */
        for( idx = 0; idx < num_streams; idx++ ) {

            /* stop working on streams that already have enough samples */
            if( audio_buf_len[idx] >= (double) getNumSamples*num_channels[idx] ) {
                break;
            }

            if( packet.stream_index == stream_idx[idx] ) {

                bytes_remaining = packet.size;
                raw_data = packet.data;

                while( bytes_remaining > 0 ) {

                    /* reallocate block buffer. This is a libavcodec requirement */
                    block_buf = (int16_t*) realloc( block_buf,
                                AVCODEC_MAX_AUDIO_FRAME_SIZE * sizeof(int16_t) );
                    block_len = AVCODEC_MAX_AUDIO_FRAME_SIZE;

                    read_bytes = avcodec_decode_audio2(pCodecContext[idx], block_buf,
                                                       &block_len,
                                                       raw_data, bytes_remaining);

                    raw_data += read_bytes;
                    bytes_remaining -= read_bytes;
                    block_len /= sizeof(int16_t);
                    audio_buf_len[idx] += block_len;

                    /* enlarge audio buffer by alloc_increment if necessary */
                    if( actual_buf_size[idx] < audio_buf_len[idx] ) {
                        actual_buf_size[idx] += alloc_increment;
                        audio_buf[idx] = (int16_t*) realloc( audio_buf[idx],
                                        actual_buf_size[idx] * sizeof(int16_t) );
                    }
                    /* copy the new samples to the audio buffer */
                    for( n = 0; n < block_len; ++n )
                        audio_buf[idx][audio_buf_len[idx] - block_len + n] = block_buf[n];
                }
            }
        }
        if( packet.data != NULL )
            av_free_packet(&packet);
    }

    /* limit audio data to their limit, if this was asked */
    for( idx = 0; idx < num_streams; idx++ ) {
        if( audio_buf_len[idx] >= (double) getNumSamples*num_channels[idx] ) {
            audio_buf[idx] = (int16_t*) realloc( audio_buf[idx],
                             getNumSamples*num_channels[idx] * sizeof(int16_t) );
            audio_buf_len[idx] = getNumSamples*num_channels[idx];
        }
    }

    /* ==========   write data to output variables   ========== */

    /* set the first output variable */

    /* calculate the max number of channels and samples */
    max_samples = 0;
    max_channels = 0;
    for( idx = 0; idx < num_streams; idx++ ) {
        if( audio_buf_len[idx]/num_channels[idx] > max_samples_per_channel )
            max_samples_per_channel = audio_buf_len[idx]/num_channels[idx];
        if( audio_buf_len[idx] > max_samples )
            max_samples = audio_buf_len[idx];
        if( num_channels[idx] > max_channels )
            max_channels = num_channels[idx];
    }
    /* create and fill the output matrix */

    /* if there is only one stream, make the output array 2-dimensional */
    if( num_streams == 1 ) {
        mwSize dims[2] = {max_channels, max_samples_per_channel};
        plhs[0] = mxCreateNumericArray( 2, dims, mxDOUBLE_CLASS, mxREAL );
    } else {
        mwSize dims[3] = {num_streams, max_channels, max_samples_per_channel};
        plhs[0] = mxCreateNumericArray( 3, dims, mxDOUBLE_CLASS, mxREAL );
    }
    pContent = mxGetPr( plhs[0] );
    for( idx = 0; idx < num_streams; idx++ ) {
        for( idx2 = 0; idx2 < num_channels[idx]; idx2++ ) {
            for( idx3 = 0; idx3 < audio_buf_len[idx]/num_channels[idx]; idx3++ ) {
                /* reorder and scale the samples
                   (there is no difference of order between 2 or 3 dimensions) */
                pContent[ idx3*num_channels[idx]*num_streams + idx2*num_streams + idx ]
                    = max_uint16_quot * audio_buf[idx][ idx3*num_channels[idx] + idx2 ];
            }
        }
    }
    mxSetPr( plhs[0], pContent );

    /* set second and third output variables (bitrate and samplerate) */

    if( nlhs >= 3 ) {
        plhs[1] = mxCreateDoubleMatrix( 1, num_streams, mxREAL);
        pContent = mxGetPr( plhs[1] );
        for( idx = 0; idx < num_streams; idx++ )
            pContent[idx] = pCodecContext[idx]->sample_rate;
        mxSetPr( plhs[1], pContent );

        plhs[2] = mxCreateDoubleMatrix( 1, num_streams, mxREAL);
        pContent = mxGetPr( plhs[2] );
        for( idx = 0; idx < num_streams; idx++ ) {
            pContent[idx] = (double) pCodecContext[idx]->bit_rate /
                (double) pCodecContext[idx]->sample_rate /
                (double) num_channels[idx];
        }
        mxSetPr( plhs[2], pContent );
    }

    /* set the last output variable. This is tricky, so do it in a function */

    if( nlhs == 4 )
        plhs[3] = create_opt_info_struct( pFormatContext,
                                          pCodecContext, pCodec, num_streams );

    /* ---------------------------------------------------------------------- *
     * free libav... memory                                                   *
     * ---------------------------------------------------------------------- */

    for( idx = 0; idx < num_streams; ++idx )
        avcodec_close( pCodecContext[idx] );
    free( pCodecContext );
    free( num_channels );
    free( pCodec );
    av_close_input_file( pFormatContext );

    free( block_buf );
    for( idx = 0; idx < num_streams; ++idx )
        free( audio_buf[idx] );
    free( audio_buf );
    free( audio_buf_len );

    return;
}

/*
            GNU GENERAL PUBLIC LICENSE
               Version 2, June 1991

 Copyright (C) 1989, 1991 Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 Everyone is permitted to copy and distribute verbatim copies
 of this license document, but changing it is not allowed.

                Preamble

  The licenses for most software are designed to take away your
freedom to share and change it.  By contrast, the GNU General Public
License is intended to guarantee your freedom to share and change free
software--to make sure the software is free for all its users.  This
General Public License applies to most of the Free Software
Foundation's software and to any other program whose authors commit to
using it.  (Some other Free Software Foundation software is covered by
the GNU Lesser General Public License instead.)  You can apply it to
your programs, too.

  When we speak of free software, we are referring to freedom, not
price.  Our General Public Licenses are designed to make sure that you
have the freedom to distribute copies of free software (and charge for
this service if you wish), that you receive source code or can get it
if you want it, that you can change the software or use pieces of it
in new free programs; and that you know you can do these things.

  To protect your rights, we need to make restrictions that forbid
anyone to deny you these rights or to ask you to surrender the rights.
These restrictions translate to certain responsibilities for you if you
distribute copies of the software, or if you modify it.

  For example, if you distribute copies of such a program, whether
gratis or for a fee, you must give the recipients all the rights that
you have.  You must make sure that they, too, receive or can get the
source code.  And you must show them these terms so they know their
rights.

  We protect your rights with two steps: (1) copyright the software, and
(2) offer you this license which gives you legal permission to copy,
distribute and/or modify the software.

  Also, for each author's protection and ours, we want to make certain
that everyone understands that there is no warranty for this free
software.  If the software is modified by someone else and passed on, we
want its recipients to know that what they have is not the original, so
that any problems introduced by others will not reflect on the original
authors' reputations.

  Finally, any free program is threatened constantly by software
patents.  We wish to avoid the danger that redistributors of a free
program will individually obtain patent licenses, in effect making the
program proprietary.  To prevent this, we have made it clear that any
patent must be licensed for everyone's free use or not licensed at all.

  The precise terms and conditions for copying, distribution and
modification follow.

            GNU GENERAL PUBLIC LICENSE
   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION

  0. This License applies to any program or other work which contains
a notice placed by the copyright holder saying it may be distributed
under the terms of this General Public License.  The "Program", below,
refers to any such program or work, and a "work based on the Program"
means either the Program or any derivative work under copyright law:
that is to say, a work containing the Program or a portion of it,
either verbatim or with modifications and/or translated into another
language.  (Hereinafter, translation is included without limitation in
the term "modification".)  Each licensee is addressed as "you".

Activities other than copying, distribution and modification are not
covered by this License; they are outside its scope.  The act of
running the Program is not restricted, and the output from the Program
is covered only if its contents constitute a work based on the
Program (independent of having been made by running the Program).
Whether that is true depends on what the Program does.

  1. You may copy and distribute verbatim copies of the Program's
source code as you receive it, in any medium, provided that you
conspicuously and appropriately publish on each copy an appropriate
copyright notice and disclaimer of warranty; keep intact all the
notices that refer to this License and to the absence of any warranty;
and give any other recipients of the Program a copy of this License
along with the Program.

You may charge a fee for the physical act of transferring a copy, and
you may at your option offer warranty protection in exchange for a fee.

  2. You may modify your copy or copies of the Program or any portion
of it, thus forming a work based on the Program, and copy and
distribute such modifications or work under the terms of Section 1
above, provided that you also meet all of these conditions:

    a) You must cause the modified files to carry prominent notices
    stating that you changed the files and the date of any change.

    b) You must cause any work that you distribute or publish, that in
    whole or in part contains or is derived from the Program or any
    part thereof, to be licensed as a whole at no charge to all third
    parties under the terms of this License.

    c) If the modified program normally reads commands interactively
    when run, you must cause it, when started running for such
    interactive use in the most ordinary way, to print or display an
    announcement including an appropriate copyright notice and a
    notice that there is no warranty (or else, saying that you provide
    a warranty) and that users may redistribute the program under
    these conditions, and telling the user how to view a copy of this
    License.  (Exception: if the Program itself is interactive but
    does not normally print such an announcement, your work based on
    the Program is not required to print an announcement.)

These requirements apply to the modified work as a whole.  If
identifiable sections of that work are not derived from the Program,
and can be reasonably considered independent and separate works in
themselves, then this License, and its terms, do not apply to those
sections when you distribute them as separate works.  But when you
distribute the same sections as part of a whole which is a work based
on the Program, the distribution of the whole must be on the terms of
this License, whose permissions for other licensees extend to the
entire whole, and thus to each and every part regardless of who wrote it.

Thus, it is not the intent of this section to claim rights or contest
your rights to work written entirely by you; rather, the intent is to
exercise the right to control the distribution of derivative or
collective works based on the Program.

In addition, mere aggregation of another work not based on the Program
with the Program (or with a work based on the Program) on a volume of
a storage or distribution medium does not bring the other work under
the scope of this License.

  3. You may copy and distribute the Program (or a work based on it,
under Section 2) in object code or executable form under the terms of
Sections 1 and 2 above provided that you also do one of the following:

    a) Accompany it with the complete corresponding machine-readable
    source code, which must be distributed under the terms of Sections
    1 and 2 above on a medium customarily used for software interchange; or,

    b) Accompany it with a written offer, valid for at least three
    years, to give any third party, for a charge no more than your
    cost of physically performing source distribution, a complete
    machine-readable copy of the corresponding source code, to be
    distributed under the terms of Sections 1 and 2 above on a medium
    customarily used for software interchange; or,

    c) Accompany it with the information you received as to the offer
    to distribute corresponding source code.  (This alternative is
    allowed only for noncommercial distribution and only if you
    received the program in object code or executable form with such
    an offer, in accord with Subsection b above.)

The source code for a work means the preferred form of the work for
making modifications to it.  For an executable work, complete source
code means all the source code for all modules it contains, plus any
associated interface definition files, plus the scripts used to
control compilation and installation of the executable.  However, as a
special exception, the source code distributed need not include
anything that is normally distributed (in either source or binary
form) with the major components (compiler, kernel, and so on) of the
operating system on which the executable runs, unless that component
itself accompanies the executable.

If distribution of executable or object code is made by offering
access to copy from a designated place, then offering equivalent
access to copy the source code from the same place counts as
distribution of the source code, even though third parties are not
compelled to copy the source along with the object code.

  4. You may not copy, modify, sublicense, or distribute the Program
except as expressly provided under this License.  Any attempt
otherwise to copy, modify, sublicense or distribute the Program is
void, and will automatically terminate your rights under this License.
However, parties who have received copies, or rights, from you under
this License will not have their licenses terminated so long as such
parties remain in full compliance.

  5. You are not required to accept this License, since you have not
signed it.  However, nothing else grants you permission to modify or
distribute the Program or its derivative works.  These actions are
prohibited by law if you do not accept this License.  Therefore, by
modifying or distributing the Program (or any work based on the
Program), you indicate your acceptance of this License to do so, and
all its terms and conditions for copying, distributing or modifying
the Program or works based on it.

  6. Each time you redistribute the Program (or any work based on the
Program), the recipient automatically receives a license from the
original licensor to copy, distribute or modify the Program subject to
these terms and conditions.  You may not impose any further
restrictions on the recipients' exercise of the rights granted herein.
You are not responsible for enforcing compliance by third parties to
this License.

  7. If, as a consequence of a court judgment or allegation of patent
infringement or for any other reason (not limited to patent issues),
conditions are imposed on you (whether by court order, agreement or
otherwise) that contradict the conditions of this License, they do not
excuse you from the conditions of this License.  If you cannot
distribute so as to satisfy simultaneously your obligations under this
License and any other pertinent obligations, then as a consequence you
may not distribute the Program at all.  For example, if a patent
license would not permit royalty-free redistribution of the Program by
all those who receive copies directly or indirectly through you, then
the only way you could satisfy both it and this License would be to
refrain entirely from distribution of the Program.

If any portion of this section is held invalid or unenforceable under
any particular circumstance, the balance of the section is intended to
apply and the section as a whole is intended to apply in other
circumstances.

It is not the purpose of this section to induce you to infringe any
patents or other property right claims or to contest validity of any
such claims; this section has the sole purpose of protecting the
integrity of the free software distribution system, which is
implemented by public license practices.  Many people have made
generous contributions to the wide range of software distributed
through that system in reliance on consistent application of that
system; it is up to the author/donor to decide if he or she is willing
to distribute software through any other system and a licensee cannot
impose that choice.

This section is intended to make thoroughly clear what is believed to
be a consequence of the rest of this License.

  8. If the distribution and/or use of the Program is restricted in
certain countries either by patents or by copyrighted interfaces, the
original copyright holder who places the Program under this License
may add an explicit geographical distribution limitation excluding
those countries, so that distribution is permitted only in or among
countries not thus excluded.  In such case, this License incorporates
the limitation as if written in the body of this License.

  9. The Free Software Foundation may publish revised and/or new versions
of the General Public License from time to time.  Such new versions will
be similar in spirit to the present version, but may differ in detail to
address new problems or concerns.

Each version is given a distinguishing version number.  If the Program
specifies a version number of this License which applies to it and "any
later version", you have the option of following the terms and conditions
either of that version or of any later version published by the Free
Software Foundation.  If the Program does not specify a version number of
this License, you may choose any version ever published by the Free Software
Foundation.

  10. If you wish to incorporate parts of the Program into other free
programs whose distribution conditions are different, write to the author
to ask for permission.  For software which is copyrighted by the Free
Software Foundation, write to the Free Software Foundation; we sometimes
make exceptions for this.  Our decision will be guided by the two goals
of preserving the free status of all derivatives of our free software and
of promoting the sharing and reuse of software generally.

                NO WARRANTY

  11. BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY
FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN
OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES
PROVIDE THE PROGRAM "AS IS" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED
OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS
TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE
PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,
REPAIR OR CORRECTION.

  12. IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR
REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,
INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING
OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED
TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY
YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER
PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE
POSSIBILITY OF SUCH DAMAGES.

             END OF TERMS AND CONDITIONS

        How to Apply These Terms to Your New Programs

  If you develop a new program, and you want it to be of the greatest
possible use to the public, the best way to achieve this is to make it
free software which everyone can redistribute and change under these terms.

  To do so, attach the following notices to the program.  It is safest
to attach them to the start of each source file to most effectively
convey the exclusion of warranty; and each file should have at least
the "copyright" line and a pointer to where the full notice is found.

    <one line to give the program's name and a brief idea of what it does.>
    Copyright (C) <year>  <name of author>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

Also add information on how to contact you by electronic and paper mail.

If the program is interactive, make it output a short notice like this
when it starts in an interactive mode:

    Gnomovision version 69, Copyright (C) year name of author
    Gnomovision comes with ABSOLUTELY NO WARRANTY; for details type `show w'.
    This is free software, and you are welcome to redistribute it
    under certain conditions; type `show c' for details.

The hypothetical commands `show w' and `show c' should show the appropriate
parts of the General Public License.  Of course, the commands you use may
be called something other than `show w' and `show c'; they could even be
mouse-clicks or menu items--whatever suits your program.

You should also get your employer (if you work as a programmer) or your
school, if any, to sign a "copyright disclaimer" for the program, if
necessary.  Here is a sample; alter the names:

  Yoyodyne, Inc., hereby disclaims all copyright interest in the program
  `Gnomovision' (which makes passes at compilers) written by James Hacker.

  <signature of Ty Coon>, 1 April 1989
  Ty Coon, President of Vice

This General Public License does not permit incorporating your program into
proprietary programs.  If your program is a subroutine library, you may
consider it more useful to permit linking proprietary applications with the
library.  If this is what you want to do, use the GNU Lesser General
Public License instead of this License.

end of file */
