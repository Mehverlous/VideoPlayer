#include <gtk/gtk.h>
#include <windows.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <portaudio.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include <pthread.h>

#define FRAMES_TO_PROCESS 1000
 
static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *audio_dec_ctx;
static int width, height;
static AVStream *audio_stream = NULL;
static const char *src_filename = "./Documents/ben10.mp4";
 
static int audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket *pkt = NULL;
static int audio_frame_count = 0;

static PaStream *pa_stream = NULL;
static PaError pa_err;
static int sample_rate = 0;
static const int channels = 2;
static const PaSampleFormat pa_sample_fmt = paFloat32;

typedef struct {
    uint8_t **audio_data;
    int audio_samples;
    int audio_channels;
    int audio_format;
} Audio_Frames;

typedef struct{
    Audio_Frames *buffer;
    int head, tail, num_entries, max_len;
}circ_buf_a;

circ_buf_a aud_frame_buf;

Audio_Frames aud_frame_data[FRAMES_TO_PROCESS];
int current_frame = 0;

gint vid_timer;
gint aud_timer;
static gboolean isStarted = FALSE;

void init_audio_buffer(circ_buf_a *b, int size){
    b->buffer = malloc(sizeof(Audio_Frames) * size);
    b->max_len = size;
    b->num_entries = 0;
    b->head = 0;
    b->tail= 0;
}

gboolean audio_buffer_empty(circ_buf_a *b){
    return (b->num_entries == 0);
}

gboolean audio_buffer_full(circ_buf_a *b){
    return (b->num_entries > (b->max_len)/2);
}

void enqueue_audio_buffer(circ_buf_a *b, AVFrame *frame){
    if(audio_buffer_full(b))
		return;

    // Allocate memory for the audio data pointers
    b->buffer[b->tail].audio_data = av_calloc(frame->ch_layout.nb_channels, sizeof(uint8_t*));
    if (!b->buffer[b->tail].audio_data) {
        fprintf(stderr, "Could not allocate audio data pointers\n");
        exit(EXIT_FAILURE);
    }

    // Allocate audio samples
    int ret = av_samples_alloc(b->buffer[b->tail].audio_data, NULL, frame->ch_layout.nb_channels, frame->nb_samples, AV_SAMPLE_FMT_FLTP , 0);
    if (ret < 0) {
        fprintf(stderr, "Could not allocate audio samples\n");
        av_free(b->buffer[b->tail].audio_data);
        exit(EXIT_FAILURE);
    }

    // Copy audio samples
    ret = av_samples_copy(b->buffer[b->tail].audio_data, frame->extended_data, 0, 0, frame->nb_samples, frame->ch_layout.nb_channels, AV_SAMPLE_FMT_FLTP );
    if (ret < 0) {
        fprintf(stderr, "Could not copy audio samples\n");
        av_freep(&b->buffer[b->tail].audio_data[0]);
        av_free(b->buffer[b->tail].audio_data);
        exit(EXIT_FAILURE);
    }

    b->buffer[b->tail].audio_samples = frame->nb_samples;
    b->buffer[b->tail].audio_channels = frame->ch_layout.nb_channels;
    b->num_entries++;
    b->tail = (b->tail + 1) % b->max_len;
}
 
static void output_audio_frame(){
    // Create an interleaved buffer for both channels
    float *interleaved_buffer = malloc(aud_frame_buf.buffer[aud_frame_buf.head].audio_samples * channels * sizeof(float));
    
    for(int i = 0; i < aud_frame_buf.buffer[aud_frame_buf.head].audio_samples; i++) {
        // Left channel
        interleaved_buffer[i * 2] = ((float*)aud_frame_buf.buffer[aud_frame_buf.head].audio_data[0])[i];
        // Right channel
        interleaved_buffer[i * 2 + 1] = ((float*)aud_frame_buf.buffer[aud_frame_buf.head].audio_data[1])[i];
    }

    // Write interleaved stereo data
    pa_err = Pa_WriteStream(pa_stream, interleaved_buffer, aud_frame_buf.buffer[aud_frame_buf.head].audio_samples);

    if (pa_err == paOutputUnderflowed) {
        // Handle underflow by waiting briefly
        Pa_Sleep(1);
    }else if (pa_err != paNoError) {
        fprintf(stderr, "Error writing audio to PortAudio stream (%s)\n", Pa_GetErrorText(pa_err));
        free(interleaved_buffer);
        exit(EXIT_FAILURE);
    }

    aud_frame_buf.head = (aud_frame_buf.head + 1) % aud_frame_buf.max_len;
	aud_frame_buf.num_entries--;
    free(interleaved_buffer);
}
 
static int decode_packet(AVCodecContext *dec, const AVPacket *pkt){
    int ret = 0;
 
    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }
 
    // get all the available frames from the decoder
    while (ret >= 0) {
        if(!audio_buffer_full(&aud_frame_buf)){
            ret = avcodec_receive_frame(dec, frame);

            if (ret < 0) {
                // those two return values are special and mean there is no output
                // frame available, but there were no errors during decoding
                if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
                    return 0;
    
                fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
                return ret;
            }
    
            // write the frame data to output file
            if (!(dec->codec->type == AVMEDIA_TYPE_VIDEO))
                enqueue_audio_buffer(&aud_frame_buf, frame);
    
            av_frame_unref(frame);
            if (ret < 0)
                return ret;
        } else {
            continue;
        }
    }
    return 0;
}
 
static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;
 
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n",
                av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
 
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n",
                    av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
 
        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n",
                    av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
 
        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
                    av_get_media_type_string(type));
            return ret;
        }
 
        /* Init the decoders */
        if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n",
                    av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }
 
    return 0;
}
 
static int get_format_from_sample_fmt(const char **fmt,
                                      enum AVSampleFormat sample_fmt)
{
    int i;
    struct sample_fmt_entry {
        enum AVSampleFormat sample_fmt; const char *fmt_be, *fmt_le;
    } sample_fmt_entries[] = {
        { AV_SAMPLE_FMT_U8,  "u8",    "u8"    },
        { AV_SAMPLE_FMT_S16, "s16be", "s16le" },
        { AV_SAMPLE_FMT_S32, "s32be", "s32le" },
        { AV_SAMPLE_FMT_FLT, "f32be", "f32le" },
        { AV_SAMPLE_FMT_DBL, "f64be", "f64le" },
    };
    *fmt = NULL;
 
    for (i = 0; i < FF_ARRAY_ELEMS(sample_fmt_entries); i++) {
        struct sample_fmt_entry *entry = &sample_fmt_entries[i];
        if (sample_fmt == entry->sample_fmt) {
            *fmt = AV_NE(entry->fmt_be, entry->fmt_le);
            return 0;
        }
    }
 
    fprintf(stderr,
            "sample format %s is not supported as output format\n",
            av_get_sample_fmt_name(sample_fmt));
    return -1;
}


void *Audio_Processing(){
    int ret = 0;

    init_audio_buffer(&aud_frame_buf, FRAMES_TO_PROCESS);


    /* open input file, and allocate format context */
    if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
        fprintf(stderr, "Could not open source file %s\n", src_filename);
        exit(1);
    }
 
    /* retrieve stream information */
    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        exit(1);
    }
 
    if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audio_stream = fmt_ctx->streams[audio_stream_idx];
        sample_rate = audio_dec_ctx->sample_rate;
    }
 
    /* dump input information to stderr */
    //av_dump_format(fmt_ctx, 0, src_filename, 0);
 
    frame = av_frame_alloc();
    if (!frame) {
        fprintf(stderr, "Could not allocate frame\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
 
    pkt = av_packet_alloc();
    if (!pkt) {
        fprintf(stderr, "Could not allocate packet\n");
        ret = AVERROR(ENOMEM);
        goto end;
    }
 
    // Initialize PortAudio
    pa_err = Pa_Initialize();
    if (pa_err != paNoError) {
        fprintf(stderr, "Error initializing PortAudio (%s)\n", Pa_GetErrorText(pa_err));
        exit(EXIT_FAILURE);
    }

    // Create output stream
    pa_err = Pa_OpenDefaultStream(&pa_stream, 0, channels, pa_sample_fmt, sample_rate, 1024, NULL, NULL);
    if (pa_err != paNoError) {
        fprintf(stderr, "Error creating PortAudio stream (%s)\n", Pa_GetErrorText(pa_err));
        exit(EXIT_FAILURE);
    }

    // Start output stream
    pa_err = Pa_StartStream(pa_stream);
    if (pa_err != paNoError) {
        fprintf(stderr, "Error starting PortAudio stream (%s)\n", Pa_GetErrorText(pa_err));
        exit(EXIT_FAILURE);
    }
 
    /* read frames from the file */
    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        // check if the packet belongs to a stream we are interested in, otherwise
        // skip it
        if (pkt->stream_index == audio_stream_idx)
            ret = decode_packet(audio_dec_ctx, pkt);
        av_packet_unref(pkt);
        if (ret < 0)
            break;
    }

    /* flush the decoders */
    if (audio_dec_ctx)
        decode_packet(audio_dec_ctx, NULL);
  

    // Clean up
    av_frame_free(&frame);
    av_packet_free(&pkt);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);

    
 
end:
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_packet_free(&pkt);
    av_frame_free(&frame);

}

void *Full_Display(){
    char play;

    printf("Please type 'p' to play the audio: ");
    scanf("%c", &play);

    if(play == 'p'){
        // Pplay decoded audio
        current_frame = 0;
        while (!audio_buffer_empty(&aud_frame_buf)){
            output_audio_frame();
            Pa_Sleep(1);
        }
    }

    Pa_StopStream(pa_stream);
    Pa_CloseStream(pa_stream);
    Pa_Terminate();
}

int main (int argc, char **argv){
    pthread_t audio_proc_thread, full_disp_thread;

    if(pthread_create(&audio_proc_thread, NULL, &Audio_Processing, NULL)){
        return 1;
    }

    if(pthread_create(&full_disp_thread, NULL, &Full_Display, NULL)){
        return 2;
    }

    if(pthread_join(audio_proc_thread, NULL)){
        return 3;
    }

    if(pthread_join(full_disp_thread, NULL)){
        return 4;
    }

    return 0;
}
