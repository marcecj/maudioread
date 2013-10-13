/* Audioread.c is a mex-file that reads in arbitrary audio files much like the
 * built-in function wavread of Matlab. For general instructions, see the README
 * file; for the full documentation of its usage, please refer to the attached
 * m-file audioread.m.
 *
 * Written by Bastian Bechtold, 2009 for the University of Applied Sciences
 * Oldenburg, Ostfriesland, Wilhelmshaven, licensed as GPLv2.  See the file
 * LICENSE for the full license text.
 */

#include <stdio.h>
#include <stdlib.h>
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/mem.h"
#include "libavutil/dict.h"
#include "libavutil/avutil.h"
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
    fieldnames = calloc( 5, sizeof(char*) );
    fieldnames[0] = "file_name";
    fieldnames[1] = "container_name";
    fieldnames[2] = "duration";
    fieldnames[3] = "tag_info";
    fieldnames[4] = "streams";

    /* create the actual struct */
    pOutArray = mxCreateStructMatrix( 1, 1, 5, (const char**) fieldnames );

    free( fieldnames );

    /* fill the struct */
    mxSetField( pOutArray, 0, "file_name",
        mxCreateString( pFormatContext->filename ) );
    mxSetField( pOutArray, 0, "container_name",
        mxCreateString( pFormatContext->iformat->long_name ) );
    mxSetField( pOutArray, 0, "duration",
        mxCreateDoubleScalar( (double)pFormatContext->duration / AV_TIME_BASE));

    /* ==========   build the tag_info struct   ========== */

    /* set the field names */
    fieldnames = calloc( 9, sizeof(char*) );
    fieldnames[0] = "title";
    fieldnames[1] = "author";
    fieldnames[2] = "artist";
    fieldnames[3] = "copyright";
    fieldnames[4] = "comment";
    fieldnames[5] = "album";
    fieldnames[6] = "date";
    fieldnames[7] = "track";
    fieldnames[8] = "genre";

    /* create the actual struct */
    pTagStruct = mxCreateStructMatrix( 1, 1, 9, (const char**) fieldnames );

    /* fill the struct */
    for( idx = 0; idx < 9; idx++ ) {
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
    fieldnames = calloc( 8, sizeof(char*) );
    fieldnames[0] = "codec_name";
    fieldnames[1] = "channels";
    fieldnames[2] = "sample_rate";
    fieldnames[3] = "bit_rate";
    fieldnames[4] = "bit_rate_tolerance";
    fieldnames[5] = "frame_size";
    fieldnames[6] = "min_quantizer";
    fieldnames[7] = "max_quantizer";

    /* create the actual struct */
    pStreamsStruct =
        mxCreateStructMatrix( 1, num_streams, 8, (const char**) fieldnames );
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
    if ( avformat_open_input( &pFormatContext, szFileName, NULL, NULL) != 0 ) {
        mexErrMsgTxt("could not retrieve codec context");
        return;
    }

    /* retrieve stream */
    if( avformat_find_stream_info(pFormatContext, NULL) < 0 ) {
        /* free memory and exit */
        avformat_close_input( &pFormatContext );
        mexErrMsgTxt("could not retrieve stream");
        return;
    }

    /* find all audio streams */
    for( unsigned int idx2 = 0; idx2 < pFormatContext->nb_streams; idx2++ ) {
        if( pFormatContext->streams[idx2]->codec->codec_type == AVMEDIA_TYPE_AUDIO ) {
            stream_idx = (int*) realloc( stream_idx, ++num_streams*sizeof(int));
            stream_idx[num_streams-1] = idx2;
        }
    }
    if( num_streams == 0 ) {
        /* free memory and exit */
        avformat_close_input( &pFormatContext );
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
            avformat_close_input( &pFormatContext );
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
        if (avcodec_open2(pCodecContext[idx], pCodec[idx], NULL) < 0) {
            /* free memory and exit */
            avformat_close_input( &pFormatContext );
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
        avformat_close_input( &pFormatContext );
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

    int block_len;                  // length of one block
    int16_t **audio_buf;            // buffer for the whole audio stream
    long *audio_buf_len;            // length of the whole audio stream
    long *actual_buf_size;          // length of the whole audio buffer
    int alloc_increment = 65536;    // allocate this much samples at buffer
    int read_bytes;                 // counter variables
    AVPacket packet;                // one block of encoded audio
    static int bytes_remaining = 0;

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

                /* request the entire file */
                AVFrame block_frame;
                bytes_remaining = packet.size;

                while( bytes_remaining > 0 ) {

                    block_frame.nb_samples = bytes_remaining;

                    read_bytes = avcodec_decode_audio4(pCodecContext[idx], &block_frame,
                                                       &block_len, &packet);

                    if( block_len == 0 )
                        continue;

                    block_len = read_bytes/sizeof(int16_t);

                    bytes_remaining -= read_bytes;
                    audio_buf_len[idx] += block_len;

                    /* enlarge audio buffer by alloc_increment if necessary */
                    if( actual_buf_size[idx] < audio_buf_len[idx] ) {
                        actual_buf_size[idx] += alloc_increment;
                        audio_buf[idx] = (int16_t*) realloc( audio_buf[idx],
                                        actual_buf_size[idx] * sizeof(int16_t) );
                    }
                    /* copy the new samples to the audio buffer */
                    memcpy(&audio_buf[idx][audio_buf_len[idx]-block_len], block_frame.data[idx], block_len*num_channels[idx]);
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
    avformat_close_input( &pFormatContext );

    for( idx = 0; idx < num_streams; ++idx )
        free( audio_buf[idx] );
    free( audio_buf );
    free( audio_buf_len );

    return;
}
