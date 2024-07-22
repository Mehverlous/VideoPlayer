#include <gtk/gtk.h>
#include <time.h>
#include <unistd.h>

struct Frame{
    GtkWidget *image;
};

static void carousel(GtkWidget *window, GtkWidget *screen){
    GtkWidget *image1 = gtk_picture_new_for_filename("Test_Images/TestImage1.png");
    GtkWidget *image2 = gtk_picture_new_for_filename("Test_Images/TestImage2.png");
    GtkWidget *image3 = gtk_picture_new_for_filename("Test_Images/TestImage3.png");
    GtkWidget *image4 = gtk_picture_new_for_filename("Test_Images/TestImage4.png");
    GtkWidget *image5 = gtk_picture_new_for_filename("Test_Images/TestImage5.png");

    struct Frame frames[5];
    frames[0].image = image1;
    frames[1].image = image2;
    frames[2].image = image3;
    frames[3].image = image4;
    frames[4].image = image5;

    GtkWidget *currentImage = frames[0].image;

    gtk_box_append(GTK_BOX(screen), currentImage);

    gtk_window_present(GTK_WINDOW(window));

    int i = 0;

    // while(1){
    //     if(i == 4){
    //         i = 0;
    //     }

    //     sleep(1);

    //     currentImage = frames[i].image;
    //     printf("Changed to image: %d\n", i+1);

    //     i++;
    // }
}

static void activate(GtkApplication *app, gpointer user_data){
    GtkWidget *window, *screen;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Main Window");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);

    screen = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(screen, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(screen, GTK_ALIGN_CENTER);

    gtk_window_set_child(GTK_WINDOW(window), screen);


    carousel(window, screen);

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