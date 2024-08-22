#include <cairo/cairo.h>
#include <gtk/gtk.h>
#include <stdlib.h>

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
void saveFrame(AVFrame *, int, int, int);
static void activate(GtkApplication *, gpointer);
int main(int ,char **);

//struct that stores the pixbuff data
typedef struct Frames{
  uint8_t *video_dst_data[4];
  int video_dst_linesize[4];
  int fwidth; 
  int fheight; 
  int fnum;
}Frames;

static uint8_t *test_dst_data[4] = {NULL};
static int      test_dst_linesize[4]; 

cairo_surface_t *currentFrame;
#define frames_to_process 5

// GdkColorspace colorspace;
// gboolean has_alpha = FALSE;
// int bits_per_sample = 24;
// int rowstride = 1280;
// GdkPixbufDestroyNotify destroy_fn = NULL;
// gpointer destroy_fn_dat = NULL;

// static int imgFrameNum = 0;
gint timer;
static gboolean isStarted = FALSE;
struct Frames frames[frames_to_process];
int currentImage = 0;

// AVFormatContext *fmt_ctx = NULL;
// static AVCodecContext *video_dec_ctx = NULL, *audio_dec_ctx;
// static int width, height;
// static enum AVPixelFormat pix_fmt;
static const char *src_filename = "./Documents/RickRoll.mp4";
// static AVStream *video_stream = NULL, *audio_stream = NULL;
// static uint8_t *video_dst_data[4] = {NULL};
// static int video_dst_linesize[4];
// static int video_dst_bufsize;
 
// static int video_stream_idx = -1, audio_stream_idx = -1;
// static AVFrame *frame = NULL;
// static AVPacket *pkt = NULL;
// static int video_frame_count = 0;
// static int audio_frame_count = 0;



static void start_timer(GtkWidget *darea){
  if (!isStarted){
    timer = g_timeout_add(17, (GSourceFunc)update_frame, darea);
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
  currentImage++;
  if (currentImage > frames_to_process)
    currentImage = 0;
  gtk_widget_queue_draw(darea);
}

static void draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data){
  //updates the source surface with new pixbuf info
  // char str[100] = {0};
  // char extension[] = ".png";
  // char directory[1000] = "./Test_Images/TestImage";
  // itoa(currentImage, str, 10);
  // strcat(str, extension);
  // strcat(directory, str);

  // GdkPixbuf *testing = gdk_pixbuf_new_from_file(directory, NULL);
  // gdk_cairo_set_source_pixbuf(cr, testing, 0,0);

  currentFrame = cairo_image_surface_create_for_data(frames[currentImage].video_dst_data[0], 
                                                      CAIRO_FORMAT_RGB24, 
                                                      frames[currentImage].fwidth, 
                                                      frames[currentImage].fheight, 
                                                      cairo_format_stride_for_width(CAIRO_FORMAT_RGB24, frames[currentImage].fwidth));

  cairo_set_source_surface(cr, currentFrame, 0, 0);

  //paint the current surface containing the current image
  cairo_paint(cr);
}

void update_buffer(AVFrame *pFrame, int width, int height, int iFrame){
  //av_image_copy2(frames[iFrame].video_dst_data, frames[iFrame].video_dst_linesize, pFrame->data, pFrame->linesize, AV_PIX_FMT_RGB24, width, height);
  av_image_alloc(frames[iFrame].video_dst_data, frames[iFrame].video_dst_linesize, width, height, AV_PIX_FMT_RGB24, 1);
  av_image_copy2(frames[iFrame].video_dst_data, frames[iFrame].video_dst_linesize, pFrame->data, pFrame->linesize, AV_PIX_FMT_RGB24, width, height);

  frames[iFrame].fwidth = width;
  frames[iFrame].fheight = height;
  frames[iFrame].fnum = iFrame + 1;
}


void saveFrame(AVFrame *pFrame, int width, int height, int iFrame){
  FILE *pFile;
  char szFilename[32];
  int  y;
  
  // Open file
  sprintf(szFilename, "./Test_Images/TestImage%d.png", iFrame);
  pFile = fopen(szFilename, "wb");
  if(pFile == NULL){
    fprintf(stderr, "Cannot open file");
    return;
  }
  
  // Write header
  fprintf(pFile, "P6\n%d %d\n%d\n", width, height, 255);
  
  // Write pixel data
  for(y = 0; y < height; y++)
    fwrite(pFrame->data[0] + y * pFrame->linesize[0], 1, width * 3, pFile);
  
  // Close file
  fclose(pFile);
}

int video_processor(){
  
  // Open video file
  AVFormatContext *pFormatCtx = avformat_alloc_context();
  if(!pFormatCtx){
    fprintf(stderr, "Could not allocate memory for format context");
    return -1;
  }

  if(avformat_open_input(&pFormatCtx, src_filename, NULL, NULL) != 0){
    fprintf(stderr, "Cannot open file");
    return -1; // Couldn't open file
  }
  
  // Retrieve stream information
  if(avformat_find_stream_info(pFormatCtx, NULL) < 0){
    fprintf(stderr, "Could not find stream information");
    return -1; // Couldn't find stream information
  }

  const AVCodec *pCodec = NULL;
  AVCodecParameters *pCodecParams = NULL;
  
  // Find the first video stream
  int videoStream = -1;

  for(int i = 0; i < pFormatCtx->nb_streams; i++){
    if(pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO){
      videoStream = i;
      pCodecParams = pFormatCtx->streams[i]->codecpar;
      pCodec = avcodec_find_decoder(pCodecParams->codec_id); //allocates decoder based on codec parameters
      break; // We want only the first video stream, the leave the other stream that might be present in the file
    }
  }

  if(videoStream == -1){
    fprintf(stderr, "Did not find a video stream");
    return -1;
  }

  // Get a pointer to the codec context for the video stream
  AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);
  if(pCodecCtx == NULL){
    fprintf(stderr, "Could not allocate video codec context");
    return -1;
  }

  // Fill the codec context from the codec parameters values
  if(avcodec_parameters_to_context(pCodecCtx, pCodecParams) < 0){
    fprintf(stderr, "Failed to copy codec params to codec context");
    return -1;
  }

  if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0){
    fprintf(stderr, "failed to open codec");
    return -1;
  }

  // Allocate video frame
  AVFrame *pFrame = av_frame_alloc();
  if(pFrame == NULL){
    fprintf(stderr, "Could not allocate video frame");
    return -1;
  }

  AVPacket *pPacket = av_packet_alloc();
  if(pPacket == NULL){
    fprintf(stderr, "Failed to allocate memory for AVPacket");
    return -1;
  }

  // Allocate an AVFrame structure
  AVFrame *pFrameRGB = av_frame_alloc();
  if(pFrameRGB == NULL){
    fprintf(stderr, "Could not allocate output video frame");
    return -1;
  }

  struct SwsContext *swsCtx =
    sws_getContext(
        pCodecCtx->width,
        pCodecCtx->height,
        pCodecCtx->pix_fmt,
        pCodecCtx->width,
        pCodecCtx->height,
        AV_PIX_FMT_RGB24,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );

  int i = 0;
  
  // Read frames and save first five frames to disk
  while (av_read_frame(pFormatCtx, pPacket) >= 0 && i < frames_to_process){
    // Is this a packet from the video stream?
    if(pPacket->stream_index == videoStream){
      // Decode video frame  
      if(avcodec_send_packet(pCodecCtx, pPacket) < 0){
        fprintf(stderr, "Error while sending packet to decoder.");
        return -1;
      }
      
      int frameFinished = 1;
      // Did we get a video frame?
      while(frameFinished >= 0 && i < frames_to_process) {
        frameFinished = avcodec_receive_frame(pCodecCtx, pFrame);
        // These two return values are special and mean there is no output
        // frame available, but there were no errors during decoding
        if(frameFinished == AVERROR(EAGAIN) || frameFinished == AVERROR_EOF)
          break;

        else if(frameFinished < 0){
          fprintf(stderr, "Error during decoding");
          return -1;
        }

        if(frameFinished >= 0){
          if (av_image_alloc(pFrameRGB->data, pFrameRGB->linesize, pCodecCtx->width, pCodecCtx->height, AV_PIX_FMT_RGB24, 1) < 0){
            fprintf(stderr, "Could not allocate output frame");
            return -1;
          }

          // Convert the image from its native format to RGB
          sws_scale(
              swsCtx,
              (const uint8_t* const*)pFrame->data,
              pFrame->linesize,
              0,
              pCodecCtx->height,
              pFrameRGB->data,
              pFrameRGB->linesize);

          // Save the frame to the array
          update_buffer(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i++);
          
	        // Save the frame to disk
	        //saveFrame(pFrameRGB, pCodecCtx->width, pCodecCtx->height, ++i);
        }
      }
    }

    // Free the packet that was allocated by av_read_frame
    av_packet_unref(pPacket);
  }

  sws_freeContext(swsCtx);

  // Free the YUV frame
  av_frame_free(&pFrame);

  // Free the RGB image
  av_frame_free(&pFrameRGB);

  // Close the codec
  avcodec_free_context(&pCodecCtx);

  // Close the video file
  avformat_close_input(&pFormatCtx);
  
  return 0;
}

static void activate(GtkApplication *app, gpointer user_data){
  if(video_processor() < 0)
    return;

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
  g_signal_connect_swapped(play_button, "clicked", G_CALLBACK(start_timer), darea);

  pause_button = gtk_button_new_with_label("Pause");
  gtk_box_append(GTK_BOX(button_box), pause_button);
  g_signal_connect_swapped(pause_button, "clicked", G_CALLBACK(stop_timer), NULL);
  
  gtk_window_present(GTK_WINDOW(window));
}

int main (int argc,char **argv){
  GtkApplication *app;
  int status;

  app = gtk_application_new("org.gtk.videoplayer", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);

  return status;
}
