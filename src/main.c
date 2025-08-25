#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <math.h>

#include <windows.h>

#include <cairo/cairo.h> 
#include <gtk/gtk.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include <portaudio.h>

//struct that stores the pixbuff data
typedef struct{
	uint8_t *video_dst_data[4];
	int video_dst_linesize[4];
	int fwidth; 
	int fheight; 
	int fnum;
}Video_Frames;

typedef struct {
    uint8_t **audio_data;
    int audio_samples;
    int audio_channels;
    int audio_format;
} Audio_Frames;

typedef struct {
	Video_Frames *buffer;
	int head, tail, num_entries, max_len;
}circ_buf_v;

typedef struct{
	Audio_Frames *buffer;
	int head, tail, num_entries, max_len;
}circ_buf_a;

#define FRAMES_TO_PROCESS 1000
#define FRAMES_PER_BUFFER 2048
#define MIN_AUDIO_BUFFER_SIZE 5

static void start_timer(GtkWidget *);
static void stop_timer();
static void update_frame(GtkWidget *);
static void play_audio();
static void draw_function(GtkDrawingArea *, cairo_t *, int, int, gpointer);
static void activate(GtkApplication *, gpointer);
static int decode_video_packet(AVCodecContext *, const AVPacket *);
void enqueue_video_buffer(circ_buf_v *, AVFrame *, int);
static int decode_audio_packet(AVCodecContext *, const AVPacket *);
void enqueue_audio_buffer(circ_buf_a *b, AVFrame *frame);
static int open_codec_context(int *, AVCodecContext **, AVFormatContext *, enum AVMediaType);
int main(int, char **);


gint vid_timer;
gint aud_timer;
static gboolean isStarted = FALSE;
static const char *src_filename;
circ_buf_v vid_frame_buf;
circ_buf_a aud_frame_buf;
GdkPixbuf *currentFrame = NULL;
pthread_mutex_t mutex;
struct SwsContext *swsCtx;

static AVFormatContext *vid_fmt_ctx = NULL;
static AVFormatContext *aud_fmt_ctx = NULL;

static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static int width, height;
static AVStream *video_stream = NULL, *audio_stream = NULL;
 
static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *vid_frame = NULL;
static AVFrame *aud_frame = NULL;
static AVFrame *vid_frameRGB = NULL;
static AVPacket *vid_pkt = NULL;
static AVPacket *aud_pkt = NULL;
static AVRational fps; 
static int video_frame_count = 0;
static int audio_frame_count = 0;

static PaStream *pa_stream = NULL;
static PaError pa_err;
static int sample_rate = 0;
static const int channels = 2;
static const PaSampleFormat pa_sample_fmt = paFloat32;

//TODO: The file dialog box takes a long time to load. Figure out how to make that process faster
static void store_file_path(GObject *source_object, GAsyncResult *result, gpointer user_data) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
    GFile *file = gtk_file_dialog_open_finish(dialog, result, NULL);
    
    if (file) {
        src_filename = g_file_get_path(file);
        printf("Selected file: %s\n", src_filename);
        g_object_unref(file);
    }
}

void select_video(GtkWidget *window) {
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Open File");
    gtk_file_dialog_open(dialog, GTK_WINDOW(window), NULL, store_file_path, NULL);
}

void init_video_buffer(circ_buf_v *b, int size){
	b->buffer = malloc(sizeof(Video_Frames) * size);
	b->max_len = size;
	b->num_entries = 0;
	b->head = 0;
	b->tail= 0;
}

gboolean video_buffer_empty(circ_buf_v *b){
	return (b->num_entries == 0);
}

gboolean video_buffer_full(circ_buf_v *b){
	return (b->num_entries > (b->max_len)/2);
}

static int decode_video_packet(AVCodecContext *dec, const AVPacket *pkt){
	int i, ret = 0;
	// submit the packet to the decoder
	ret = avcodec_send_packet(dec, pkt);
	if(ret < 0){
		fprintf(stderr, "Error while sending packet to decoder.");
		return ret;
	}

	// get all available frames from the decoder
	while(ret >= 0) {
		if(!video_buffer_full(&vid_frame_buf)){
			ret = avcodec_receive_frame(dec, vid_frame);
			if(ret < 0){
				// These two return values are special and mean there is no output
				// frame available, but there were no errors during decoding
				if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				return 0;

				fprintf(stderr, "Error during decoding");
				return ret;
			}

			if(dec->codec->type == AVMEDIA_TYPE_VIDEO){
				if(av_image_alloc(vid_frameRGB->data, vid_frameRGB->linesize, video_dec_ctx->width, video_dec_ctx->height, AV_PIX_FMT_RGB24, 1)){
					// Convert the image from its native format to RGB    
					sws_scale(swsCtx,
					(const uint8_t* const*)vid_frame->data,
					vid_frame->linesize,
					0,
					video_dec_ctx->height,
					vid_frameRGB->data,
					vid_frameRGB->linesize);

					// Save the frame to the array
					enqueue_video_buffer(&vid_frame_buf, vid_frameRGB, i++);
				}
				else{
					fprintf(stderr, "Could not allocate output frame");
					return -1;
				}
			}
		}else{
			continue;
		}

		av_frame_unref(vid_frame);
	}
	return ret;
}

//TODO: Synchronicity issue with video and audio playback 
void enqueue_video_buffer(circ_buf_v *b, AVFrame *frame, int iFrame){
	if(video_buffer_full(b))
		return;

	pthread_mutex_lock(&mutex);

	av_image_alloc(b->buffer[b->tail].video_dst_data, b->buffer[b->tail].video_dst_linesize, width, height, AV_PIX_FMT_RGB24, 1);
	av_image_copy2(b->buffer[b->tail].video_dst_data, b->buffer[b->tail].video_dst_linesize, frame->data, frame->linesize, AV_PIX_FMT_RGB24, width, height);

	b->buffer[b->tail].fwidth = width;
	b->buffer[b->tail].fheight = height;
	b->buffer[b->tail].fnum = iFrame + 1;

	b->buffer[b->tail].video_dst_data;
	b->num_entries++;
	b->tail = (b->tail + 1) % b->max_len;

	pthread_mutex_unlock(&mutex);
}

static void update_frame(GtkWidget *darea){
	pthread_mutex_lock(&mutex);

	if(video_buffer_empty(&vid_frame_buf))
		return;

	currentFrame = gdk_pixbuf_new_from_data(vid_frame_buf.buffer[vid_frame_buf.head].video_dst_data[0], 
											GDK_COLORSPACE_RGB, 
											FALSE, 
											8, 
											vid_frame_buf.buffer[vid_frame_buf.head].fwidth, 
											vid_frame_buf.buffer[vid_frame_buf.head].fheight, 
											vid_frame_buf.buffer[vid_frame_buf.head].video_dst_linesize[0], 
											NULL, 
											NULL);

	vid_frame_buf.head = (vid_frame_buf.head + 1) % vid_frame_buf.max_len;
	vid_frame_buf.num_entries--;
	pthread_mutex_unlock(&mutex);

	gtk_widget_queue_draw(darea);

}

int Video_Processing_thd(){
	int ret = 0;

	init_video_buffer(&vid_frame_buf, FRAMES_TO_PROCESS);

	if (open_codec_context(&video_stream_idx, &video_dec_ctx, vid_fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
		video_stream = vid_fmt_ctx->streams[video_stream_idx];
		fps = vid_fmt_ctx->streams[video_stream_idx]->r_frame_rate;

		// allocate image where the decoded image will be put
		width = video_dec_ctx->width;
		height = video_dec_ctx->height;

		swsCtx = sws_getContext(video_dec_ctx->width,
								video_dec_ctx->height,
								video_dec_ctx->pix_fmt,
								video_dec_ctx->width,
								video_dec_ctx->height,
								AV_PIX_FMT_RGB24,
								SWS_BILINEAR,
								NULL,
								NULL,
								NULL);

		printf("Successfully openned video context\n");
	}	

	int i = 0;

	while (av_read_frame(vid_fmt_ctx, vid_pkt) >= 0){
		// Is this a packet from the video stream?
		if(vid_pkt->stream_index == video_stream_idx)
			ret = decode_video_packet(video_dec_ctx, vid_pkt);

		av_packet_unref(vid_pkt);

		if(ret < 0)
			break;
	}

	// Flushing the decoders
	if(video_dec_ctx)
		decode_video_packet(video_dec_ctx, NULL);
  
	return ret < 0;
}

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

static int decode_audio_packet(AVCodecContext *dec, const AVPacket *pkt){
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
            ret = avcodec_receive_frame(dec, aud_frame);

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
                enqueue_audio_buffer(&aud_frame_buf, aud_frame);
    
            av_frame_unref(aud_frame);
            if (ret < 0)
                return ret;
        } else {
            continue;
        }
    }
    return 0;
}

//TODO: Adding mutex locks to enqueue_audio_buffer() and play_audio() causes playback to hault
void enqueue_audio_buffer(circ_buf_a *b, AVFrame *frame){
	if(audio_buffer_full(b))
		return;

	//pthread_mutex_lock(&mutex);
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

	//pthread_mutex_unlock(&mutex);

}

static void play_audio() {
	//pthread_mutex_lock(&mutex);

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
    }else if(pa_err != paNoError) {
        fprintf(stderr, "Error writing audio to PortAudio stream (%s)\n", Pa_GetErrorText(pa_err));
        free(interleaved_buffer);
        exit(EXIT_FAILURE);
    }

	aud_frame_buf.head = (aud_frame_buf.head + 1) % aud_frame_buf.max_len;
	aud_frame_buf.num_entries--;
    free(interleaved_buffer);

	//pthread_mutex_unlock(&mutex);
}

int Audio_Processing_thd(){
    int ret = 0;

    init_audio_buffer(&aud_frame_buf, FRAMES_TO_PROCESS);

    if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, aud_fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
        audio_stream = aud_fmt_ctx->streams[audio_stream_idx];
        sample_rate = audio_dec_ctx->sample_rate;

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

		printf("Successfully openned audio context\n");
    }

    /* read frames from the file */
    while (av_read_frame(aud_fmt_ctx, aud_pkt) >= 0) {
        // check if the packet belongs to a stream we are interested in, otherwise
        // skip it
        if (aud_pkt->stream_index == audio_stream_idx)
            ret = decode_audio_packet(audio_dec_ctx, aud_pkt);
        av_packet_unref(aud_pkt);
        if (ret < 0)
            break;
    }

    /* flush the decoders */
    if (audio_dec_ctx)
        decode_audio_packet(audio_dec_ctx, NULL);
}

static int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type){
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

static void start_timer(GtkWidget *darea){
	double frame_rate = fps.num / fps.den;
	double interval = 9;

	if (!isStarted){
		vid_timer = g_timeout_add(frame_rate, (GSourceFunc)update_frame, darea);
		aud_timer = g_timeout_add(interval, (GSourceFunc)play_audio, NULL);

		isStarted = TRUE;
		printf("Starting Video Timer: %d\nStarting Audio Timer: %d\n", vid_timer, aud_timer);
	}
}

static void stop_timer(){
	if(isStarted){
		g_source_remove(vid_timer);
		g_source_remove(aud_timer);
		isStarted = FALSE;
		printf("Stopping Timer: %d\nStopping Timer2: %d\n", vid_timer, aud_timer);
	}
}

static void draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data){
	gdk_cairo_set_source_pixbuf(cr, currentFrame, 0, 0);

	//paint the current surface containing the current image
	cairo_paint(cr);
}

static void activate(GtkApplication *app, gpointer user_data){
	GtkWidget *window, *vbox, *darea, *da_container, *button_box, *play_button, *pause_button, *select_button;

	currentFrame = gdk_pixbuf_new_from_file("./Documents/Empty.png", NULL);

	//initializing the window
	window = gtk_application_window_new(app);
	gtk_window_set_title(GTK_WINDOW(window), "Video Player");
	gtk_window_set_default_size(GTK_WINDOW(window), 1280, 800);

	//creating a container to store the main widgets (drawing area and buttons)
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
	gtk_widget_set_margin_start(vbox, 10);
	gtk_widget_set_margin_end(vbox, 10);
	gtk_widget_set_margin_top(vbox, 10);
	gtk_widget_set_margin_bottom(vbox, 10);
	gtk_window_set_child(GTK_WINDOW(window), vbox);

	//creating frame to outline the position of the drawing area
	da_container = gtk_frame_new(NULL);
	gtk_widget_set_vexpand(da_container, TRUE);
	gtk_widget_set_hexpand(da_container, TRUE);
	gtk_box_append(GTK_BOX(vbox), da_container);

	//initializing the drawing area
	darea = gtk_drawing_area_new();
	gtk_frame_set_child(GTK_FRAME(da_container), darea);
	gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(darea), 1050);
	gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(darea), 700);
	gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(darea), draw_function, NULL, NULL);

	//container to store the buttons 
	button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	gtk_box_append(GTK_BOX(vbox), button_box);

	play_button = gtk_button_new_with_label("Play");
	gtk_box_append(GTK_BOX(button_box), play_button);
	g_signal_connect_swapped(play_button, "clicked", G_CALLBACK(start_timer), darea);

	pause_button = gtk_button_new_with_label("Pause");
	gtk_box_append(GTK_BOX(button_box), pause_button);
	g_signal_connect_swapped(pause_button, "clicked", G_CALLBACK(stop_timer), NULL);

	select_button = gtk_button_new_with_label("Select Video");
	gtk_box_append(GTK_BOX(button_box), select_button);
	g_signal_connect_swapped(select_button, "clicked", G_CALLBACK(select_video), window);

	gtk_window_present(GTK_WINDOW(window));
}

int Main_Window(int argc, char **argv){
	GtkApplication *app;
	int status;

	app = gtk_application_new("org.gtk.videoplayer", G_APPLICATION_DEFAULT_FLAGS);
	g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
	status = g_application_run(G_APPLICATION(app), argc, argv);
	g_object_unref(app);

	return status;
}

int main (int argc,char **argv){
	pthread_t main_window, audio_decode, video_decode;
	pthread_mutex_init(&mutex, NULL);
	int ret; 

	pthread_create(&main_window, NULL, (void *)Main_Window, NULL);

	//TODO: Find a cleaner way of waiting for the Main Window thread to update the src_filename variable
	while(!src_filename){}

	// Open video file and allocate format context
	vid_fmt_ctx = avformat_alloc_context();
	if(!vid_fmt_ctx){
		fprintf(stderr, "Could not allocate memory for format context");
		return -1;
	}

	if(avformat_open_input(&vid_fmt_ctx, src_filename, NULL, NULL) != 0){
		fprintf(stderr, "Cannot open file");
		return -1;
	}

	// Retrieve stream information
	if(avformat_find_stream_info(vid_fmt_ctx, NULL) < 0){
		fprintf(stderr, "Could not find stream information");
		return -1;
	}

	vid_frame = av_frame_alloc();
	if (!vid_frame) {
		fprintf(stderr, "Could not allocate frame\n");
		ret = AVERROR(ENOMEM);
		goto end;
	}

	vid_frameRGB = av_frame_alloc();
	if(!vid_frameRGB){
		fprintf(stderr, "Could not allocate RGB frame\n");
		ret = AVERROR(ENOMEM);
		goto end;
	}

	vid_pkt = av_packet_alloc();
	if (!vid_pkt) {
		fprintf(stderr, "Could not allocate packet\n");
		ret = AVERROR(ENOMEM);
		goto end;
	}

	// Open video file and allocate format context
	aud_fmt_ctx = avformat_alloc_context();
	if(!aud_fmt_ctx){
		fprintf(stderr, "Could not allocate memory for format context");
		return -1;
	}

	if(avformat_open_input(&aud_fmt_ctx, src_filename, NULL, NULL) != 0){
		fprintf(stderr, "Cannot open file");
		return -1;
	}

	// Retrieve stream information
	if(avformat_find_stream_info(aud_fmt_ctx, NULL) < 0){
		fprintf(stderr, "Could not find stream information");
		return -1;
	}

	aud_frame = av_frame_alloc();
	if (!aud_frame) {
		fprintf(stderr, "Could not allocate frame\n");
		ret = AVERROR(ENOMEM);
		goto end;
	}

	aud_pkt = av_packet_alloc();
	if (!aud_pkt) {
		fprintf(stderr, "Could not allocate packet\n");
		ret = AVERROR(ENOMEM);
		goto end;
	}
	

	pthread_create(&video_decode, NULL, (void *)Video_Processing_thd, NULL);
	pthread_create(&audio_decode, NULL, (void *)Audio_Processing_thd, NULL);

	pthread_join(video_decode, NULL);
	pthread_join(audio_decode, NULL);
	pthread_join(main_window, NULL);

	pthread_mutex_destroy(&mutex);

	Pa_StopStream(pa_stream);
	Pa_CloseStream(pa_stream);
	Pa_Terminate();

	end: 
		sws_freeContext(swsCtx);
		avcodec_free_context(&video_dec_ctx);
		avcodec_free_context(&audio_dec_ctx);
		avformat_close_input(&vid_fmt_ctx);
		av_packet_free(&vid_pkt);
		av_frame_free(&vid_frame);
		av_frame_free(&vid_frameRGB);
		avformat_close_input(&aud_fmt_ctx);
		av_packet_free(&aud_pkt);
		av_frame_free(&aud_frame);

	return 0;
}
