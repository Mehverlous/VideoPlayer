#include <cairo.h>
#include <gtk/gtk.h>
#include <stdlib.h>

static void start_timer(GtkWidget *);
static void stop_timer(gint);
static gboolean update_frame(GtkWidget *);
static void draw_function(GtkDrawingArea *, cairo_t *, int, int, gpointer);
static void activate(GtkApplication *, gpointer);
int main (int ,char **);

//struct that stores the pixbuff data
struct Frames{
  GdkPixbuf *imgFrameData;
};

static int imgFrameNum = 0;
gint timer;
static gboolean isStarted = FALSE;
struct Frames frames[5];

static void start_timer(GtkWidget *darea){
  if (!isStarted){
    timer = g_timeout_add(650, (GSourceFunc)update_frame, darea);
    isStarted = TRUE;
    printf("Starting Timer: %d\n", timer);
  }
}

//
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

  //updates the source surface with new pixbuf info
  gdk_cairo_set_source_pixbuf(cr, frames[imgFrameNum].imgFrameData, 0,0);

  //paint the current surface containing the current image
  cairo_paint(cr);
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
  g_signal_connect_swapped(play_button, "clicked", G_CALLBACK(start_timer), darea);

  pause_button = gtk_button_new_with_label("Pause");
  gtk_box_append(GTK_BOX(button_box), pause_button);
  g_signal_connect_swapped(pause_button, "clicked", G_CALLBACK(stop_timer), NULL);

  
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
