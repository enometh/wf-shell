#include <wordexp.h>
#include <glibmm/main.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/window.h>
#include <gtkmm/image.h>
#include <gdkmm/pixbuf.h>
#include <gdkmm/general.h>
#include <gdk/gdkwayland.h>

#include <random>
#include <algorithm>

#include <iostream>
#include <map>

#include <gtk-utils.hpp>
#include <gtk-layer-shell.h>
#include <glib-unix.h>

#define GNOME_BG
#include "background.hpp"

#ifdef GNOME_BG
// ;madhu 230223 pixbuf_blend and pixbuf_tile from
// gnome-desktop/libgnome-desktop/gnome-bg.c (C) Redhat LGPLv2

static void pixbuf_blend(GdkPixbuf *src,
    GdkPixbuf *dest,
    int src_x,
    int src_y,
    int src_width,
    int src_height,
    int dest_x,
    int dest_y,
    double alpha)
{
    int dest_width  = gdk_pixbuf_get_width(dest);
    int dest_height = gdk_pixbuf_get_height(dest);
    int offset_x    = dest_x - src_x;
    int offset_y    = dest_y - src_y;

    if (src_width < 0)
    {
        src_width = gdk_pixbuf_get_width(src);
    }

    if (src_height < 0)
    {
        src_height = gdk_pixbuf_get_height(src);
    }

    if (dest_x < 0)
    {
        dest_x = 0;
    }

    if (dest_y < 0)
    {
        dest_y = 0;
    }

    if (dest_x + src_width > dest_width)
    {
        src_width = dest_width - dest_x;
    }

    if (dest_y + src_height > dest_height)
    {
        src_height = dest_height - dest_y;
    }

    gdk_pixbuf_composite(src, dest,
        dest_x, dest_y,
        src_width, src_height,
        offset_x, offset_y,
        1, 1, GDK_INTERP_NEAREST,
        alpha * 0xFF + 0.5);
}

static void pixbuf_tile(Glib::RefPtr<Gdk::Pixbuf> src, Glib::RefPtr<Gdk::Pixbuf> dest, bool centered = false,
    bool notiled = false)
{
    int x, y;
    int tile_width, tile_height;
    int dest_width  = gdk_pixbuf_get_width(dest->gobj());
    int dest_height = gdk_pixbuf_get_height(dest->gobj());
    tile_width  = gdk_pixbuf_get_width(src->gobj());
    tile_height = gdk_pixbuf_get_height(src->gobj());

// fprintf(stderr, "pixbuf_tiled: centered: %d notiled=%d, dest_width=%d tile_width=%d\ndest_height=%d
// tile_height=%d\n", centered, notiled, dest_width, tile_width, dest_height, tile_height);

    int src_x = 0, src_y = 0, src_width = tile_width, src_height = tile_height, dst_x = 0, dst_y = 0;
    int nx = 0, ny = 0;

    // dst_x, dst_y if non-zero are top left corner of the first
    // centrally tiled image. nx, ny if non-zero are spillover
    // pixels before the first integral tile.

    if (src_width < dest_width)
    {
        if (src_height < dest_height)
        {
            if (centered)
            {
                dst_x = (dest_width - src_width) * 0.5;
                dst_y = (dest_height - src_height) * 0.5;
                nx    = dst_x % src_width;
                ny    = dst_y % src_height;
            }
        } else
        {
            if (centered)
            {
                dst_x = (dest_width - src_width) * 0.5;
                src_y = (src_height - dest_height) * 0.5;
                nx    = dst_x % src_width;
            }

            src_height = dest_height;
        }
    } else // image width > screen width
    {
        if (src_height < dest_height)
        {
            if (centered)
            {
                dst_y = (dest_height - src_height) * 0.5;
                src_x = (src_width - dest_width) * 0.5;
                ny    = dst_y % src_height;
            }

            src_width = dest_width;
        } else
        {
            if (centered)
            {
                src_y = (src_height - dest_height) * 0.5;
                src_x = (src_width - dest_width) * 0.5;
            }

            src_width  = dest_width;
            src_height = dest_height;
        }
    }

// fprintf(stderr, ":src_x=%d src_y=%d src_width=%d src_height=%d dst_X=%d dst_Y=%d, nx=%d, ny=%d\n", src_x,
// src_y, src_width, src_height, dst_x, dst_y, nx, ny);

    if (notiled)
    {
        pixbuf_blend(src->gobj(), dest->gobj(), src_x, src_y,
            src_width, src_height, dst_x, dst_y, 1.0);
        return;
    }

    if (ny)
    {
        for (x = 0; x < dest_width; x += x == 0 && nx ? nx : src_width)
        {
            pixbuf_blend(src->gobj(), dest->gobj(),
                x == 0 && nx ? src_x + src_width - nx : src_x,
                src_y + src_height - ny,
                x == 0 && nx ? nx : src_width,
                ny,
                x, 0, 1.0);
        }
    }

    if (nx)
    {
        for (y = 0; y < dest_height; y += y == 0 && ny ? ny : src_height)
        {
            pixbuf_blend(src->gobj(), dest->gobj(),
                src_x + src_width - nx,
                y == 0 && ny ? src_y + src_height - ny : src_y,
                nx,
                y == 0 && ny ? ny : src_height,
                0, y, 1.0);
        }
    }

    for (y = ny; y < dest_height; y += src_height)
    {
        for (x = nx; x < dest_width; x += src_width)
        {
            pixbuf_blend(src->gobj(), dest->gobj(), src_x, src_y,
                src_width, src_height, x, y, 1.0);
        }
    }
}

#endif


void BackgroundDrawingArea::show_image(Glib::RefPtr<Gdk::Pixbuf> image,
    double offset_x, double offset_y, double image_scale)
{
    if (!image)
    {
        to_image.source.clear();
        from_image.source.clear();
        return;
    }

    from_image = to_image;
    to_image.source = Gdk::Cairo::create_surface_from_pixbuf(image,
        this->get_scale_factor());

    to_image.x = offset_x / this->get_scale_factor();
    to_image.y = offset_y / this->get_scale_factor();
// #ifndef GNOME_BG
    to_image.scale = image_scale;
// #endif

    fade = {
        fade_duration,
        wf::animation::smoothing::linear
    };

    fade.animate(from_image.source ? 0.0 : 1.0, 1.0);

    Glib::signal_idle().connect_once([=] ()
    {
        this->queue_draw();
    });
}

bool BackgroundDrawingArea::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    if (!to_image.source)
    {
        return false;
    }

    if (fade.running())
    {
        queue_draw();
    } else
    {
        from_image.source.clear();
    }

// #ifndef GNOME_BG
    cr->save();
    cr->scale(to_image.scale, to_image.scale);
// #endif
    cr->set_source(to_image.source, to_image.x, to_image.y);
    cr->paint_with_alpha(fade);
// #ifndef GNOME_BG
    cr->restore();
// #endif
    if (!from_image.source)
    {
        return false;
    }

// #ifndef GNOME_BG
    cr->save();
    cr->scale(from_image.scale, from_image.scale);
// #endif
    cr->set_source(from_image.source, from_image.x, from_image.y);
    cr->paint_with_alpha(1.0 - fade);
// #ifndef GNOME_BG
    cr->restore();
// #endif
    return false;
}

BackgroundDrawingArea::BackgroundDrawingArea()
{
    fade.animate(0, 0);
}

Glib::RefPtr<Gdk::Pixbuf> WayfireBackground::create_from_file_safe(std::string path)
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;
    int width  = window.get_allocated_width() * scale;
    int height = window.get_allocated_height() * scale;

    std::string fill_and_crop_string = "fill_and_crop";
    std::string stretch_string = "stretch";

    if (!stretch_string.compare(background_fill_mode))
    {
        try {
            pbuf =
                Gdk::Pixbuf::create_from_file(path, width, height,
                    false);
        } catch (...)
        {
            return Glib::RefPtr<Gdk::Pixbuf>();
        }

        offset_x    = offset_y = 0.0;
        image_scale = 1.0;
#ifdef GNOME_BG
        goto gb_branch;
#else
        return pbuf;
#endif
    }

    try {
#ifdef GNOME_BG
        if (!background_span)
        {
            pbuf =
                Gdk::Pixbuf::create_from_file(path);
            if (pbuf && background_always_fit &&
                ((pbuf->get_width() > width) || (pbuf->get_height() > height)))
            {
                fprintf(stderr, "ignoring background span for image dim (%d, %d) > (%d, %d) \n",
                    pbuf->get_width(), pbuf->get_height(), width, height);
                pbuf =
                    Gdk::Pixbuf::create_from_file(path, width, height,
                        true);

                goto gb_branch;
            }
        } else
        {
            pbuf =
                Gdk::Pixbuf::create_from_file(path, width, height,
                    true);
            goto gb_branch;
        }

#else
        pbuf =
            Gdk::Pixbuf::create_from_file(path, width, height,
                true);

        return pbuf;
#endif
    } catch (...)
    {
        return Glib::RefPtr<Gdk::Pixbuf>();
    }

    if (!fill_and_crop_string.compare(background_fill_mode)
#ifdef GNOME_BG
        && !background_span
#endif
    )
    {
        float screen_aspect_ratio = (float)width / height;
        float image_aspect_ratio  = (float)pbuf->get_width() / pbuf->get_height();
        bool should_fill_width    = (screen_aspect_ratio > image_aspect_ratio);
        if (should_fill_width)
        {
            image_scale = (double)width / pbuf->get_width();
            offset_y    = ((height / image_scale) - pbuf->get_height()) * 0.5;
            offset_x    = 0;
        } else
        {
            image_scale = (double)height / pbuf->get_height();
            offset_x    = ((width / image_scale) - pbuf->get_width()) * 0.5;
            offset_y    = 0;
        }
    } else
    {
        bool eq_width = (width == pbuf->get_width());
        image_scale = 1.0;
        offset_x    = eq_width ? 0 : (width - pbuf->get_width()) * 0.5;
        offset_y    = eq_width ? (height - pbuf->get_height()) * 0.5 : 0;
    }

#ifdef GNOME_BG
    goto gb_branch;
#else
    return pbuf;
#endif

#ifdef GNOME_BG
gb_branch:
    Glib::RefPtr<Gdk::Pixbuf> pbuf2 =
        Gdk::Pixbuf::create(pbuf->get_colorspace(),
            pbuf->get_has_alpha(),
            pbuf->get_bits_per_sample(),
            width,
            height);
    pixbuf_tile(pbuf, pbuf2, background_center, !background_tile);
    offset_x = offset_y = 0.0;
    return pbuf2;
#endif // GNOME_BG
}

bool WayfireBackground::change_background()
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;
    std::string path;

    if (!load_next_background(pbuf, path))
    {
        set_background();
        return false;
    }

    std::cout << "Loaded " << path << std::endl;
    drawing_area.show_image(pbuf, offset_x, offset_y, image_scale);
    return true;
}

bool WayfireBackground::load_images_from_dir(std::string path)
{
    wordexp_t exp;

    /* Expand path */
    if (wordexp(path.c_str(), &exp, 0))
    {
        return false;
    }

    if (!exp.we_wordc)
    {
        wordfree(&exp);
        return false;
    }

    auto dir = opendir(exp.we_wordv[0]);
    if (!dir)
    {
        wordfree(&exp);
        return false;
    }

    /* Iterate over all files in the directory */
    dirent *file;
    while ((file = readdir(dir)) != 0)
    {
        /* Skip hidden files and folders */
        if (file->d_name[0] == '.')
        {
            continue;
        }

        auto fullpath = std::string(exp.we_wordv[0]) + "/" + file->d_name;

        struct stat next;
        if (stat(fullpath.c_str(), &next) == 0)
        {
            if (S_ISDIR(next.st_mode))
            {
                /* Recursive search */
                load_images_from_dir(fullpath);
            } else
            {
                images.push_back(fullpath);
            }
        }
    }

    wordfree(&exp);

    if (background_randomize && images.size())
    {
        std::random_device random_device;
        std::mt19937 random_gen(random_device());
        std::shuffle(images.begin(), images.end(), random_gen);
    }

    return true;
}

bool WayfireBackground::load_next_background(Glib::RefPtr<Gdk::Pixbuf> & pbuf,
    std::string & path)
{
    while (!pbuf)
    {
        if (!images.size())
        {
            std::cerr << "Failed to load background images from " <<
                    (std::string)background_image << std::endl;
//
// window.remove();
            return false;
        }

        current_background = (current_background + 1) % images.size();

        path = images[current_background];
        pbuf = create_from_file_safe(path);

        if (!pbuf)
        {
            images.erase(images.begin() + current_background);
        }
    }

    return true;
}

void WayfireBackground::reset_background()
{
    images.clear();
    current_background = 0;
    change_bg_conn.disconnect();
    scale = window.get_scale_factor();
}

#define GIOMM_24
static Glib::RefPtr<Gio::Cancellable> cancellable;

static void print_file_monitor_event(const Glib::RefPtr<Gio::File>& file,
    const Glib::RefPtr<Gio::File>& other_file,
#ifdef GIOMM_24
    Gio::FileMonitorEvent
#else
    Gio::FileMonitor::Event
#endif
    event)
{
    const char *s;
    switch (event)
    {
      case G_FILE_MONITOR_EVENT_CHANGED:
        s = "G_FILE_MONITOR_EVENT_CHANGED:\ta file changed.";
        break;

      case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        s =
            "G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:\ta hint that this was probably the last change in a set of changes.";
        break;

      case G_FILE_MONITOR_EVENT_DELETED:
        s = "G_FILE_MONITOR_EVENT_DELETED:\ta file was deleted.";
        break;

      case G_FILE_MONITOR_EVENT_CREATED:
        s = "G_FILE_MONITOR_EVENT_CREATED:\ta file was created.";
        break;

      case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        s = "G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:\ta file attribute was changed.";
        break;

      case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
        s = "G_FILE_MONITOR_EVENT_PRE_UNMOUNT:\tthe file location will soon be unmounted.";
        break;

      case G_FILE_MONITOR_EVENT_UNMOUNTED:
        s = "G_FILE_MONITOR_EVENT_UNMOUNTED:\tthe file location was unmounted.";
        break;

      case G_FILE_MONITOR_EVENT_MOVED:
        s =
            "G_FILE_MONITOR_EVENT_MOVED:\tthe file was moved -- only sent if the (deprecated) G_FILE_MONITOR_SEND_MOVED flag is set";
        break;

      case G_FILE_MONITOR_EVENT_RENAMED:
        s =
            "G_FILE_MONITOR_EVENT_RENAMED:\tthe file was renamed within the current directory -- only sent if the G_FILE_MONITOR_WATCH_MOVES flag is set. Since: 2.46.";
        break;

      case G_FILE_MONITOR_EVENT_MOVED_IN:
        s =
            "G_FILE_MONITOR_EVENT_MOVED_IN:\tthe file was moved into the monitored directory from another location -- only sent if the G_FILE_MONITOR_WATCH_MOVES flag is set. Since: 2.46.";
        break;

      case G_FILE_MONITOR_EVENT_MOVED_OUT:
        s =
            "G_FILE_MONITOR_EVENT_MOVED_OUT:\tthe file was moved out of the monitored directory to another location -- only sent if the G_FILE_MONITOR_WATCH_MOVES flag is set. Since: 2.46.";
        break;
    }

    fprintf(stderr, "%s: file: %s: other_file:%s\n", s,
        file->get_path().c_str() ?: "", other_file ? other_file->get_path().c_str() ?: "" : "");
}

void WayfireBackground::file_monitor(std::string& path)
{
    if (tag)
    {
        tag.disconnect();
    }

    Glib::RefPtr<Gio::File> gf = Gio::File::create_for_path(path.c_str());
    Gio::FileType ft = gf->query_file_type();
    try {
        if (ft ==
#ifdef GIOMM_24
            Gio::FILE_TYPE_DIRECTORY
#else
            Gio::FileType::DIRECTORY
#endif
        )
        {
            fm = gf->monitor_directory(cancellable,
#ifdef GIOMM_24
                Gio::FILE_MONITOR_NONE
#else
                Gio::FileMonitorFlags::NONE
#endif
            );
        } else
        {
            fm = gf->monitor_file(cancellable,
#ifdef GIOMM_24
                Gio::FILE_MONITOR_NONE
#else
                Gio::FileMonitorFlags::NONE
#endif
            );
        }
    } catch (const Gio::Error& error)
    {
        std::cerr << "g_file_monitor_file(" << path <<
            ": failed: " << error.what() << std::endl;
        return;
    }

    tag = fm->signal_changed().connect([this] (const Glib::RefPtr<Gio::File>& file,
                                               const Glib::RefPtr<Gio::File>& other_file,
#ifdef GIOMM_24
                                               Gio::FileMonitorEvent
#else
                                               Gio::FileMonitor::Event
#endif
                                               event)
    {
        print_file_monitor_event(file, other_file, event);
        if (
#ifdef GIOMM_24
            event != Gio::FILE_MONITOR_EVENT_CHANGES_DONE_HINT &&
            event != Gio::FILE_MONITOR_EVENT_DELETED

#else
            event != Gio::FileMonitorEvent::CHANGES_DONE_HINT &&
            event != Gio::FileMonitorEvent::DELETED
#endif
        )
        {
            return;
        }

        set_background();
    });
}

void WayfireBackground::set_background()
{
    Glib::RefPtr<Gdk::Pixbuf> pbuf;

    reset_background();

    std::string path = background_image;
    file_monitor(path);
    try {
        if (load_images_from_dir(path) && images.size())
        {
            if (!load_next_background(pbuf, path))
            {
                throw std::exception();
            }

            std::cout << "Loaded " << path << std::endl;
        } else
        {
            pbuf = create_from_file_safe(path);
            if (!pbuf)
            {
                throw std::exception();
            }
        }
    } catch (...)
    {
        std::cerr << "Failed to load background image(s) " << path << std::endl;
    }

    if (!pbuf)
    {
        pbuf = Gdk::Pixbuf::create(Gdk::Colorspace::COLORSPACE_RGB,
            true,
            8,
            window.get_allocated_width() * scale,
            window.get_allocated_height() * scale);
        pbuf->fill(0x00000000);
    }

    reset_cycle_timeout();
    drawing_area.show_image(pbuf, offset_x, offset_y, image_scale);

    if (inhibited && output->output)
    {
        zwf_output_v2_inhibit_output_done(output->output);
        inhibited = false;
    }
}

void WayfireBackground::reset_cycle_timeout()
{
    int cycle_timeout = background_cycle_timeout * 1000;
    change_bg_conn.disconnect();
    if (images.size())
    {
        change_bg_conn = Glib::signal_timeout().connect(sigc::mem_fun(
            this, &WayfireBackground::change_background), cycle_timeout);
    }
}

void WayfireBackground::setup_window()
{
    window.set_decorated(false);

    gtk_layer_init_for_window(window.gobj());
    gtk_layer_set_layer(window.gobj(), GTK_LAYER_SHELL_LAYER_BACKGROUND);
    gtk_layer_set_monitor(window.gobj(), this->output->monitor->gobj());

    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_BOTTOM, true);
    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_LEFT, true);
    gtk_layer_set_anchor(window.gobj(), GTK_LAYER_SHELL_EDGE_RIGHT, true);
    gtk_layer_set_keyboard_mode(window.gobj(), GTK_LAYER_SHELL_KEYBOARD_MODE_ON_DEMAND);

    gtk_layer_set_exclusive_zone(window.gobj(), -1);
    window.add(drawing_area);
    window.show_all();

    auto reset_background = [=] () { set_background(); };
    auto reset_cycle = [=] () { reset_cycle_timeout(); };
    background_image.set_callback(reset_background);
    background_randomize.set_callback(reset_background);
    background_fill_mode.set_callback(reset_background);
    background_cycle_timeout.set_callback(reset_cycle);

#ifdef GNOME_BG
    background_tile.set_callback(reset_background);
    background_center.set_callback(reset_background);
    background_span.set_callback(reset_background);
    background_always_fit.set_callback(reset_background);
#endif

    window.property_scale_factor().signal_changed().connect(
        sigc::mem_fun(this, &WayfireBackground::set_background));
}

WayfireBackground::WayfireBackground(WayfireShellApp *app, WayfireOutput *output)
{
    this->app    = app;
    this->output = output;

    if (output->output)
    {
        this->inhibited = true;
        zwf_output_v2_inhibit_output(output->output);
    }

    setup_window();

    this->window.signal_size_allocate().connect_notify(
        [this, width = 0, height = 0] (Gtk::Allocation& alloc) mutable
    {
        if ((alloc.get_width() != width) || (alloc.get_height() != height))
        {
            this->set_background();
            width  = alloc.get_width();
            height = alloc.get_height();
        }
    });
}

WayfireBackground::~WayfireBackground()
{
    reset_background();
}

class WayfireBackgroundApp : public WayfireShellApp
{
    std::map<WayfireOutput*, std::unique_ptr<WayfireBackground>> backgrounds;

  public:
    using WayfireShellApp::WayfireShellApp;
    static void create(int argc, char **argv)
    {
        WayfireShellApp::instance =
            std::make_unique<WayfireBackgroundApp>(argc, argv);
        g_unix_signal_add(SIGUSR1, sigusr1_handler, (void*)instance.get());
        instance->run();
    }

    void handle_new_output(WayfireOutput *output) override
    {
        backgrounds[output] = std::unique_ptr<WayfireBackground>(
            new WayfireBackground(this, output));
    }

    void handle_output_removed(WayfireOutput *output) override
    {
        backgrounds.erase(output);
    }

    static gboolean sigusr1_handler(void *instance)
    {
        for (const auto& [_, bg] : ((WayfireBackgroundApp*)instance)->backgrounds)
        {
            bg->change_background();
        }

        return TRUE;
    }
};

int main(int argc, char **argv)
{
    WayfireBackgroundApp::create(argc, argv);
    return 0;
}
