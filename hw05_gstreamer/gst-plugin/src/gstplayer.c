
/**
 * SECTION:element-player
 *
 * FIXME:Describe player here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 player location=./test.wav ! audio/x-raw,format=S16LE,channels=1,rate=48000 ! autoaudiosink
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <gst/base/gstbasesrc.h>
#include "gstplayer.h"
#include <stdio.h>

GST_DEBUG_CATEGORY_STATIC (gst_player_debug);
#define GST_CAT_DEFAULT gst_player_debug

#define WAV_HEADER_SIZE 44
#define MAX_DATA_SIZE (element->audiosize + WAV_HEADER_SIZE + src->blocksize)

typedef struct wav_header_struct {
    char chunk_id[4];
    uint32_t chunk_size;
    char format[4];
    char subchunk1_id[4];
    uint32_t subchunk1_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char subchunk2_id[4];
    uint32_t subchunk2_size;
} header;


/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_LOCATION
};

/* the capabilities of the inputs and outputs.
 */

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_player_parent_class parent_class
G_DEFINE_TYPE (Gstplayer, gst_player, GST_TYPE_BASE_SRC);

GST_ELEMENT_REGISTER_DEFINE (player, "player", GST_RANK_NONE, GST_TYPE_PLAYER);

static void gst_player_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_player_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_player_finalize(GObject* object);

static gboolean gst_player_negotiate(GstBaseSrc* src);
static gboolean gst_player_start(GstBaseSrc* src);
static gboolean gst_player_stop(GstBaseSrc* src);
static GstFlowReturn gst_player_create(GstBaseSrc* src, guint64 offset, guint size, GstBuffer** buf);


/* GObject vmethod implementations */

/* initialize the player's class */
static void
gst_player_class_init (GstplayerClass * klass)
{
  GObjectClass *gobject_class;
  GstBaseSrcClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstBaseSrcClass *) klass;

  gobject_class->set_property = gst_player_set_property;
  gobject_class->get_property = gst_player_get_property;
  gobject_class->finalize = gst_player_finalize;

  gstelement_class->negotiate = gst_player_negotiate;
  gstelement_class->start = gst_player_start;
  gstelement_class->stop = gst_player_stop;
  // gstelement_class->query = gst_player_query;
  gstelement_class->create = gst_player_create;

  g_object_class_install_property (gobject_class, PROP_LOCATION, g_param_spec_string ("location", "location", "File Source Location ?", FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_details_simple (GST_ELEMENT_CLASS(klass),
      "player",
      "player:Generic",
      "player:Generic Template Element", " ak <formarishka@list.ru>");

  gst_element_class_add_pad_template (GST_ELEMENT_CLASS(klass), gst_static_pad_template_get (&src_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad callback functions
 * initialize instance structure
 */
static void
gst_player_init (Gstplayer * element)
{
  element->location = NULL;
  element->current_buffer = NULL;
  element->contents = NULL;
  element->audiosize = 0;
}

static void
gst_player_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec)
{
  Gstplayer *element = GST_PLAYER(object);
  
  switch (prop_id) {
    case PROP_LOCATION:
      element->location = g_strdup(g_value_get_string(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_player_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  Gstplayer *element = GST_PLAYER (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string(value, element->location);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void gst_player_finalize(GObject* object)
{
    G_OBJECT_CLASS(gst_player_parent_class)->finalize(object);
}

/* GstElement vmethod implementations */

static gboolean gst_player_negotiate(GstBaseSrc* src)
{
    return TRUE;
}

static gboolean gst_player_start(GstBaseSrc* src)
{
  Gstplayer *element = GST_PLAYER(src);
  
  if (g_file_get_contents(element->location, &element->contents, NULL, NULL)) {
    header wav_h;
    memcpy(&wav_h, element->contents, WAV_HEADER_SIZE);
    element->audiosize = wav_h.subchunk2_size;
    gst_base_src_set_blocksize (src, (guint)wav_h.byte_rate);
  } 
  else {
    g_print("Error reading file %s\n", element->location);
    return FALSE;
  }
  return TRUE;
}

static gboolean gst_player_stop(GstBaseSrc* src)
{
  Gstplayer *element = GST_PLAYER(src);
  g_free(element->location);
  g_free(element->contents);
  return TRUE;
}

/* create function
 * this function does the actual processing
 */
static GstFlowReturn gst_player_create(GstBaseSrc* src, guint64 offset, guint size, GstBuffer** buf)
{
    Gstplayer *element = GST_PLAYER(src);
    element->current_buffer = gst_buffer_new();
    if (offset >= element->audiosize + WAV_HEADER_SIZE) {
      return GST_FLOW_EOS;
    }
    // printf("offset: %lld \n", offset);
    // printf("size: %d \n", size);
    // GstMemory* memory = gst_memory_new_wrapped(0, element->contents, element->audiosize + WAV_HEADER_SIZE + src->blocksize, WAV_HEADER_SIZE + offset, src->blocksize, NULL, NULL);
    GstMemory* memory = gst_memory_new_wrapped(0, element->contents, MAX_DATA_SIZE, WAV_HEADER_SIZE + offset, MIN(((element->audiosize + WAV_HEADER_SIZE) - offset), src->blocksize), NULL, NULL);
    if(!memory) {
      g_print("memory allocation error\n");
      return GST_FLOW_ERROR;
    }
    gst_buffer_insert_memory(element->current_buffer, -1, memory);
    *buf = element->current_buffer;
    return GST_FLOW_OK;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
player_init (GstPlugin * player)
{
  /* debug category for filtering log messages
   *
   * exchange the string 'Template player' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_player_debug, "player", 0, "Template player");

  return GST_ELEMENT_REGISTER (player, player);
}

/* PACKAGE: this is usually set by meson depending on some _INIT macro
 * in meson.build and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use meson to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "myfirstplayer"
#endif
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "0.0.1"
#endif
#ifndef GST_LICENSE
#define GST_LICENSE "LGPL"
#endif
#ifndef GST_PACKAGE_NAME
#define GST_PACKAGE_NAME "myfirstplayer"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "google.com"
#endif

/* gstreamer looks for this structure to register players
 *
 * exchange the string 'Template player' with your player description
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    player,
    "player",
    player_init,
    PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
