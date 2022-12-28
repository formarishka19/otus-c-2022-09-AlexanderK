#include <gst/gst.h>

static gboolean
bus_call (GstBus     *bus,
      GstMessage *msg,
      gpointer    data)
{
  GMainLoop *loop = data;

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      g_print ("End-of-stream\n");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_ERROR: {
      gchar *debug = NULL;
      GError *err = NULL;

      gst_message_parse_error (msg, &err, &debug);

      g_print ("Error: %s\n", err->message);
      g_error_free (err);

      if (debug) {
        g_print ("Debug details: %s\n", debug);
        g_free (debug);
      }

      g_main_loop_quit (loop);
      break;
    }
    default:
      break;
  }

  return TRUE;
}

gint main (gint   argc, gchar *argv[])
{
    
  GstStateChangeReturn ret;
  GstElement *pipeline;
  GMainLoop *loop;
  GstBus *bus;
  guint watch_id;

  /* initialization */
  gst_init (&argc, &argv);
  loop = g_main_loop_new (NULL, FALSE);
  if (argc != 2) {
    g_print ("Usage: %s <wav filename>\n", argv[0]);
    return -1;
  }

  /* create elements */
  pipeline = gst_pipeline_new("my_pipeline");
  bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));
  watch_id = gst_bus_add_watch (bus, bus_call, loop);
  gst_object_unref (bus);

  GstElement* source  = gst_element_factory_make ("player", "my_player");
  GstElement* sink    = gst_element_factory_make ("autoaudiosink", "my_autoaudiosink");


  if (!sink) {
    g_print ("autoaudiosink not be found - check your install\n");
    return -1;
  } else if (!source) {
    g_print ("Your self-written filesrc could not be found. Make sure it "
             "is installed correctly in $(libdir)/gstreamer-1.0/ or "
             "~/.gstreamer-1.0/plugins/ and that gst-inspect-1.0 lists it. "
             "If it doesn't, check with 'GST_DEBUG=*:2 gst-inspect-1.0' for "
             "the reason why it is not being loaded.");
    return -1;
  }

  g_object_set (G_OBJECT (source), "location", argv[1], NULL);

  gst_bin_add_many (GST_BIN (pipeline), source, sink, NULL);

  /* link everything together */
  if (!gst_element_link_many (source, sink, NULL)) {
    g_print ("Failed to link one or more elements!\n");
    return -1;
  }

  /* run */
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
  if (ret == GST_STATE_CHANGE_FAILURE) {
    GstMessage *msg;

    g_print ("Failed to start up pipeline!\n");

    /* check if there is an error message with details on the bus */
    msg = gst_bus_poll (bus, GST_MESSAGE_ERROR, 0);
    if (msg) {
      GError *err = NULL;

      gst_message_parse_error (msg, &err, NULL);
      g_print ("ERROR: %s\n", err->message);
      g_error_free (err);
      gst_message_unref (msg);
    }
    return -1;
  }

  g_main_loop_run (loop);

  /* clean up */
  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_object_unref (pipeline);
  g_source_remove (watch_id);
  g_main_loop_unref (loop);

  return 0;
}
