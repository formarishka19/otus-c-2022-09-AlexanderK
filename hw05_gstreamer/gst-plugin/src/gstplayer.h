#ifndef __GST_PLAYER_H__
#define __GST_PLAYER_H__

#include <gst/gst.h>
#include <stdint.h>

G_BEGIN_DECLS

#define GST_TYPE_PLAYER (gst_player_get_type())
G_DECLARE_FINAL_TYPE (Gstplayer, gst_player, GST, PLAYER, GstBaseSrc)

struct _Gstplayer
{
  GstBaseSrc element;
  uint32_t audiosize;
  gchar* location;
  gchar* contents;
  GstBuffer* current_buffer;
};

G_END_DECLS

#endif /* __GST_PLAYER_H__ */
