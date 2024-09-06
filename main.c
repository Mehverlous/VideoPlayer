#include <cairo/cairo.h> 
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

static void start_timer(GtkWidget *);
static void stop_timer();
static void update_frame(GtkWidget *);
static void draw_function(GtkDrawingArea *, cairo_t *, int, int, gpointer);
static void update_buffer(AVFrame *, int, int, int);
static void activate(GtkApplication *, gpointer);
int main(int, char **);

//struct that stores the pixbuff data
typedef struct Frames{
  uint8_t *video_dst_data[4];
  int video_dst_linesize[4];
  int fwidth; 
  int fheight; 
  int fnum;
}Frames;

typedef struct {
  Frames *buffer;
  int head, tail, num_entries, max_len;
}circ_buf_t;

#define frames_to_process 1000

gint timer;
static gboolean isStarted = FALSE;
struct Frames frames[frames_to_process];
int currentImage = 0;
static const char *src_filename = "./Documents/RickRoll.mp4";
circ_buf_t vid_frame_buf;
GdkPixbuf *currentFrame = NULL;
pthread_mutex_t mutex;

static AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static int width, height;
static enum AVPixelFormat pix_fmt;
static AVStream *video_stream = NULL, *audio_stream = NULL;
 
static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket *pkt = NULL;
static int video_frame_count = 0;
static int audio_frame_count = 0;

void init_buffer(circ_buf_t *b, int size){
  b->buffer = malloc(sizeof(Frames) * size);
  b->max_len = size;
  b->num_entries = 0;
  b->head = 0;
  b->tail= 0;
}

int buffer_empty(circ_buf_t *b){
  if(b->num_entries == 0)
    return 1;
  return 0;
}

int buffer_full(circ_buf_t *b){
  if(b->num_entries == b->max_len)
    return 1;
  return 0;
}

void enqueue_buffer(circ_buf_t *b, AVFrame *frame, int iFrame){
  if(buffer_full(b) == 1){
    printf("Buffer full size: %d\n", b->num_entries);
    return;
  }

  av_image_alloc(b->buffer[b->tail].video_dst_data, b->buffer[b->tail].video_dst_linesize, width, height, AV_PIX_FMT_RGB24, 1);
  av_image_copy2(b->buffer[b->tail].video_dst_data, b->buffer[b->tail].video_dst_linesize, frame->data, frame->linesize, pix_fmt, width, height);

  b->buffer[b->tail].fwidth = width;
  b->buffer[b->tail].fheight = height;
  b->buffer[b->tail].fnum = iFrame + 1;

  b->buffer[b->tail].video_dst_data;
  b->num_entries++;
  b->tail = (b->tail + 1) % b->max_len;
}

void dequeue_buffer(circ_buf_t *b){
  if(buffer_empty(b) == 1){
    printf("Buffer Empty\n");
    return;
  }

  currentFrame = gdk_pixbuf_new_from_data(b->buffer[b->head].video_dst_data[0], 
                                          GDK_COLORSPACE_RGB, 
                                          FALSE, 
                                          8, 
                                          b->buffer[b->head].fwidth, 
                                          b->buffer[b->head].fheight, 
                                          b->buffer[b->head].video_dst_linesize[0], 
                                          NULL, 
                                          NULL);

  b->head = (b->head + 1) % b->max_len;
  b->num_entries--;
}

static int decode_packet(AVCodecContext *dec, const AVPacket *pkt){
  int i, ret = 0;
  // submit the packet to the decoder
  ret = avcodec_send_packet(dec, pkt);
  if(ret < 0){
    fprintf(stderr, "Error while sending packet to decoder.");
    return ret;
  }
      
  // get all available frames from the decoder
  while(ret >= 0) {
    ret = avcodec_receive_frame(dec, frame);
    if(ret < 0){
      // These two return values are special and mean there is no output
      // frame available, but there were no errors during decoding
      if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        return 0;

      fprintf(stderr, "Error during decoding");
      return ret;
    }

    // Save the frame to the array
    pthread_mutex_lock(&mutex);
    enqueue_buffer(&vid_frame_buf, frame, i++);
    pthread_mutex_unlock(&mutex);

    av_frame_unref(frame);
  }
  return ret;
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

int video_processor(){
  int ret = 0;

  init_buffer(&vid_frame_buf, frames_to_process);
  
  // Open video file and allocate format context
  fmt_ctx = avformat_alloc_context();
  if(!fmt_ctx){
    fprintf(stderr, "Could not allocate memory for format context");
    return -1;
  }

  if(avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) != 0){
    fprintf(stderr, "Cannot open file");
    return -1;
  }
  
  // Retrieve stream information
  if(avformat_find_stream_info(fmt_ctx, NULL) < 0){
    fprintf(stderr, "Could not find stream information");
    return -1;
  }

  if (open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
    video_stream = fmt_ctx->streams[video_stream_idx];

    // allocate image where the decoded image will be put
    width = video_dec_ctx->width;
    height = video_dec_ctx->height;
    pix_fmt = video_dec_ctx->pix_fmt;

  }
 
  // if (open_codec_context(&audio_stream_idx, &audio_dec_ctx, fmt_ctx, AVMEDIA_TYPE_AUDIO) >= 0) {
  //   audio_stream = fmt_ctx->streams[audio_stream_idx];
  //   audio_dst_file = fopen(audio_dst_filename, "wb");
  //   if (!audio_dst_file) {
  //       fprintf(stderr, "Could not open destination file %s\n", audio_dst_filename);
  //       ret = 1;
  //       goto end;
  //   }
  // }

  // dump info to stderr
  av_dump_format(fmt_ctx, 0, src_filename, 0);

  if (/*!audio_stream &&*/ !video_stream) {
      fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
      ret = 1;
      goto end;
  }

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


  int i = 0;
  
  while (av_read_frame(fmt_ctx, pkt) >= 0){
    // Is this a packet from the video stream?
    if(pkt->stream_index == video_stream_idx)
      ret = decode_packet(video_dec_ctx, pkt);
    if(ret < 0)
      break;
  }

  // Flushing the decoders
  if(video_dec_ctx)
    decode_packet(video_dec_ctx, NULL);
  if(audio_dec_ctx)
    decode_packet(audio_dec_ctx, NULL);

  
  end: 
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_packet_free(&pkt);
    av_frame_free(&frame);

    return ret < 0;
}

static void start_timer(GtkWidget *darea){
  if (!isStarted){
    timer = g_timeout_add(18, (GSourceFunc)update_frame, darea);
    isStarted = TRUE;
    printf("Starting Timer: %d\n", timer);
  }
}

static void stop_timer(){
  if(isStarted){
    g_source_remove(timer);
    isStarted = FALSE;
    printf("Stopping Timer: %d\n", timer);
  }
}

static void update_frame(GtkWidget *darea){
  pthread_mutex_lock(&mutex);
  dequeue_buffer(&vid_frame_buf);
  pthread_mutex_unlock(&mutex);
  gtk_widget_queue_draw(darea);
  
}

static void draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data){
  gdk_cairo_set_source_pixbuf(cr, currentFrame, 0, 0);

  //paint the current surface containing the current image
  cairo_paint(cr);
}

static void activate(GtkApplication *app, gpointer user_data){
  GtkWidget *window, *vbox, *darea, *da_container, *button_box, *play_button, *pause_button;

  currentFrame = gdk_pixbuf_new_from_file("./Documents/Empty.png", NULL);

  //initializing the window
  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Testing Carousel");
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
  
  gtk_window_present(GTK_WINDOW(window));
}

int video_display(int argc, char **argv){
  GtkApplication *app;
  int status;

  app = gtk_application_new("org.gtk.videoplayer", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
}

int main (int argc,char **argv){
  pthread_t decode, display;
  pthread_mutex_init(&mutex, NULL);

  pthread_create(&decode, NULL, (void *)video_processor, NULL);
  pthread_create(&display, NULL, (void *)video_display, NULL);

  pthread_join(decode, NULL);
  pthread_join(display, NULL);

  pthread_mutex_destroy(&mutex);
  return 0;
}
