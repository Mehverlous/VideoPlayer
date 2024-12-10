#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <gtk/gtk.h>
#include <SDL2/SDL.h>

#define FRAMES_TO_PROCESS 1000

#define FILE_PATH "./Documents/cheer_long.wav"
 
//struct that stores the pixbuff data
typedef struct {
    uint8_t *audio_data[4];
    int audio_samples;
    int a_num;
} Audio_Frames;

typedef struct{
  Audio_Frames *buffer;
  int head, tail, num_entries, max_len;
}circ_buf_a;

typedef struct {
	Uint8* pos;
	Uint32 length;
}AudioData;

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL, *audio_stream = NULL;
static const char *src_filename = "./Documents/RickRoll.mp4";

circ_buf_a aud_frame_buf;

SDL_AudioSpec audio_spec;
SDL_AudioDeviceID audio_device = 0;
 
static uint8_t *video_dst_data[4] = {NULL};
static int      video_dst_linesize[4];
static int video_dst_bufsize;
 
static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket *pkt = NULL;
static int video_frame_count = 0;
static int audio_frame_count = 0;

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

void enqueue_audio_buffer(circ_buf_a *b, AVFrame *frame, int iFrame){
    if(audio_buffer_full(b))
        return;

    for (int i = 0; i < 4; i++){
        b->buffer[b->tail].audio_data[i] = frame->extended_data[i];
    }

    //memcpy(b->buffer[b->tail].audio_data, frame->extended_data[0], sizeof(uint8_t)*4);

    b->buffer[b->tail].audio_samples = frame->nb_samples;
    b->buffer[b->tail].a_num = iFrame + 1;
    b->num_entries++;
    b->tail = (b->tail + 1) % b->max_len;
}

Audio_Frames* dequeue_audio_buffer(circ_buf_a *b) {
    if(audio_buffer_empty(b))
        return NULL;

    Audio_Frames* audio_frame = &b->buffer[b->head];
    b->num_entries--;
    b->head = (b->head + 1) % b->max_len;
    return audio_frame;
}

static int decode_packet(AVCodecContext *dec, const AVPacket *pkt)
{
    int i, ret = 0;
 
    // submit the packet to the decoder
    ret = avcodec_send_packet(dec, pkt);
    if (ret < 0) {
        fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
        return ret;
    }
 
    // get all the available frames from the decoder
    while (ret >= 0) {
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
        if (dec->codec->type == AVMEDIA_TYPE_AUDIO)
            enqueue_audio_buffer(&aud_frame_buf, frame, i++);
 
        av_frame_unref(frame);
    }
 
    return ret;
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
 
void MyAudioCallback(void* userdata, Uint8* stream, int streamLength)
{
	AudioData* audio = (AudioData*)userdata;

	if(audio->length == 0)
	{
		return;
	}

	Uint32 length = (Uint32)streamLength;
	length = (length > audio->length ? audio->length : length);

	SDL_memcpy(stream, audio->pos, length);

	audio->pos += length;
	audio->length -= length;
}

int main (int argc, char *argv[])
{
    int ret = 0;

    // init_audio_buffer(&aud_frame_buf, FRAMES_TO_PROCESS);

    // /* open input file, and allocate format context */
    // if (avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0) {
    //     fprintf(stderr, "Could not open source file %s\n", src_filename);
    //     exit(1);
    // }
 
    // /* retrieve stream information */
    // if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
    //     fprintf(stderr, "Could not find stream information\n");
    //     exit(1);
    // }
 
    // if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
    //     audio_stream = fmt_ctx->streams[audio_stream_idx];

    // }
 
    // /* dump input information to stderr */
    // av_dump_format(fmt_ctx, 0, src_filename, 0);
 
    // if (!audio_stream && !video_stream) {
    //     fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
    //     ret = 1;
    //     goto end;
    // }
 
    // frame = av_frame_alloc();
    // if (!frame) {
    //     fprintf(stderr, "Could not allocate frame\n");
    //     ret = AVERROR(ENOMEM);
    //     goto end;
    // }
 
    // pkt = av_packet_alloc();
    // if (!pkt) {
    //     fprintf(stderr, "Could not allocate packet\n");
    //     ret = AVERROR(ENOMEM);
    //     goto end;
    // }
 
    // /* read frames from the file */
    // while (av_read_frame(fmt_ctx, pkt) >= 0) {
    //     // check if the packet belongs to a stream we are interested in, otherwise
    //     // skip it
    //     if (pkt->stream_index == audio_stream_idx)
    //         ret = decode_packet(audio_dec_ctx, pkt);
    //     av_packet_unref(pkt);
    //     if (ret < 0)
    //         break;
    // }
 

    // /* flush the decoders */
    // if (video_dec_ctx)
    //     decode_packet(video_dec_ctx, NULL);
    // if (audio_dec_ctx)
    //     decode_packet(audio_dec_ctx, NULL);
 
    // printf("Demuxing succeeded.\n");

    //--------------------------------------------------Testing simple SDL Audio functionality-------------------------------------------------------------
    SDL_Init(SDL_INIT_AUDIO);

	SDL_AudioSpec wavSpec;
	Uint8* wavStart;
	Uint32 wavLength;

	if(SDL_LoadWAV(FILE_PATH, &wavSpec, &wavStart, &wavLength) < 0)
	{
		// TODO: Proper error handling
		printf("Could not open file path\n");
		return 1;
	}

	AudioData audio;
	audio.pos = wavStart;
	audio.length = wavLength;

	wavSpec.callback = MyAudioCallback;
	wavSpec.userdata = &audio;

	SDL_AudioDeviceID device = SDL_OpenAudioDevice(NULL, 0, &wavSpec, NULL, 0);
	if(device == 0)
	{
		// TODO: Proper error handling
		printf("Could not open audio device\n");
		return 1;
	}

	SDL_PauseAudioDevice(device, 0);

	while(audio.length > 0)
	{
		SDL_Delay(150);
	}

	SDL_CloseAudioDevice(device);
	SDL_FreeWAV(wavStart);
	SDL_Quit();
        
end:
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);
 
    return ret < 0;
}