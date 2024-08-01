#include <gtk/gtk.h>
#include <time.h>
#include <unistd.h>

struct Frame{
    GtkWidget *image;
};

// static void carousel(GtkWidget *window, GtkWidget *screen){
//     GtkWidget *image1 = gtk_picture_new_for_filename("Test_Images/TestImage1.png");
//     GtkWidget *image2 = gtk_picture_new_for_filename("Test_Images/TestImage2.png");
//     GtkWidget *image3 = gtk_picture_new_for_filename("Test_Images/TestImage3.png");
//     GtkWidget *image4 = gtk_picture_new_for_filename("Test_Images/TestImage4.png");
//     GtkWidget *image5 = gtk_picture_new_for_filename("Test_Images/TestImage5.png");

//     struct Frame frames[5];
//     frames[0].image = image1;
//     frames[1].image = image2;
//     frames[2].image = image3;
//     frames[3].image = image4;
//     frames[4].image = image5;

//     GtkWidget *currentImage = frames[0].image;

//     gtk_box_append(GTK_BOX(screen), currentImage);

//     gtk_window_present(GTK_WINDOW(window));

//     int i = 0;

//     // while(1){
//     //     if(i == 4){
//     //         i = 0;
//     //     }

//     //     sleep(1);

//     //     currentImage = frames[i].image;
//     //     printf("Changed to image: %d\n", i+1);

//     //     i++;
//     // }
// }

static void
draw_function(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data){
  GdkRGBA color;

  cairo_arc(cr, width / 2.0, height / 2.0, MIN (width, height) / 2.0, 0, 2 * G_PI);

  gtk_widget_get_color(GTK_WIDGET(area), &color);
  gdk_cairo_set_source_rgba(cr, &color);

  cairo_fill(cr);
}

static void activate(GtkApplication *app, gpointer user_data){
    GtkWidget *window, *vbox, *darea, *da_container;

    //initializing the window
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Main Window");
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

int main(int argc, char **argv){
    GtkApplication *app;
    int status;

    app = gtk_application_new("main.gtk.video_player", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}