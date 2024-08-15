#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <stdlib.h>

#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

// static void start_timer(GtkWidget *);
// static void stop_timer(gint);
static gboolean update_frame(GtkWidget *);
static void draw_function(GtkDrawingArea *, cairo_t *, int, int, gpointer);
static void activate(GtkApplication *, gpointer);
static int decode_packet(AVCodecContext *, const AVPacket *);
static int open_codec_context(int *, AVCodecContext **, AVFormatContext *, enum AVMediaType);
int main(int ,char **);

//struct that stores the pixbuff data
struct Frames{
  GdkPixbuf *imgFrameData;
};

GdkPixbuf *currentFrame;
GdkColorspace colorspace;
gboolean has_alpha = FALSE;
int bits_per_sample = 24;
int rowstride = 1280;
GdkPixbufDestroyNotify destroy_fn = NULL;
gpointer destroy_fn_dat = NULL;

static int imgFrameNum = 0;
gint timer;
static gboolean isStarted = FALSE;
struct Frames frames[5];

AVFormatContext *fmt_ctx = NULL;
static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
static int width, height;
static enum AVPixelFormat pix_fmt;
static const char *src_filename = "../Documents/RickRoll.mp4";
static AVStream *video_stream = NULL, *audio_stream = NULL;
static uint8_t *video_dst_data[4] = {NULL};
static int video_dst_linesize[4];
static int video_dst_bufsize;
 
static int video_stream_idx = -1, audio_stream_idx = -1;
static AVFrame *frame = NULL;
static AVPacket *pkt = NULL;
static int video_frame_count = 0;
static int audio_frame_count = 0;



static void start_timer(GtkWidget *darea){
  if (!isStarted){
    timer = g_timeout_add(650, (GSourceFunc)update_frame, darea);
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

static gboolean update_frame(GtkWidget *darea){
  imgFrameNum++;
  if (imgFrameNum > 4)
    imgFrameNum = 0;
  gtk_widget_queue_draw(darea);
}

static void draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data){
  frames[0].imgFrameData = gdk_pixbuf_new_from_file("./Test_Images/TestImage1.png", NULL);
  frames[1].imgFrameData = gdk_pixbuf_new_from_file("./Test_Images/TestImage2.png", NULL);
  frames[2].imgFrameData = gdk_pixbuf_new_from_file("./Test_Images/TestImage3.png", NULL);
  frames[3].imgFrameData = gdk_pixbuf_new_from_file("./Test_Images/TestImage4.png", NULL);
  frames[4].imgFrameData = gdk_pixbuf_new_from_file("./Test_Images/TestImage5.png", NULL);

  // //updates the source surface with new pixbuf info
  // gdk_cairo_set_source_pixbuf(cr, frames[imgFrameNum].imgFrameData, 0,0);

  gdk_cairo_set_source_pixbuf(cr, currentFrame, 0,0);

  //paint the current surface containing the current image
  cairo_paint(cr);
}

static int output_video_frame(AVFrame *frame){
  if (frame->width != width || frame->height != height || frame->format != pix_fmt) {
      /* To handle this change, one could call av_image_alloc again and
        * decode the following frames into another rawvideo file. */
      fprintf(stderr, "Error: Width, height and pixel format have to be "
              "constant in a rawvideo file, but the width, height or "
              "pixel format of the input video changed:\n"
              "old: width = %d, height = %d, format = %s\n"
              "new: width = %d, height = %d, format = %s\n",
              width, height, av_get_pix_fmt_name(pix_fmt),
              frame->width, frame->height,
              av_get_pix_fmt_name(frame->format));
      return -1;
  }

  printf("video_frame n:%d coded_n: NA\n", video_frame_count++);

  /* copy decoded frame to destination buffer:
    * this is required since rawvideo expects non aligned data */
  av_image_copy(video_dst_data, video_dst_linesize, (const uint8_t **)(frame->data), frame->linesize, pix_fmt, width, height);

  //creating a new pixbuf of the current frame to send to the drawing function
  currentFrame = gdk_pixbuf_new_from_data((const uint8_t *)(frame->data), colorspace, has_alpha, bits_per_sample, width, height, rowstride, destroy_fn, destroy_fn_dat);

  return 0;
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
  while(ret >= 0) {
      ret = avcodec_receive_frame(dec, frame);
      if(ret < 0) {
          // those two return values are special and mean there is no output
          // frame available, but there were no errors during decoding
          if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
              return 0;

          fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
          return ret;
      }

      // write the frame data to output file
      if(dec->codec->type == AVMEDIA_TYPE_VIDEO)
          ret = output_video_frame(frame);
      else
          //ret = output_audio_frame(frame);

      av_frame_unref(frame);
      if(ret < 0)
          return ret;
  }

  return 0;
}

static int open_codec_context(int *stream_idx, AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type){
    int ret, stream_index;
    AVStream *st;
    const AVCodec *dec = NULL;
 
    ret = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (ret < 0) {
        fprintf(stderr, "Could not find %s stream in input file '%s'\n", av_get_media_type_string(type), src_filename);
        return ret;
    } else {
        stream_index = ret;
        st = fmt_ctx->streams[stream_index];
 
        /* find decoder for the stream */
        dec = avcodec_find_decoder(st->codecpar->codec_id);
        if (!dec) {
            fprintf(stderr, "Failed to find %s codec\n", av_get_media_type_string(type));
            return AVERROR(EINVAL);
        }
 
        /* Allocate a codec context for the decoder */
        *dec_ctx = avcodec_alloc_context3(dec);
        if (!*dec_ctx) {
            fprintf(stderr, "Failed to allocate the %s codec context\n", av_get_media_type_string(type));
            return AVERROR(ENOMEM);
        }
 
        /* Copy codec parameters from input stream to output codec context */
        if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
            fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n", av_get_media_type_string(type));
            return ret;
        }
 
        /* Init the decoders */
        if ((ret = avcodec_open2(*dec_ctx, dec, NULL)) < 0) {
            fprintf(stderr, "Failed to open %s codec\n", av_get_media_type_string(type));
            return ret;
        }
        *stream_idx = stream_index;
    }
 
    return 0;
}

int video_processor(){
  int ret = 0;

  // Open the file and allocaate the format context to the flie
  if(avformat_open_input(&fmt_ctx, src_filename, NULL, NULL) < 0){
    fprintf(stderr, "Could not open source file %s\n", src_filename);
    exit(1);
  }

  /* retrieve stream information 
      some file formats do not have headers or store enough information. To prevent issues from such cases
      it is important to call the avformat_find_stream_into() function, which reads and decodes a few frames to get
      missing information
  */
  if(avformat_find_stream_info(fmt_ctx, NULL) < 0) {
      fprintf(stderr, "Could not find stream information\n");
      exit(1);
  }

  //Decoding the video
  if(open_codec_context(&video_stream_idx, &video_dec_ctx, fmt_ctx, AVMEDIA_TYPE_VIDEO) >= 0) {
    video_stream = fmt_ctx->streams[video_stream_idx];

    /* allocate image where the decoded image will be put */
    width = video_dec_ctx->width;
    height = video_dec_ctx->height;
    pix_fmt = video_dec_ctx->pix_fmt;
    ret = av_image_alloc(video_dst_data, video_dst_linesize, width, height, pix_fmt, 1);
    if(ret < 0) {
        fprintf(stderr, "Could not allocate raw video buffer\n");
        exit(1);
    }
    video_dst_bufsize = ret;
  }

  //Decoding the audio
  /* dump input information to stderr */
  av_dump_format(fmt_ctx, 0, src_filename, 0);

  if(!audio_stream && !video_stream) {
      fprintf(stderr, "Could not find audio or video stream in the input, aborting\n");
      ret = 1;
      goto end;
  }

  frame = av_frame_alloc();
  if(!frame) {
      fprintf(stderr, "Could not allocate frame\n");
      ret = AVERROR(ENOMEM);
      goto end;
  }

  pkt = av_packet_alloc();
  if(!pkt) {
      fprintf(stderr, "Could not allocate packet\n");
      ret = AVERROR(ENOMEM);
      goto end;
  }

  /* read frames from the file */
  // while (av_read_frame(fmt_ctx, pkt) >= 0) {
  //     // check if the packet belongs to a stream we are interested in, otherwise
  //     // skip it
  //     if (pkt->stream_index == video_stream_idx)
  //         ret = decode_packet(video_dec_ctx, pkt);
  //     else if (pkt->stream_index == audio_stream_idx)
  //         ret = decode_packet(audio_dec_ctx, pkt);
  //     av_packet_unref(pkt);
  //     if (ret < 0)
  //         break;
  // }

  for(int i = 0; i < 5; i++){
    if(pkt->stream_index == video_stream_idx)
        ret = decode_packet(video_dec_ctx, pkt);
    else if(pkt->stream_index == audio_stream_idx)
        ret = decode_packet(audio_dec_ctx, pkt);
    av_packet_unref(pkt);
    if(ret < 0)
        break;
  }

  /* flush the decoders */
  if(video_dec_ctx)
      decode_packet(video_dec_ctx, NULL);
  if(audio_dec_ctx)
      decode_packet(audio_dec_ctx, NULL);

  printf("Demuxing succeeded.\n");

  // Free resources
  end:
    avcodec_free_context(&video_dec_ctx);
    avcodec_free_context(&audio_dec_ctx);
    avformat_close_input(&fmt_ctx);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    av_free(video_dst_data[0]);
 
    return ret < 0;
}

static void activate(GtkApplication *app, gpointer user_data){
  GtkWidget *window, *vbox, *darea, *da_container, *button_box, *play_button, *pause_button;

  //initializing the window
  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Testing Carousel");
  gtk_window_set_default_size(GTK_WINDOW(window), 1080, 800);

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
  //g_signal_connect_swapped(play_button, "clicked", G_CALLBACK(start_timer), darea);

  pause_button = gtk_button_new_with_label("Pause");
  gtk_box_append(GTK_BOX(button_box), pause_button);
  //g_signal_connect_swapped(pause_button, "clicked", G_CALLBACK(stop_timer), NULL);

  //int play = video_processor();
  
  gtk_window_present(GTK_WINDOW(window));
}

int main (int argc,char **argv){
  GtkApplication *app;
  int status;

  app = gtk_application_new("org.gtk.demo02", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
}
