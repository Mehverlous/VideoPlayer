#include <gtk/gtk.h>

static void print_hello(GtkWidget *widget, gpointer data){
  g_print("Hello World Updated\n");
}

static void activate(GtkApplication *app, gpointer user_data){
  GtkWidget *window;
  GtkWidget *image = gtk_image_new_from_file ("Test_Images/TestImage2.jpeg");


  window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "Hello");
  gtk_window_set_default_size(GTK_WINDOW(window), 1080, 720);


  gtk_window_set_child(GTK_WINDOW(window), image);

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