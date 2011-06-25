/* -*- mode: C; c-basic-offset: 2; indent-tabs-mode: nil; -*- */
/*
 * Copyright (C) 2009-2011  Tiger Soldier <tigersoldi@gmail.com>
 *
 * This file is part of OSD Lyrics.
 * 
 * OSD Lyrics is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OSD Lyrics is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OSD Lyrics.  If not, see <http://www.gnu.org/licenses/>. 
 */
#include <string.h>
#include <gtk/gtk.h>
#include "ol_option.h"
#include "ol_about.h"
#include "ol_gui.h"
#include "ol_config.h"
#include "ol_osd_render.h"      /* For getting preview for OSD font and color */
#include "ol_lrc_fetch.h"
#include "ol_path_pattern.h"     /* For getting preview for LRC filename */
#include "ol_intl.h"
#include "ol_debug.h"
#include "ol_cell_renderer_button.h"
#include "ol_lrc_engine_list.h"
#include "ol_player.h"
#include "ol_utils.h"

#define BUFFER_SIZE 1024

typedef struct _OptionWidgets OptionWidgets;

struct CheckButtonOptions
{
  const char *widget_name;
  const char *config_name;
  const char *config_group;
};

struct RadioStringValues
{
  const char *widget_name;
  const char *value;
};

struct RadioStringOptions
{
  const char *config_name;
  const char *config_group;
  struct RadioStringValues *values;
};

struct TogglePropertyOptions
{
  const char *toggle_widget;
  const char *target_widget;
  const char *property;
  gboolean negated;
};

struct WidgetConfigOptions
{
  const char *widget_name;
  const char *config_group;
  const char *config_name;
};

struct ComboStringOptions
{
  const char *widget_name;
  const char *config_group;
  const char *config_name;
  const char **values;
};

enum {
  CHECK_OPT_DOWNLOAD_FIRST = 0,
  CHECK_OPT_TRANSLUCENT_MOUSEOVER,
  CHECK_OPT_NOTIFY_MUSIC,
};

static struct CheckButtonOptions check_button_options[] = {
  {"download-first-lyric", "download-first-lyric", "Download"},
  {"translucent-on-mouse-over", "translucent-on-mouse-over", "OSD"},
  {"notify-music", "notify-music", "General"},
};

static struct RadioStringValues proxy_values[] = {
  {"proxy-no", "no"},
  {"proxy-system", "system"},
  {"proxy-manual", "manual"},
  {NULL, NULL},
};

static struct RadioStringValues osd_window_mode_values[] = {
  {.widget_name = "osd-window-mode-normal", .value = "normal"},
  {.widget_name = "osd-window-mode-dock", .value = "dock"},
  {NULL, NULL},
};

static struct RadioStringOptions radio_str_options[] = {
  {.config_name = "proxy", .config_group = "Download", .values = proxy_values},
  {.config_name = "osd-window-mode", .config_group = "OSD", .values = osd_window_mode_values},
};

static struct WidgetConfigOptions entry_str_options[] = {
  {.widget_name = "proxy-host", .config_group = "Download", .config_name = "proxy-host"},
  {.widget_name = "proxy-username", .config_group = "Download", .config_name = "proxy-username"},
  {.widget_name = "proxy-passwd", .config_group = "Download", .config_name = "proxy-password"},
};

static struct WidgetConfigOptions spin_int_options[] = {
  {.widget_name = "proxy-port", .config_group = "Download", .config_name = "proxy-port"},
  {.widget_name = "outline-width", .config_group = "OSD", .config_name = "outline-width"},
};

static struct WidgetConfigOptions scale_double_options[] = {
  {.widget_name = "scroll-opacity", .config_group = "ScrollMode", .config_name = "opacity"},
};

static struct WidgetConfigOptions color_str_options[] = {
  {.widget_name = "scroll-bg-color", .config_group = "ScrollMode", .config_name = "bg-color"},
  {.widget_name = "scroll-active-lrc-color", .config_group = "ScrollMode", .config_name = "active-lrc-color"},
  {.widget_name = "scroll-inactive-lrc-color", .config_group = "ScrollMode", .config_name = "inactive-lrc-color"},
};

static struct WidgetConfigOptions font_str_options[] = {
  {.widget_name = "scroll-font", .config_group = "ScrollMode", .config_name = "font-name"},
};

static const char *proxy_types[] = {"http", "socks4", "socks5", NULL};
static const char *scroll_modes[] = {"always", "lines", NULL};

static struct ComboStringOptions combo_str_options[] = {
  {.widget_name = "proxy-type", .config_group = "Download", .config_name = "proxy-type", .values = proxy_types},
  {.widget_name = "scroll-scroll-mode", .config_group = "ScrollMode", .config_name = "scroll-mode", .values = scroll_modes},
};

static struct _OptionWidgets
{
  GtkWidget *close;
  GtkWidget *font;
  GtkWidget *outline_width;
  GtkWidget *lrc_align[2];
  GtkWidget *active_lrc_color[OL_LINEAR_COLOR_COUNT];
  GtkWidget *inactive_lrc_color[OL_LINEAR_COLOR_COUNT];
  GtkWidget *osd_preview;
  GtkWidget *line_count[2];
  GtkWidget *download_engine;
  GtkWidget *lrc_path;
  GtkWidget *lrc_path_text;
  GtkWidget *lrc_filename;
  GtkWidget *lrc_filename_text;
  GtkWidget *lrc_filename_sample;
  GtkWidget *path_chooser;
  GtkWidget *filename_menu;
  GtkWidget *startup_player;
  GtkWidget *startup_player_cb;
  GtkWidget *display_mode_osd;
  GtkWidget *display_mode_scroll;
} options;

static struct ListExtraWidgets
{
  GtkWidget *entry;
  GtkWidget *add_button;
  GtkWidget *extra_button;
  GtkWidget *list;
  void (*save_func) ();
} lrc_path_widgets, lrc_filename_widgets;

struct ListExtraButton
{
  char *stock_name;
  void (*click_func) (GtkCellRenderer *cell,
                      gchar *path,
                      GtkTreeView *view);
};

enum TreeColumns {
  TEXT_COLUMN = 0,
  REMOVE_COLUMN,
  N_COLUMN,
};

static struct TogglePropertyOptions toggle_properties[] = {
  {"proxy-manual", "align-proxy-manual", "sensitive", FALSE},
  {"show-proxy-passwd", "proxy-passwd", "visibility", FALSE},
};

static void save_check_button_option (struct CheckButtonOptions* opt);
/* General options */
void ol_option_display_mode_changed (GtkToggleButton *togglebutton,
                                     gpointer user_data);
void ol_option_notify_music_changed (GtkToggleButton *togglebutton,
                                     gpointer user_data);
void ol_option_startup_player_changed (GtkComboBox *cb,
                                       gpointer user_data);
gboolean ol_option_save_startup_player ();
static void init_startup_player (GtkWidget *widget);
/* OSD options */
void ol_option_font_changed (GtkWidget *widget);
void ol_option_update_preview (GtkWidget *widget);
void ol_option_preview_expose (GtkWidget *widget,
                               GdkEventExpose *event,
                               gpointer data);
void ol_option_osd_outline_changed (GtkSpinButton *spinbutton,
                                    gpointer user_data);
void ol_option_osd_line_count_changed (GtkToggleButton *togglebutton,
                                       gpointer user_data);
void ol_option_mouseover_changed (GtkToggleButton *togglebutton,
                                  gpointer user_data);
void ol_option_osd_alignment_changed (GtkRange *range,
                                      gpointer user_data);
void ol_option_osd_color_changed (GtkColorButton *widget,
                                  gpointer user_data);
/* Path options */
void ol_option_save_path_pattern ();
void ol_option_save_file_pattern ();
/* Download options */
static void _connect_download_engine_changed (GtkTreeView *list,
                                              void (*callback) (GtkTreeModel *model));
static void _disconnect_download_engine_changed (GtkTreeView *list,
                                                 void (*callback) (GtkTreeModel *model));
static void ol_option_download_engine_changed (GtkTreeModel *model);
void ol_option_download_first_changed (GtkToggleButton *togglebutton,
                                       gpointer user_data);
void ol_option_about_clicked (GtkWidget *widget, gpointer data);
void ol_option_close_clicked (GtkWidget *widget);
void ol_option_lrc_filename_changed (GtkEditable *editable,
                                     gpointer user_data);
void ol_option_menu_title_activate (GtkMenuItem *menuitem,
                                    gpointer user_data);
void ol_option_menu_artist_activate (GtkMenuItem *menuitem,
                                    gpointer user_data);
void ol_option_menu_album_activate (GtkMenuItem *menuitem,
                                    gpointer user_data);
void ol_option_menu_number_activate (GtkMenuItem *menuitem,
                                    gpointer user_data);
void ol_option_menu_filename_activate (GtkMenuItem *menuitem,
                                    gpointer user_data);
static void ol_option_list_add_clicked (GtkButton *button,
                                        struct ListExtraWidgets *widgets);
void ol_option_filename_clicked (GtkButton *button,
                                 struct ListExtraWidgets *widgets);
void ol_option_path_clicked (GtkButton *button,
                             struct ListExtraWidgets *widgets);
static void ol_option_list_select_changed (GtkTreeSelection *selection,
                                           gpointer data);
static void ol_option_list_entry_changed (GtkEditable *editable,
                                          gpointer user_data);
static OlColor ol_color_from_gdk_color (const GdkColor color);
static GdkColor ol_color_to_gdk_color (const OlColor color);
static void ol_option_update_widget (OptionWidgets *widgets);
static char **get_list_content (GtkTreeView *view);
static void set_list_content (GtkTreeView *view, char **list);
static void list_remove_clicked (GtkCellRenderer *cell,
                                 gchar *path,
                                 GtkTreeView *view);

/** 
 * @brief Get font family and font size from a GtkFontButton
 * 
 * @param font A GtkFontButton
 * @param font_family Ppointer to a string, should point to NULL and
 *                    freed by g_free
 * @param font_size Size of the font
 */
static void ol_option_get_font_info (GtkFontButton *font,
                                     gchar **font_family,
                                     double *font_size);
static void load_osd ();
static void load_download ();
static void load_general ();
static void load_check_button_options ();
static void load_radio_str_options ();
static void load_entry_str_options ();
static void load_spin_int_options ();
static void load_color_str_options ();
static void load_font_str_options ();
static void load_scale_double_options ();
static void load_combo_str_options ();
static void init_list (struct ListExtraWidgets *widgets,
                       struct ListExtraButton *buttons);
static void init_signals ();
static void init_toggle_properties ();
static void radio_str_changed (GtkToggleButton *button,
                               struct RadioStringOptions *option);
static void toggle_set_property (GtkToggleButton *widget,
                                 struct TogglePropertyOptions *option);
static void entry_str_changed (GtkEditable *widget,
                               struct WidgetConfigOptions *option);
static void spin_int_changed (GtkSpinButton *widget,
                              struct WidgetConfigOptions *option);
static void scale_double_changed (GtkScale *widget,
                                  struct WidgetConfigOptions *option);
static void color_str_changed (GtkColorButton *widget,
                               struct WidgetConfigOptions *option);
static void font_str_changed (GtkFontButton *widget,
                              struct WidgetConfigOptions *option);
static void combo_str_changed (GtkComboBox *widget,
                               struct ComboStringOptions *option);

/* General Options */
void
ol_option_display_mode_changed (GtkToggleButton *togglebutton,
                                gpointer user_data)
{
  if (gtk_toggle_button_get_active (togglebutton))
  {
    const char *mode = "OSD";
    if (GTK_WIDGET(togglebutton) == options.display_mode_scroll)
      mode = "scroll";
    ol_config_set_string (ol_config_get_instance (),
                          "General",
                          "display-mode",
                          mode);
  }
}

void
ol_option_notify_music_changed (GtkToggleButton *togglebutton,
                                gpointer user_data)
{
  save_check_button_option (&check_button_options[CHECK_OPT_NOTIFY_MUSIC]);
}

void
ol_option_startup_player_changed (GtkComboBox *cb,
                                  gpointer user_data)
{
  if (options.startup_player == NULL)
    return;
  GtkEntry *entry = GTK_ENTRY (options.startup_player);
  int index = gtk_combo_box_get_active (cb);
  gtk_widget_set_sensitive (options.startup_player, FALSE);
  if (index == 0)
  {
    gtk_entry_set_text (entry, "");
  }
  else
  {
    GtkTreeModel *liststore = gtk_combo_box_get_model (cb);
    GtkTreeIter iter;
    if (gtk_combo_box_get_active_iter (cb, &iter))
    {
      char *command = NULL;
      gtk_tree_model_get (liststore, &iter, 1, &command, -1);
      if (!ol_is_string_empty (command))
      {
        gtk_entry_set_text (entry, command);
      }
      else                        /* Customize */
      {
        gtk_widget_set_sensitive (options.startup_player, TRUE);
        gtk_widget_grab_focus (options.startup_player);
      }
      g_free (command);
    }
  }
  ol_option_save_startup_player ();
}

gboolean
ol_option_save_startup_player ()
{
  if (options.startup_player != NULL)
  {
    ol_config_set_string (ol_config_get_instance (),
                          "General",
                          "startup-player",
                          gtk_entry_get_text (GTK_ENTRY (options.startup_player)));
  }
  return FALSE;
}

/* OSD options */
void
ol_option_font_changed (GtkWidget *widget)
{
  GtkFontButton *font = GTK_FONT_BUTTON (widget);
  if (font != NULL)
  {
    OlConfig *config = ol_config_get_instance ();
    gchar *font_family = NULL;
    double font_size;
    ol_option_get_font_info (font, &font_family, &font_size);
    ol_config_set_string (config,
                          "OSD",
                          "font-family",
                          font_family);
    ol_config_set_double (config,
                          "OSD",
                          "font-size",
                          font_size);
    g_free (font_family);
  }
}

void
ol_option_update_preview (GtkWidget *widget)
{
  gtk_widget_queue_draw (options.osd_preview);
}

void
ol_option_preview_expose (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  ol_assert (options.font != NULL);
  static const char *preview_text = "OSD Lyrics";
  cairo_t *cr = gdk_cairo_create (widget->window);
  gchar *font_family = NULL;
  double font_size;
  ol_option_get_font_info (GTK_FONT_BUTTON (options.font), &font_family, &font_size);
  static OlOsdRenderContext *render = NULL;
  render = ol_osd_render_context_new ();
  ol_osd_render_set_font_family (render, font_family);
  ol_osd_render_set_font_size (render, font_size);
  if (options.outline_width != NULL)
  {
    int outline = gtk_spin_button_get_value (GTK_SPIN_BUTTON (options.outline_width));
    ol_osd_render_set_outline_width (render, outline);
  }
  int tw, th, w, h;
  gdk_drawable_get_size (widget->window, &w, &h);
  ol_osd_render_get_pixel_size (render, preview_text, &tw, &th);
  double x = (w - tw) / 2.0;
  double y = (h - th) / 2.0;
  int i;
  for (i = 0; i < OL_LINEAR_COLOR_COUNT; i++)
  {
    if (options.active_lrc_color[i] != NULL)
    {
      OlColor color;
      GdkColor c;
      gtk_color_button_get_color (GTK_COLOR_BUTTON (options.active_lrc_color[i]),
                                  &c);
      color = ol_color_from_gdk_color (c);
      ol_osd_render_set_linear_color (render, i, color);
    }
  }
  cairo_save (cr);
  cairo_rectangle (cr, 0, 0, w / 2, h);
  cairo_clip (cr);
  ol_osd_render_paint_text (render, cr, preview_text, x, y);
  cairo_restore (cr);
  for (i = 0; i < OL_LINEAR_COLOR_COUNT; i++)
  {
    if (options.inactive_lrc_color[i] != NULL)
    {
      OlColor color;
      GdkColor c;
      gtk_color_button_get_color (GTK_COLOR_BUTTON (options.inactive_lrc_color[i]),
                                  &c);
      color.r = c.red / 65535.0;
      color.g = c.green / 65535.0;
      color.b = c.blue / 65535.0;
      ol_osd_render_set_linear_color (render, i, color);
    }
  }
  cairo_save (cr);
  cairo_rectangle (cr, w / 2, 0, w / 2, h);
  cairo_clip (cr);
  ol_osd_render_paint_text (render, cr, preview_text, x, y);
  cairo_restore (cr);
  ol_osd_render_context_destroy (render);
  cairo_destroy (cr);
  g_free (font_family);
}

void
ol_option_osd_line_count_changed (GtkToggleButton *togglebutton,
                                  gpointer user_data)
{
  int i;
  if (gtk_toggle_button_get_active (togglebutton))
    for (i = 0; i < 2; i++)
      if (GTK_WIDGET (togglebutton) == options.line_count[i])
      {
        ol_config_set_int (ol_config_get_instance (),
                           "OSD",
                           "line-count", i + 1);
        return;
      }
}

void
ol_option_mouseover_changed (GtkToggleButton *togglebutton,
                             gpointer user_data)
{
  save_check_button_option (&check_button_options[CHECK_OPT_TRANSLUCENT_MOUSEOVER]);
}

void
ol_option_osd_alignment_changed (GtkRange *range,
                                 gpointer user_data)
{
  int i;
  for (i = 0; i < 2; i++)
  {
    if (GTK_WIDGET (range) == options.lrc_align[i])
    {
      char buffer[20];
      sprintf (buffer, "lrc-align-%d", i);
      ol_config_set_double (ol_config_get_instance (),
                            "OSD",
                            buffer, 
                            gtk_range_get_value (range));

    }
  }
}

void
ol_option_osd_color_changed (GtkColorButton *widget,
                             gpointer user_data)
{
  int i;
  OlConfig *config = ol_config_get_instance ();
  GtkWidget **color_widgets[] =
    {options.active_lrc_color, options.inactive_lrc_color};
  char *color_props[] =
    {"active-lrc-color", "inactive-lrc-color"};
  int k;
  OlColor colors[OL_LINEAR_COLOR_COUNT];
  for (k = 0; k < 2; k++)
  {
    for (i = 0; i < OL_LINEAR_COLOR_COUNT; i++)
    {
      if (color_widgets[k][i] != NULL)
      {
        GtkColorButton *color_button = GTK_COLOR_BUTTON (color_widgets[k][i]);
        GdkColor color;
        gtk_color_button_get_color (color_button, &color);
        colors[i] = ol_color_from_gdk_color (color);
      }
    }
    char **lrc_color_str = ol_color_to_str_list (colors, OL_LINEAR_COLOR_COUNT);
    ol_config_set_str_list (config,
                            "OSD",
                            color_props[k],
                            (const char**)lrc_color_str,
                            OL_LINEAR_COLOR_COUNT);
    g_strfreev (lrc_color_str);
  }
}

/* Path options */
void
ol_option_save_path_pattern ()
{
  OlConfig *config = ol_config_get_instance ();
  if (options.lrc_path != NULL)
  {
    GtkTreeView *view = GTK_TREE_VIEW (options.lrc_path);
    char **list = get_list_content (view);
    if (list)
    {
      ol_config_set_str_list (config,
                              "General",
                              "lrc-path",
                              (const char **)list,
                              g_strv_length (list));
      g_strfreev (list);
    }
  }
}

void
ol_option_save_file_pattern ()
{
  OlConfig *config = ol_config_get_instance ();
  if (options.lrc_filename != NULL)
  {
    GtkTreeView *view = GTK_TREE_VIEW (options.lrc_filename);
    char **list = get_list_content (view);
    if (list != NULL)
    {
      ol_config_set_str_list (config,
                              "General",
                              "lrc-filename",
                              (const char **)list,
                              g_strv_length (list));
      g_strfreev (list);
    }
  }
}

/* Download options */
void
ol_option_download_engine_changed (GtkTreeModel *model)
{
  OlConfig *config = ol_config_get_instance ();
  char **engine_names = ol_lrc_engine_list_get_engine_names (GTK_TREE_VIEW (options.download_engine));
  if (engine_names != NULL)
  {
    ol_config_set_str_list (config,
                            "Download",
                            "download-engine",
                            (const char**)engine_names,
                            g_strv_length (engine_names));
    g_strfreev (engine_names);
  }
  else
  {
    ol_error ("Failed to get the name of engine");
  }
}

void
ol_option_download_first_changed (GtkToggleButton *togglebutton,
                                  gpointer user_data)
{
  save_check_button_option (&check_button_options[CHECK_OPT_DOWNLOAD_FIRST]);
}

void
ol_option_about_clicked (GtkWidget *widget, gpointer data)
{
  ol_about_show ();
}

static void
ol_option_get_font_info (GtkFontButton *font,
                         gchar **font_family,
                         double *font_size)
{
  const gchar *font_name = gtk_font_button_get_font_name (font);
  PangoFontDescription *font_desc = pango_font_description_from_string (font_name);
  if (font_size != NULL)
  {
    *font_size = pango_font_description_get_size (font_desc);
    if (!pango_font_description_get_size_is_absolute (font_desc))
    {
      *font_size /= PANGO_SCALE;
    }
  }
  if (font_family)
    *font_family = g_strdup (pango_font_description_get_family (font_desc));
  pango_font_description_free (font_desc);
}

static OlColor
ol_color_from_gdk_color (const GdkColor c)
{
  OlColor color;
  color.r = c.red / 65535.0;
  color.g = c.green / 65535.0;
  color.b = c.blue / 65535.0;
  return color;
}

static GdkColor
ol_color_to_gdk_color (const OlColor color)
{
  GdkColor ret;
  ret.red = color.r * 65535;
  ret.green = color.g * 65535;
  ret.blue = color.b * 65535;
  return ret;
}

static void
ol_option_list_add_clicked (GtkButton *button,
                            struct ListExtraWidgets *widgets)
{
  if (button == NULL || widgets == NULL)
    return;
  if (widgets->list == NULL)
    return;
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widgets->list));
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (widgets->list));
  GtkTreeIter iter;
  gtk_list_store_append (GTK_LIST_STORE (model), &iter);
  gtk_tree_selection_select_iter (selection, &iter);
  if (widgets->entry)
    gtk_widget_grab_focus (widgets->entry);
}

void
ol_option_filename_clicked (GtkButton *button,
                            struct ListExtraWidgets *widgets)
{
  ol_log_func ();
  ol_assert (options.filename_menu != NULL);
  gtk_menu_popup (GTK_MENU (options.filename_menu),
                  NULL,         /* parent_menu_shell */
                  NULL,         /* parent_menu_item */
                  NULL,         /* func */
                  NULL,         /* data */
                  1,
                  gtk_get_current_event_time());
}

void
ol_option_path_clicked (GtkButton *button,
                        struct ListExtraWidgets *widgets)
{
  ol_log_func ();
  ol_assert (options.path_chooser != NULL);
  ol_assert (lrc_path_widgets.entry != NULL);
  const char *current_path = gtk_entry_get_text (GTK_ENTRY (lrc_path_widgets.entry));
  if (strcmp (current_path, "%s") != 0)
  {  
    ol_debugf ("  current path:%s\n", current_path);
    char expanded_path[BUFFER_SIZE];
    /* expand `~' to home directory*/
    ol_path_expand_path_pattern (current_path, NULL,
                                 expanded_path, BUFFER_SIZE);
    ol_debugf ("  expanded path:%s\n", expanded_path);
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (options.path_chooser),
                                         expanded_path);
  }
  if (gtk_dialog_run (GTK_DIALOG (options.path_chooser)) == GTK_RESPONSE_ACCEPT)
  {
    char *path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (options.path_chooser));
    ol_debugf ("  path:%s\n", path);
    gtk_entry_set_text (GTK_ENTRY (lrc_path_widgets.entry),
                                   path);
    g_free (path);
  }
  gtk_widget_hide (options.path_chooser);
}

static void
ol_option_list_entry_changed (GtkEditable *editable,
                              gpointer data)
{
  if (editable == NULL || data == NULL)
    return;
  struct ListExtraWidgets *widgets = (struct ListExtraWidgets *) data;
  if (widgets->list == NULL)
    return;
  GtkTreeSelection *selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (widgets->list));
  GtkTreeModel *model = gtk_tree_view_get_model (GTK_TREE_VIEW (widgets->list));
  GtkTreeIter iter;
  gboolean selected = gtk_tree_selection_get_selected (selection, NULL, &iter);
  if (!selected)
    return;
  char *text = gtk_editable_get_chars (editable, 0, -1);
  gtk_list_store_set (GTK_LIST_STORE (model), &iter,
                      TEXT_COLUMN, text,
                      -1);
  g_free (text);
}

void
ol_option_lrc_filename_changed (GtkEditable *editable,
                                gpointer user_data)
{
  static char buffer[BUFFER_SIZE] = "";
  OlMusicInfo info;
  if (options.lrc_filename_sample == NULL)
    return;
  ol_music_info_init (&info);
  info.album = "Album";
  info.title = "Title";
  info.track_number = 1;
  info.artist = "Artist";
  info.uri = "file:///music_path/music_filename.ogg";
  char *pattern = gtk_editable_get_chars (editable, 0, -1);
  if (ol_path_get_lrc_pathname ("", pattern, &info,
                                buffer, BUFFER_SIZE) >= 0)
  {
    gtk_label_set_text (GTK_LABEL (options.lrc_filename_sample), buffer + 1);
  }
  else
  {
    gtk_label_set_text (GTK_LABEL (options.lrc_filename_sample), "");
  }
  g_free (pattern);
}

static void
ol_option_list_select_changed (GtkTreeSelection *selection, gpointer data)
{
  if (selection == NULL || data == NULL)
    return;
  struct ListExtraWidgets *widgets = (struct ListExtraWidgets *) data;
  GtkTreeModel *model;
  GtkTreeIter iter;
  gboolean selected = gtk_tree_selection_get_selected (selection, &model, &iter);
  if (widgets->entry != NULL)
  {
    gtk_widget_set_sensitive (widgets->entry, selected);
    if (selected)
    {
      gchar *text = NULL;
      gtk_tree_model_get (model, &iter,
                          TEXT_COLUMN, &text,
                          -1);
      if (text != NULL)
      {
        gtk_entry_set_text (GTK_ENTRY (widgets->entry), text);
        g_free (text);
      }
      else
      {
        gtk_entry_set_text (GTK_ENTRY (widgets->entry), "");
      }
    }
    else
    {
      gtk_entry_set_text (GTK_ENTRY (widgets->entry), "");
    }
  }
  if (widgets->extra_button != NULL)
  {
    gtk_widget_set_sensitive (widgets->extra_button, selected);
  }
}

static void
ol_option_update_widget (OptionWidgets *widgets)
{
  load_osd ();
  load_download ();
  load_general ();
  load_check_button_options ();
  load_radio_str_options ();
  load_entry_str_options ();
  load_spin_int_options ();
  load_scale_double_options ();
  load_color_str_options ();
  load_font_str_options ();
  load_combo_str_options ();
  init_toggle_properties ();
}

static void
load_osd ()
{
  int i;
  OlConfig *config = ol_config_get_instance ();
  ol_assert (config != NULL);
  /* Updates font */
  GtkFontButton *font = GTK_FONT_BUTTON (options.font);
  if (font != NULL)
  {
    gchar *font_family = ol_config_get_string (config,"OSD", "font-family");
    gchar *font_name = g_strdup_printf ("%s %0.0lf", font_family, ol_config_get_double (config, "OSD", "font-size"));
    gtk_font_button_set_font_name (font, font_name);
    g_free (font_name);
    g_free (font_family);
  }
  /* Lrc align */
  for (i = 0; i < 2; i++)
  {
    GtkRange *lrc_align = GTK_RANGE (options.lrc_align[i]);
    if (lrc_align != NULL)
    {
      char buffer[20];
      sprintf (buffer, "lrc-align-%d", i);
      gtk_range_set_value (lrc_align,
                           ol_config_get_double (config, "OSD", buffer));
    }
  }
  /* [In]Active lrc color */
  GtkWidget **color_widgets[] =
    {options.active_lrc_color, options.inactive_lrc_color};
  char *color_props[] =
    {"active-lrc-color", "inactive-lrc-color"};
  int k;
  for (k = 0; k < 2; k++)
  {
    char ** lrc_color_str = ol_config_get_str_list (config,
                                                    "OSD",
                                                    color_props[k],
                                                    NULL);
    for (i = 0; i < OL_LINEAR_COLOR_COUNT; i++)
    {
      if (color_widgets[k][i] != NULL)
      {
        GtkColorButton *color_button = GTK_COLOR_BUTTON (color_widgets[k][i]);
        GdkColor color = ol_color_to_gdk_color (ol_color_from_string (lrc_color_str[i]));
        gtk_color_button_set_color (color_button, &color);
      }
    }
    g_strfreev (lrc_color_str);
  }
  /* OSD Line count */
  int line_count = ol_config_get_int (config, "OSD", "line-count");
  if (line_count < 1) line_count = 1;
  if (line_count > 2) line_count = 2;
  line_count--;
  if (options.line_count[line_count] != NULL && GTK_IS_TOGGLE_BUTTON (options.line_count[line_count]))
  {
    GtkToggleButton *line_count_widget = GTK_TOGGLE_BUTTON (options.line_count[line_count]);
    gtk_toggle_button_set_active (line_count_widget, TRUE);
  }
}

static void
init_signals ()
{
  int i;
  /* Connect RadioButton signals for string options */
  for (i = 0; i < G_N_ELEMENTS (radio_str_options); i++)
  {
    struct RadioStringValues *value = radio_str_options[i].values;
    if (value == NULL)
      continue;
    for (; value->widget_name != NULL && value->value != NULL; value++)
    {
      GtkToggleButton *radio_button = GTK_TOGGLE_BUTTON (ol_gui_get_widget (value->widget_name)); 
      if (radio_button != NULL)
      {
        g_signal_connect (G_OBJECT (radio_button),
                          "toggled",
                          G_CALLBACK (radio_str_changed),
                          &radio_str_options[i]);
      }
    }
  }

  for (i = 0; i < G_N_ELEMENTS (toggle_properties); i++)
  {
    GtkWidget *widget = ol_gui_get_widget (toggle_properties[i].toggle_widget);
    if (GTK_IS_TOGGLE_BUTTON (widget))
    {
      g_signal_connect (G_OBJECT (widget),
                        "toggled",
                        G_CALLBACK (toggle_set_property),
                        &toggle_properties[i]);
    }
  }
  
  for (i = 0; i < G_N_ELEMENTS (entry_str_options); i++)
  {
    GtkWidget *widget = ol_gui_get_widget (entry_str_options[i].widget_name);
    if (GTK_IS_EDITABLE (widget))
    {
      g_signal_connect (G_OBJECT (widget),
                        "changed",
                        G_CALLBACK (entry_str_changed),
                        &entry_str_options[i]);
    }
  }
  for (i = 0; i < G_N_ELEMENTS (spin_int_options); i++)
  {
    GtkWidget *widget = ol_gui_get_widget (spin_int_options[i].widget_name);
    if (GTK_IS_SPIN_BUTTON (widget))
    {
      g_signal_connect (G_OBJECT (widget),
                        "value-changed",
                        G_CALLBACK (spin_int_changed),
                        &spin_int_options[i]);
    }
  }
  
  for (i = 0; i < G_N_ELEMENTS (scale_double_options); i++)
  {
    GtkWidget *widget = ol_gui_get_widget (scale_double_options[i].widget_name);
    if (GTK_IS_RANGE (widget))
    {
      g_signal_connect (G_OBJECT (widget),
                        "value-changed",
                        G_CALLBACK (scale_double_changed),
                        &scale_double_options[i]);
    }
  }
  
  for (i = 0; i < G_N_ELEMENTS (color_str_options); i++)
  {
    GtkWidget *widget = ol_gui_get_widget (color_str_options[i].widget_name);
    if (GTK_IS_COLOR_BUTTON (widget))
    {
      g_signal_connect (G_OBJECT (widget),
                        "color-set",
                        G_CALLBACK (color_str_changed),
                        &color_str_options[i]);
    }
  }
  
  for (i = 0; i < G_N_ELEMENTS (font_str_options); i++)
  {
    GtkWidget *widget = ol_gui_get_widget (font_str_options[i].widget_name);
    if (GTK_IS_FONT_BUTTON (widget))
    {
      g_signal_connect (G_OBJECT (widget),
                        "font-set",
                        G_CALLBACK (font_str_changed),
                        &font_str_options[i]);
    }
  }
  
  for (i = 0; i < G_N_ELEMENTS (combo_str_options); i++)
  {
    GtkWidget *widget = ol_gui_get_widget (combo_str_options[i].widget_name);
    if (GTK_IS_COMBO_BOX (widget))
    {
      g_signal_connect (G_OBJECT (widget),
                        "changed",
                        G_CALLBACK (combo_str_changed),
                        &combo_str_options[i]);
    }
  }
}

static void
init_toggle_properties ()
{
  int i;
  for (i = 0; i < G_N_ELEMENTS (toggle_properties); i++)
  {
    toggle_set_property (NULL, &toggle_properties[i]);
  }
}

static void
toggle_set_property (GtkToggleButton *widget,
                     struct TogglePropertyOptions *option)
{
  ol_assert (option != NULL);
  if (widget == NULL)
  {
    widget = GTK_TOGGLE_BUTTON (ol_gui_get_widget (option->toggle_widget));
  }
  GtkWidget *target = ol_gui_get_widget (option->target_widget);
  ol_assert (widget != NULL);
  ol_assert (target != NULL);

  GValue value = {0};
  g_value_init (&value, G_TYPE_BOOLEAN);
  gboolean prop = gtk_toggle_button_get_active (widget);
  if (option->negated)
    prop = !prop;
  g_value_set_boolean (&value, prop);
  g_object_set_property (G_OBJECT (target), option->property, &value);
  g_value_unset (&value);
}

static void
radio_str_changed (GtkToggleButton *button,
                   struct RadioStringOptions *option)
{
  ol_assert (GTK_IS_TOGGLE_BUTTON (button));
  ol_assert (option != NULL);
  if (!gtk_toggle_button_get_active (button))
    return;
  OlConfig *config = ol_config_get_instance ();
  const char *config_value = NULL;
  struct RadioStringValues *value = option->values;
  for (; value->widget_name != NULL && value->value != NULL; value++)
  {
    GtkToggleButton *radio_button = GTK_TOGGLE_BUTTON (ol_gui_get_widget (value->widget_name));
    if (radio_button != NULL && gtk_toggle_button_get_active (radio_button))
    {
      config_value = value->value;
      break;
    }
  }
  if (config_value != NULL)
  {
    ol_config_set_string (config,
                          option->config_group,
                          option->config_name,
                          config_value);
  }
}

static void
entry_str_changed (GtkEditable *widget,
                   struct WidgetConfigOptions *option)
{
  ol_assert (GTK_IS_EDITABLE (widget));
  ol_assert (option != NULL);
  OlConfig *config = ol_config_get_instance ();
  char *value = gtk_editable_get_chars (widget, 0, -1);
  ol_config_set_string (config,
                        option->config_group,
                        option->config_name,
                        value);
  g_free (value);
}

static void
spin_int_changed (GtkSpinButton *widget,
                  struct WidgetConfigOptions *option)
{
  ol_assert (GTK_IS_SPIN_BUTTON (widget));
  ol_assert (option != NULL);
  OlConfig *config = ol_config_get_instance ();
  int value = gtk_spin_button_get_value (widget);
  ol_config_set_int (config,
                     option->config_group,
                     option->config_name,
                     value);
}

static void
scale_double_changed (GtkScale *widget,
                      struct WidgetConfigOptions *option)
{
  ol_assert (GTK_IS_RANGE (widget));
  ol_assert (option != NULL);
  OlConfig *config = ol_config_get_instance ();
  double value = gtk_range_get_value (GTK_RANGE (widget));
  ol_config_set_double (config,
                        option->config_group,
                        option->config_name,
                        value);
}

static void
color_str_changed (GtkColorButton *widget,
                   struct WidgetConfigOptions *option)
{
  ol_assert (GTK_IS_COLOR_BUTTON (widget));
  ol_assert (option != NULL);
  OlConfig *config = ol_config_get_instance ();
  GdkColor gcolor;
  gtk_color_button_get_color (GTK_COLOR_BUTTON (widget), &gcolor);
  OlColor color = ol_color_from_gdk_color (gcolor);
  const char *value = ol_color_to_string (color);
  ol_config_set_string (config,
                        option->config_group,
                        option->config_name,
                        value);
}

static void
font_str_changed (GtkFontButton *widget,
                   struct WidgetConfigOptions *option)
{
  ol_assert (GTK_IS_FONT_BUTTON (widget));
  ol_assert (option != NULL);
  OlConfig *config = ol_config_get_instance ();
  const char *value = gtk_font_button_get_font_name (GTK_FONT_BUTTON (widget));
  ol_config_set_string (config,
                        option->config_group,
                        option->config_name,
                        value);
}

static void
combo_str_changed (GtkComboBox *widget,
                   struct ComboStringOptions *option)
{
  ol_assert (GTK_IS_COMBO_BOX (widget));
  ol_assert (option != NULL);
  OlConfig *config = ol_config_get_instance ();
  int index = gtk_combo_box_get_active (widget);
  if (index < g_strv_length ((char**)option->values))
    ol_config_set_string (config,
                          option->config_group,
                          option->config_name,
                          option->values[index]);
  else
    ol_errorf ("Index of combobox %s out of range. Value is %d\n",
               option->widget_name, index);
}

static void
load_check_button_options ()
{
  int i = 0;
  OlConfig *config = ol_config_get_instance ();
  if (config == NULL)
    return;
  for (i = 0; i < G_N_ELEMENTS (check_button_options); i++)
  {
    GtkToggleButton *check_button = GTK_TOGGLE_BUTTON (ol_gui_get_widget (check_button_options[i].widget_name)); 
    if (check_button_options != NULL)
    {
      gtk_toggle_button_set_active (check_button, 
                                    ol_config_get_bool (config,
                                                        check_button_options[i].config_group,
                                                        check_button_options[i].config_name));

    }
  }
}

static void
save_check_button_option (struct CheckButtonOptions* opt)
{
  ol_assert (opt != NULL);
  OlConfig *config = ol_config_get_instance ();
  if (config == NULL)
    return;
  GtkToggleButton *check_button = GTK_TOGGLE_BUTTON (ol_gui_get_widget (opt->widget_name));
  ol_config_set_bool (config,
                      opt->config_group,
                      opt->config_name,
                      gtk_toggle_button_get_active (check_button));
}

static void
load_radio_str_options ()
{
  int i = 0;
  OlConfig *config = ol_config_get_instance ();
  if (config == NULL)
    return;
  for (i = 0; i < G_N_ELEMENTS (radio_str_options); i++)
  {
    char *config_value = ol_config_get_string (config,
                                               radio_str_options[i].config_group,
                                               radio_str_options[i].config_name);
    if (config_value == NULL)
      continue;
    struct RadioStringValues *value = radio_str_options[i].values;
    for (; value->widget_name != NULL && value->value != NULL; value++)
    {
      if (strcmp (value->value, config_value) == 0)
        break;
    }
    if (value->widget_name == NULL || value->value == NULL)
      value = radio_str_options[i].values;
    GtkToggleButton *radio_button = GTK_TOGGLE_BUTTON (ol_gui_get_widget (value->widget_name)); 
    if (radio_button != NULL)
    {
      gtk_toggle_button_set_active (radio_button, TRUE);
    }
    g_free (config_value);
  }
}

static void
load_entry_str_options ()
{
  int i = 0;
  OlConfig *config = ol_config_get_instance ();
  if (config == NULL)
    return;
  for (i = 0; i < G_N_ELEMENTS (entry_str_options); i++)
  {
    char *config_value = ol_config_get_string (config,
                                               entry_str_options[i].config_group,
                                               entry_str_options[i].config_name);
    if (config_value == NULL)
      continue;
    GtkEntry *entry = GTK_ENTRY (ol_gui_get_widget (entry_str_options[i].widget_name)); 
    if (entry != NULL)
    {
      gtk_entry_set_text (entry, config_value);
    }
    g_free (config_value);
  }
}

static void
load_spin_int_options ()
{
  int i = 0;
  OlConfig *config = ol_config_get_instance ();
  if (config == NULL)
    return;
  for (i = 0; i < G_N_ELEMENTS (spin_int_options); i++)
  {
    int config_value = ol_config_get_int (config,
                                          spin_int_options[i].config_group,
                                          spin_int_options[i].config_name);
    GtkSpinButton *spin = GTK_SPIN_BUTTON (ol_gui_get_widget (spin_int_options[i].widget_name)); 
    if (spin != NULL)
    {
      gtk_spin_button_set_value (spin, config_value);
    }
  }
}

static void
load_scale_double_options ()
{
  int i = 0;
  OlConfig *config = ol_config_get_instance ();
  if (config == NULL)
    return;
  for (i = 0; i < G_N_ELEMENTS (scale_double_options); i++)
  {
    double config_value = ol_config_get_double (config,
                                                scale_double_options[i].config_group,
                                                scale_double_options[i].config_name);
    GtkWidget *range = ol_gui_get_widget (scale_double_options[i].widget_name);
    if (GTK_IS_RANGE (range))
    {
      gtk_range_set_value (GTK_RANGE (range), config_value);
    }
  }
}

static void
load_color_str_options ()
{
  int i = 0;
  OlConfig *config = ol_config_get_instance ();
  if (config == NULL)
    return;
  for (i = 0; i < G_N_ELEMENTS (color_str_options); i++)
  {
    char* config_value = ol_config_get_string (config,
                                               color_str_options[i].config_group,
                                               color_str_options[i].config_name);
    if (config_value == NULL)
      continue;
    GtkWidget *button = ol_gui_get_widget (color_str_options[i].widget_name);
    if (GTK_IS_COLOR_BUTTON (button))
    {
      OlColor color = ol_color_from_string (config_value);
      GdkColor gcolor = ol_color_to_gdk_color (color);
      gtk_color_button_set_color (GTK_COLOR_BUTTON (button), &gcolor);
    }
    g_free (config_value);
  }
}

static void
load_font_str_options ()
{
  int i = 0;
  OlConfig *config = ol_config_get_instance ();
  if (config == NULL)
    return;
  for (i = 0; i < G_N_ELEMENTS (font_str_options); i++)
  {
    char* config_value = ol_config_get_string (config,
                                               font_str_options[i].config_group,
                                               font_str_options[i].config_name);
    if (config_value == NULL)
      continue;
    GtkWidget *button = ol_gui_get_widget (font_str_options[i].widget_name);
    if (GTK_IS_FONT_BUTTON (button))
    {
      gtk_font_button_set_font_name (GTK_FONT_BUTTON (button), config_value);
    }
    g_free (config_value);
  }
}

static void
load_combo_str_options ()
{
  int i;
  int index;
  const char **combo_value;
  GtkComboBox *combo;
  OlConfig *config = ol_config_get_instance ();
  for (i = 0; i < G_N_ELEMENTS (combo_str_options); i++)
  {
    char *config_value = ol_config_get_string (config,
                                               combo_str_options[i].config_group,
                                               combo_str_options[i].config_name);
    if (config_value == NULL)
      continue;
    combo = GTK_COMBO_BOX (ol_gui_get_widget (combo_str_options[i].widget_name));
    if (combo == NULL)
      continue;
    combo_value = combo_str_options[i].values;
    for (index = 0; *combo_value != NULL; index++, combo_value++)
    {
      if (strcmp (config_value, *combo_value) == 0)
        break;
    }
    if (combo_value == NULL)
      index = 0;
    gtk_combo_box_set_active (combo, index);
    g_free (config_value);
  }
}

static void
load_download ()
{
  OlConfig *config = ol_config_get_instance ();
  /* Download engine */
  char **download_engines = ol_config_get_str_list (config,
                                                    "Download",
                                                    "download-engine",
                                                    NULL);
  _disconnect_download_engine_changed (GTK_TREE_VIEW (options.download_engine),
                                       ol_option_download_engine_changed);
  ol_lrc_engine_list_set_engine_names (GTK_TREE_VIEW (options.download_engine),
                                       download_engines);
  _connect_download_engine_changed (GTK_TREE_VIEW (options.download_engine),
                                    ol_option_download_engine_changed);
  g_strfreev (download_engines);
}

static char **
get_list_content (GtkTreeView *view)
{
  if (view == NULL)
    return NULL;
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  int cnt = gtk_tree_model_iter_n_children (model, NULL);
  char **list = g_new0 (char *, cnt + 1);
  int i = 0;
  GtkTreeIter iter;
  if (gtk_tree_model_get_iter_first (model, &iter))
  {
    do
    {
      gtk_tree_model_get (model, &iter,
                          TEXT_COLUMN, &list[i],
                          -1);

      i++;
    } while (gtk_tree_model_iter_next (model, &iter));
  }
  return list;
}

static void
set_list_content (GtkTreeView *view, char **list)
{
  GtkTreeModel *model = gtk_tree_view_get_model (view);
  GtkListStore *store = GTK_LIST_STORE (model);
  gtk_list_store_clear (store);
  int i;
  GtkTreeIter iter;
  for (i = 0; list[i] != NULL; i++)
  {
    gtk_list_store_append (store, &iter);
    gtk_list_store_set (store, &iter,
                        TEXT_COLUMN, list[i],
                        REMOVE_COLUMN, GTK_STOCK_REMOVE,
                        -1);
    GtkTreePath *path = gtk_tree_model_get_path (model, &iter);
    
    gtk_tree_path_free (path);
  }
}

static void
load_general ()
{
  OlConfig *config = ol_config_get_instance ();
  if (options.lrc_path != NULL)
  {
    GtkTreeView *view = GTK_TREE_VIEW (options.lrc_path);
    char **list = ol_config_get_str_list (config,
                                          "General",
                                          "lrc-path",
                                          NULL);
    if (list != NULL)
    {
      set_list_content (view, list);
      g_strfreev (list);
    }
  }
  if (options.lrc_filename != NULL)
  {
    GtkTreeView *view = GTK_TREE_VIEW (options.lrc_filename);
    char **list = ol_config_get_str_list (config,
                                          "General",
                                          "lrc-filename",
                                          NULL);
    if (list != NULL)
    {
      set_list_content (view, list);
      g_strfreev (list);
    }
  }
  /* Startup player */
  char *player_cmd = ol_config_get_string (config,
                                           "General",
                                           "startup-player");
   gboolean startup_custom = TRUE;
   if (options.startup_player_cb != NULL)
   {
     GtkComboBox *cb = GTK_COMBO_BOX (options.startup_player_cb);
     if (ol_is_string_empty (player_cmd))
     {
       gtk_combo_box_set_active (cb, 0);
       startup_custom = FALSE;
     }
     else
     {
       int i = 1;
       GtkTreeModel *liststore = gtk_combo_box_get_model (GTK_COMBO_BOX (options.startup_player_cb));
       GtkTreeIter iter;
       gboolean valid;
       for (valid = gtk_tree_model_get_iter_from_string (liststore, &iter, "1");
            valid && startup_custom;
            valid = gtk_tree_model_iter_next (liststore, &iter), i++)
       {
         char *cmd = NULL;
         gtk_tree_model_get (liststore, &iter, 1, &cmd, -1);
         if (cmd != NULL &&
             strcmp (player_cmd, cmd) == 0)
         {
           gtk_combo_box_set_active (cb, i);
           startup_custom = FALSE;
         }
         g_free (cmd);
       }
       if (startup_custom)
       {
         gtk_combo_box_set_active (cb, i - 1);
       }
    }
  }
  if (options.startup_player != NULL)
  {
    gtk_widget_set_sensitive (options.startup_player,
                              startup_custom);
    gtk_entry_set_text (GTK_ENTRY (options.startup_player),
                        player_cmd);
  }
  g_free (player_cmd);

  char *display_mode = ol_config_get_string (config,
                                             "General",
                                             "display-mode");
  if (options.display_mode_scroll != NULL &&
      ol_stricmp (display_mode, "scroll", -1) == 0)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (options.display_mode_scroll),
                                  TRUE);
  else if (options.display_mode_osd != NULL)
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (options.display_mode_osd),
                                  TRUE);
  g_free (display_mode);
}

void
ol_option_close_clicked (GtkWidget *widget)
{
  GtkWidget *toplevel = gtk_widget_get_toplevel (widget);
  if (GTK_WIDGET_TOPLEVEL (toplevel))
  {
    gtk_widget_hide (toplevel);
  }
}

static void
append_button_to_column (GtkTreeViewColumn *column,
                         GtkWidget *list,
                         struct ListExtraButton *button)
{
  if (column == NULL)
    return;
  if (button == NULL || button->click_func == NULL || button->stock_name == NULL)
    return;
  GtkCellRenderer *btncell;
  btncell = ol_cell_renderer_button_new ();
  GValue remove_stock = {0};
  g_value_init (&remove_stock, G_TYPE_STRING);
  g_value_set_static_string (&remove_stock, button->stock_name);
  g_object_set_property (G_OBJECT (btncell),
                         "stock",
                         &remove_stock);
  g_value_unset (&remove_stock);
  g_signal_connect (G_OBJECT (btncell), "clicked",
                    G_CALLBACK (button->click_func),
                    (gpointer) list);
  gtk_tree_view_column_pack_start (column,
                                   btncell,
                                   FALSE);
}

static GtkTreeViewColumn *
get_list_column (GtkWidget *list,
                 struct ListExtraButton *buttons)
{
  GtkTreeViewColumn *column;
  GtkCellRenderer *textcell;
  textcell = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes ("Pattern",
                                                     textcell,
                                                     "text", TEXT_COLUMN,
                                                     NULL);
  GValue text_editable = {0};
  g_value_init (&text_editable, G_TYPE_BOOLEAN);
  g_value_set_boolean (&text_editable, TRUE);
  g_object_set_property (G_OBJECT (textcell),
                         "editable",
                         &text_editable);
  g_value_unset (&text_editable);

  while (buttons != NULL && buttons->stock_name != NULL && buttons->click_func != NULL)
  {
    append_button_to_column (column, list, buttons);
    buttons++;
  }
  struct ListExtraButton remove_button = {
    GTK_STOCK_REMOVE,
    list_remove_clicked,
  };
  append_button_to_column (column, list, &remove_button);

  return column;
}

static void
init_list (struct ListExtraWidgets *widgets,
           struct ListExtraButton *buttons)
{
  GtkTreeView *list = GTK_TREE_VIEW (widgets->list);
  if (list == NULL)
    return;
  GtkListStore *store = gtk_list_store_new (N_COLUMN,
                                            G_TYPE_STRING,
                                            G_TYPE_STRING);
    
  /* gtk_tree_view_column_add_attribute (column, */
  /*                                     btncell, */
  /*                                     "stock", REMOVE_COLUMN); */
  gtk_tree_view_append_column (list, get_list_column (widgets->list, buttons));
  gtk_tree_view_set_model (list, GTK_TREE_MODEL (store));
  GtkTreeSelection *select;
  select = gtk_tree_view_get_selection (list);
  gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
  g_signal_connect (G_OBJECT (select), "changed",
                    G_CALLBACK (ol_option_list_select_changed),
                    (gpointer) widgets);
  ol_option_list_select_changed (select, (gpointer) widgets);
  gtk_tree_view_set_reorderable (list, TRUE);
  /* Setup extra widgets (add, remove, text entry)*/
  if (widgets->save_func != NULL)
  {
    g_signal_connect (G_OBJECT (store), "row-changed",
                      G_CALLBACK (widgets->save_func),
                      (gpointer) widgets);
    g_signal_connect (G_OBJECT (store), "row-deleted",
                      G_CALLBACK (widgets->save_func),
                      (gpointer) widgets);
    g_signal_connect (G_OBJECT (store), "row-inserted",
                      G_CALLBACK (widgets->save_func),
                      (gpointer) widgets);
    g_signal_connect (G_OBJECT (store), "rows-reordered",
                      G_CALLBACK (widgets->save_func),
                      (gpointer) widgets);
  }
  if (widgets->entry != NULL)
  {
    g_signal_connect (G_OBJECT (widgets->entry), "changed",
                      G_CALLBACK (ol_option_list_entry_changed),
                      (gpointer) widgets);
  }
  if (widgets->add_button != NULL)
  {
    g_signal_connect (G_OBJECT (widgets->add_button), "clicked",
                      G_CALLBACK (ol_option_list_add_clicked),
                      (gpointer) widgets);
  }
}

static void
list_remove_clicked (GtkCellRenderer *cell,
                     gchar *path,
                     GtkTreeView *list)
{
  ol_logf (OL_DEBUG,
           "%s\n"
           "  path:%s\n",
           __FUNCTION__,
           path);
  if (list == NULL || path == NULL || !GTK_IS_TREE_VIEW (list))
    return;
  ol_logf (OL_DEBUG, "  test\n");
  GtkTreeModel *model = gtk_tree_view_get_model (list);
  GtkTreeIter iter;
  GtkTreePath *tree_path;
  tree_path = gtk_tree_path_new_from_string (path);
  if (gtk_tree_model_get_iter (model, &iter, tree_path))
  {
    ol_logf (OL_DEBUG, "  path valid\n");
    gtk_list_store_remove (GTK_LIST_STORE (model), &iter);
  }
  gtk_tree_path_free (tree_path);
}

static void
insert_to_filename (const char *text)
{
  ol_log_func ();
  ol_debugf ("  text:%s\n", text);
  ol_assert (lrc_filename_widgets.entry != NULL);
  ol_assert (text != NULL);
  GtkEditable *editable = GTK_EDITABLE (lrc_filename_widgets.entry);
  gtk_editable_delete_selection (editable);
  gint position = gtk_editable_get_position (editable);
  gtk_editable_insert_text (editable,
                            text,
                            strlen (text),
                            &position);
  ol_debugf ("  position after: %d\n", position);
  gtk_widget_grab_focus (lrc_filename_widgets.entry);
  gtk_editable_set_position (editable, position);
}

void
ol_option_menu_title_activate (GtkMenuItem *menuitem,
                               gpointer user_data)
{
  insert_to_filename ("%t");
}

void
ol_option_menu_artist_activate (GtkMenuItem *menuitem,
                                gpointer user_data)
{
  insert_to_filename ("%p");
}

void
ol_option_menu_album_activate (GtkMenuItem *menuitem,
                               gpointer user_data)
{
  insert_to_filename ("%a");
}

void
ol_option_menu_number_activate (GtkMenuItem *menuitem,
                                gpointer user_data)
{
  insert_to_filename ("%n");
}

void
ol_option_menu_filename_activate (GtkMenuItem *menuitem,
                                  gpointer user_data)
{
  insert_to_filename ("%f");
}

static void
_connect_download_engine_changed (GtkTreeView *list,
                                  void (*callback) (GtkTreeModel *model))
{
  ol_assert (GTK_IS_TREE_VIEW (list));
  ol_assert (callback != NULL);
  GtkTreeModel *model = gtk_tree_view_get_model (list);
  g_signal_connect (G_OBJECT (model),
                    "row-changed",
                    (GCallback) callback,
                    NULL);
  g_signal_connect (G_OBJECT (model),
                    "row-deleted",
                    (GCallback) callback,
                    NULL);
  g_signal_connect (G_OBJECT (model),
                    "row-inserted",
                    (GCallback) callback,
                    NULL);
  g_signal_connect (G_OBJECT (model),
                    "rows-reordered",
                    (GCallback) callback,
                    NULL);
}

static void
_disconnect_download_engine_changed (GtkTreeView *list,
                                     void (*callback) (GtkTreeModel *model))
{
  ol_assert (GTK_IS_TREE_VIEW (list));
  ol_assert (callback != NULL);
  GtkTreeModel *model = gtk_tree_view_get_model (list);
  g_signal_handlers_disconnect_by_func (model,
                                        (GCallback) callback,
                                        NULL);
}

static void
init_startup_player (GtkWidget *widget)
{
  GtkComboBox *cb = GTK_COMBO_BOX (widget);
  if (cb == NULL)
    return;
  gtk_combo_box_append_text (cb, _("Choose on startup"));
  GtkListStore *liststore = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (widget)));
  GList *players = g_list_sort (ol_player_get_support_players (),
                                (GCompareFunc) ol_app_info_cmp);
  for (; players != NULL; players = g_list_delete_link (players, players))
  {
    GAppInfo *app_info = players->data;
    GtkTreeIter iter;
    gtk_list_store_append (liststore, &iter);
    gtk_list_store_set (liststore, &iter,
                        0, g_app_info_get_display_name (app_info),
                        1, g_app_info_get_commandline (app_info),
                        -1);
    g_object_unref (G_OBJECT (app_info));
  }
  /* gtk_list_store_append (liststore, &iter); */
  /* gtk_list_store_set (liststore, &iter, */
  /*                     0, "Customize", */
  /*                     1, "", */
  /*                     -1); */
  gtk_combo_box_append_text (cb, _("Customize"));
}

void
ol_option_show ()
{
  static GtkWidget *window = NULL;
  if (window == NULL)
  {
    window = ol_gui_get_widget ("optiondialog");
    ol_assert (window != NULL);
    g_signal_connect (window, "delete-event",
                      G_CALLBACK (gtk_widget_hide_on_delete),
                      NULL);
    options.display_mode_osd = ol_gui_get_widget ("display-mode-osd");
    options.display_mode_scroll = ol_gui_get_widget ("display-mode-scroll");
    options.close = ol_gui_get_widget ("option-close");
    options.font = ol_gui_get_widget ("osd-font");
    options.outline_width = ol_gui_get_widget ("outline-width");
    options.lrc_align[0] = ol_gui_get_widget ("lrc-align-0");
    options.lrc_align[1] = ol_gui_get_widget ("lrc-align-1");
    options.active_lrc_color[0] = ol_gui_get_widget ("active-lrc-color-0");
    options.active_lrc_color[1] = ol_gui_get_widget ("active-lrc-color-1");
    options.active_lrc_color[2] = ol_gui_get_widget ("active-lrc-color-2");
    options.inactive_lrc_color[0] = ol_gui_get_widget ("inactive-lrc-color-0");
    options.inactive_lrc_color[1] = ol_gui_get_widget ("inactive-lrc-color-1");
    options.inactive_lrc_color[2] = ol_gui_get_widget ("inactive-lrc-color-2");
    options.osd_preview = ol_gui_get_widget ("osd-preview");
    options.line_count[0] = ol_gui_get_widget ("line-count-1");
    options.line_count[1] = ol_gui_get_widget ("line-count-2");
    options.download_engine = ol_gui_get_widget ("download-engine");
    options.lrc_path = ol_gui_get_widget ("lrc-path");
    options.lrc_path_text = ol_gui_get_widget ("lrc-path-text");
    options.lrc_filename = ol_gui_get_widget ("lrc-filename");
    options.lrc_filename_text = ol_gui_get_widget ("lrc-filename-text");
    options.lrc_filename_sample = ol_gui_get_widget ("lrc-filename-sample");
    options.startup_player = ol_gui_get_widget ("startup-player");
    options.startup_player_cb = ol_gui_get_widget ("startup-player-cb");
    init_startup_player (options.startup_player_cb);
    /* Init download engine combobox */
    ol_lrc_engine_list_init (GTK_TREE_VIEW (options.download_engine));
    lrc_path_widgets.entry = options.lrc_path_text;
    lrc_path_widgets.list = options.lrc_path;
    lrc_path_widgets.add_button = ol_gui_get_widget ("add-lrc-path");
    lrc_path_widgets.extra_button = ol_gui_get_widget ("lrc-path-browse");
    lrc_path_widgets.save_func = ol_option_save_path_pattern;
    struct ListExtraButton path_buttons[] = {
      /* {GTK_STOCK_DIRECTORY, list_browse_clicked}, */
      {NULL, NULL},
    };
    init_list (&lrc_path_widgets, path_buttons);
    
    lrc_filename_widgets.entry = options.lrc_filename_text;
    lrc_filename_widgets.list = options.lrc_filename;
    lrc_filename_widgets.add_button = ol_gui_get_widget ("add-lrc-filename");
    lrc_filename_widgets.extra_button = ol_gui_get_widget ("lrc-filename-pattern");
    lrc_filename_widgets.save_func = ol_option_save_file_pattern;
    struct ListExtraButton file_buttons[] = {
      /* {GTK_STOCK_INFO, list_pattern_clicked}, */
      {NULL, NULL},
    };
    init_list (&lrc_filename_widgets, file_buttons);

    options.path_chooser = gtk_file_chooser_dialog_new (_("Select a folder"),
                                                        GTK_WINDOW (window),
                                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                                        GTK_STOCK_CANCEL,
                                                        GTK_RESPONSE_CANCEL,
                                                        GTK_STOCK_OPEN,
                                                        GTK_RESPONSE_ACCEPT,
                                                        NULL);
#if GTK_CHECK_VERSION (2, 18, 0)
    gtk_file_chooser_set_create_folders (GTK_FILE_CHOOSER (options.path_chooser),
                                         TRUE);
#endif
    options.filename_menu = ol_gui_get_widget ("filename-pattern-popup");
    /* display the about button at the left edge */
    gtk_button_box_set_child_secondary (GTK_BUTTON_BOX (ol_gui_get_widget ("dialog-action_area2")),
                                        ol_gui_get_widget ("option-aboug"),
                                        TRUE);
    init_signals ();
  }
  ol_option_update_widget (&options);
  gtk_widget_show (GTK_WIDGET (window));
}
