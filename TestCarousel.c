#include <cairo.h>
#include <gtk/gtk.h>
#include <stdlib.h>

int counter = 1;
gint timer;

static void start_timer();
static void draw_function (GtkDrawingArea *, cairo_t *, int, int, gpointer);
static void increment_counter(GtkWidget *);

static void start_timer(GtkWidget *darea){
  timer = g_timeout_add(650, (GSourceFunc) increment_counter, darea);
}

static void stop_timer(){
  g_source_remove(timer);
}

static void increment_counter(GtkWidget *darea){
  if(counter > 4){
    counter = 1;
  }else{
    counter++;
  }

  gtk_widget_queue_draw(darea);
}

static void draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data){
  cairo_surface_t *image1 = cairo_image_surface_create_from_png("Test_Images/TestImage1.png");
  cairo_surface_t *image2 = cairo_image_surface_create_from_png("Test_Images/TestImage2.png");
  cairo_surface_t *image3 = cairo_image_surface_create_from_png("Test_Images/TestImage3.png");
  cairo_surface_t *image4 = cairo_image_surface_create_from_png("Test_Images/TestImage4.png");
  cairo_surface_t *image5 = cairo_image_surface_create_from_png("Test_Images/TestImage5.png");

  cairo_surface_t *painted_image = cairo_image_surface_create_from_png("Test_Images/TestImage1.png");

  if(counter == 1){
    painted_image = image1;
  }else if (counter == 2){
    painted_image = image2;
  }else if (counter == 3){
    painted_image = image3;
  }else if (counter == 4){
    painted_image = image4;
  }else if (counter == 5){
    painted_image = image5;
  }

  cairo_set_source_surface(cr, painted_image, 75, 0);
  cairo_paint(cr);
}

static void activate(GtkApplication *app, gpointer user_data){
  GtkWidget *window, *start_timer_button,*stop_timer_button, *frame, *box, *dareaFrame, *darea;

  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Slideshow");
  gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);

  frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_halign(frame, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(frame, GTK_ALIGN_CENTER);

  box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_START);

  gtk_window_set_child(GTK_WINDOW(window), frame);

  dareaFrame = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_halign(box, GTK_ALIGN_CENTER);
  gtk_widget_set_valign(box, GTK_ALIGN_CENTER);

  darea = gtk_drawing_area_new();
  gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (darea), 750);
  gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (darea), 400);
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (darea), draw_function, NULL, NULL);

  gtk_box_append(GTK_BOX(dareaFrame), darea);

  start_timer_button = gtk_button_new_with_label("Start Timer");
  gtk_box_append(GTK_BOX(box), start_timer_button);
  g_signal_connect_swapped(start_timer_button, "clicked", G_CALLBACK(start_timer), darea);

  stop_timer_button = gtk_button_new_with_label("Stop Timer");
  gtk_box_append(GTK_BOX(box), stop_timer_button);
  g_signal_connect_swapped(stop_timer_button, "clicked", G_CALLBACK(stop_timer), NULL);

  gtk_box_append(GTK_BOX(frame), dareaFrame);
  gtk_box_append(GTK_BOX(frame), box);

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
