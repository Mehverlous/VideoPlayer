#include <cairo.h>
#include <gtk/gtk.h>
#include <stdlib.h>

static void
draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data){
  GdkPixbuf *pix;
  pix = gdk_pixbuf_new_from_file("./Test_Images/TestImage2.png", NULL);
  gdk_cairo_set_source_pixbuf(cr, pix, 0,0);

  cairo_paint(cr);
}

static void activate(GtkApplication *app, gpointer user_data){
    GtkWidget *window, *vbox, *darea, *da_container;

    //initializing the window
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Testing Carousel");
    gtk_window_set_default_size(GTK_WINDOW(window), 1080, 720);

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
