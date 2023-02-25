#include <memory>
#include <gdkmm/pixbuf.h>
#include <glib.h>
#include <glibmm/main.h>
#include <glibmm/refptr.h>

#define GNOME_BG
#include "background-gl.hpp"

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
        dest->fill(0x00000000);
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

static const char *vertex_shader =
    R"(
attribute vec2 in_position;

//uniform mat4 matrix;
attribute highp vec2 uvpos;
varying vec2 uv;

void main() {
    uv = uvpos;
    gl_Position = vec4(in_position, 0.0, 1.0);
}
)";

static const char *fragment_shader =
    R"(
precision highp float;
uniform sampler2D bg_texture_from;
uniform sampler2D bg_texture_to;
uniform float progress;
uniform vec4 from_adj;
uniform vec4 to_adj;

varying vec2 uv;

void main() {
    vec2 from_uv = vec2((uv.x * from_adj.z) - from_adj.x, (uv.y * from_adj.w) - from_adj.y);
    vec2 to_uv = vec2((uv.x * to_adj.z) - to_adj.x, (uv.y * to_adj.w) - to_adj.y);
    vec4 from = texture2D(bg_texture_from, from_uv);
    vec4 to = texture2D(bg_texture_to, to_uv);
    vec3 color = mix(from.rgb, to.rgb, progress);
    gl_FragColor = vec4(color, 1.0);
}
)";

/* Create and compile a shader */
static GLuint create_shader(int type, const char *src)
{
    GLuint shader;
    int status;

    shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);

    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE)
    {
        int log_len;
        char *buffer;

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);

        buffer = (char*)g_malloc(log_len + 1);
        glGetShaderInfoLog(shader, log_len, NULL, buffer);

        printf("Compile failure in %s shader:\n%s",
            type == GL_VERTEX_SHADER ? "vertex" : "fragment",
            buffer);

        g_free(buffer);

        glDeleteShader(shader);

        return 0;
    }

    return shader;
}

/* Initialize the shaders and link them into a program */
static GLuint init_shaders()
{
    GLuint vertex, fragment;
    GLuint program = 0;
    int status;

    vertex = create_shader(GL_VERTEX_SHADER, vertex_shader);

    if (vertex == 0)
    {
        return 0;
    }

    fragment = create_shader(GL_FRAGMENT_SHADER, fragment_shader);

    if (fragment == 0)
    {
        glDeleteShader(vertex);
        return 0;
    }

    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);

    glLinkProgram(program);

    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE)
    {
        int log_len;
        char *buffer;

        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_len);

        buffer = (char*)g_malloc(log_len + 1);
        glGetProgramInfoLog(program, log_len, NULL, buffer);

        g_warning("Linking failure:\n%s", buffer);

        g_free(buffer);

        glDeleteProgram(program);
        program = 0;

        goto out;
    }

    glDetachShader(program, vertex);
    glDetachShader(program, fragment);

out:
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return program;
}

void BackgroundImage::generate_adjustments(int width, int height)
{
    // Sanity checks
    if ((width == 0) ||
        (height == 0) ||
        (source == nullptr) ||
        (source->get_width() == 0) ||
        (source->get_height() == 0))
    {
        return;
    }

    double screen_width  = (double)width;
    double screen_height = (double)height;
    double source_width  = (double)source->get_width();
    double source_height = (double)source->get_height();

    adjustments = Glib::RefPtr<BackgroundImageAdjustments>(new BackgroundImageAdjustments());
    std::string fill_and_crop_string = "fill_and_crop";
    std::string stretch_string = "stretch";
    if (!stretch_string.compare(fill_type))
    {
        adjustments->x = 0.0;
        adjustments->y = 0.0;
        adjustments->scale_x = 1.0;
        adjustments->scale_y = 1.0;
    } else if (!fill_and_crop_string.compare(fill_type))
    {
        auto width_difference  = screen_width - source_width;
        auto height_difference = screen_height - source_height;
        if (width_difference > height_difference)
        {
            adjustments->scale_x = 1.0;
            adjustments->scale_y = (screen_height / screen_width) * (source_width / source_height);
            adjustments->x = 0.0;
            adjustments->y = (screen_height - source_height * (screen_width / source_width)) *
                adjustments->scale_y * 0.5 * (1.0 / screen_height);
        } else
        {
            adjustments->scale_x = (screen_width / screen_height) * (source_height / source_width);
            adjustments->scale_y = 1.0;
            adjustments->x = (screen_width - source_width * (screen_height / source_height)) *
                adjustments->scale_x * 0.5 * (1.0 / screen_width);
            adjustments->y = 0.0;
        }
    } else /* "preserve_aspect" */
    {
        auto width_difference  = screen_width / source_width;
        auto height_difference = screen_height / source_height;
        if (width_difference > height_difference)
        {
            adjustments->scale_x = (screen_width / screen_height) * (source_height / source_width);
            adjustments->scale_y = 1.0;
            adjustments->x = (screen_width - source_width * (screen_height / source_height)) *
                adjustments->scale_x * 0.5 * (1.0 / screen_width);
            adjustments->y = 0.0;
        } else
        {
            adjustments->scale_x = 1.0;
            adjustments->scale_y = (screen_height / screen_width) * (source_width / source_height);
            adjustments->x = 0.0;
            adjustments->y = (screen_height - source_height * (screen_width / source_width)) *
                adjustments->scale_y * 0.5 * (1.0 / screen_height);
        }
    }
}

bool BackgroundGLArea::show_image(std::string path)
{
    std::shared_ptr<BackgroundImage> image = std::make_shared<BackgroundImage>();
    std::string fill_mode = WfOption<std::string>{"background/fill_mode"};
    image->fill_type = fill_mode;

    try {
#ifndef GNOME_BG
        image->source = Gdk::Pixbuf::create_from_file(path);
#else
        int height = get_height();
        int width  = get_width();

        // ;madhu 260504; #324 got rid of window_height window_width
        // slots of Background. the first time we come here
        // Gtk::GLArea doesn't gives us 0,0, work around it, by
        // sticking in arbitrary values

        if (height == 0)
        {
            height = 480;
        }

        if (width == 0)
        {
            width = 640;
        }

        std::string preserve_aspect_string = "preserve_aspect";
        if (preserve_aspect_string.compare(fill_mode))
        {
            image->source = Gdk::Pixbuf::create_from_file(path);
        } else
        {
            Glib::RefPtr<Gdk::Pixbuf> pbuf;
            if (!WfOption<bool>{"background/span"})
            {
                pbuf = Gdk::Pixbuf::create_from_file(path);
                if (pbuf && WfOption<bool>{"background/always_fit"} &&
                    ((pbuf->get_width() > width) || (pbuf->get_height() > height)))
                {
                    fprintf(stderr, "ignoring background span for image dim (%d, %d) > (%d, %d) \n",
                        pbuf->get_width(), pbuf->get_height(), width, height);
                    goto always_fit;
                }
            } else
            {
always_fit:
                bool background_preserve_aspect = true;
                pbuf = Gdk::Pixbuf::create_from_file(path, width, height,
                    background_preserve_aspect);
            }

            Glib::RefPtr<Gdk::Pixbuf> pbuf2 = Gdk::Pixbuf::create(pbuf->get_colorspace(),
                pbuf->get_has_alpha(),
                pbuf->get_bits_per_sample(),
                width,
                height);
            pixbuf_tile(pbuf, pbuf2, WfOption<bool>{"background/center"}, !WfOption<bool>{"background/tile"});
            image->source = pbuf2;
        }

#endif
    } catch (...)
    {
        return false;
    }

    if (image->source == nullptr)
    {
        return false;
    }

    show_image(image);
    return true;
}

void BackgroundGLArea::show_image(std::shared_ptr<BackgroundImage> next_image)
{
    int window_width = get_width(), window_height = get_height();
    if (!next_image || !next_image->source)
    {
        to_image   = nullptr;
        from_image = nullptr;
        return;
    }

    if (to_image)
    {
        from_image = to_image;
    }

    to_image = next_image;

    int width, height;

    to_image->generate_adjustments(window_width, window_height);
    width  = to_image->source->get_width();
    height = to_image->source->get_height();
    auto format = (to_image->source->get_n_channels() == 3) ? GL_RGB : GL_RGBA;
    glBindTexture(GL_TEXTURE_2D, to_image->tex_id);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE,
        to_image->source->get_pixels());
    glBindTexture(GL_TEXTURE_2D, 0);

    fade = {
        WfOption<int>{"background/fade_duration"},
        wf::animation::smoothing::sigmoid
    };
    fade.animate(0.0, 1.0);

    this->queue_draw();
}

static GLuint create_texture()
{
    GLuint tex;

    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

BackgroundImage::BackgroundImage()
{
    tex_id = create_texture();
}

BackgroundImage::~BackgroundImage()
{
    glDeleteTextures(1, &tex_id);
}

BackgroundGLArea::BackgroundGLArea()
{
    signal_realize().connect(sigc::mem_fun(*this, &BackgroundGLArea::realize));
    signal_render().connect(sigc::mem_fun(*this, &BackgroundGLArea::render), false);
    signal_resize().connect(
        [this] (int width, int height)
    {
        if (to_image)
        {
            int window_width = get_width(), window_height = get_height();
            to_image->generate_adjustments(window_width, window_height);
            queue_draw();
        }
    });
}

void BackgroundGLArea::realize()
{
    this->make_current();
    program = init_shaders();
}

bool BackgroundGLArea::render(const Glib::RefPtr<Gdk::GLContext>& context)
{
    if (!to_image)
    {
        return true;
    }

    static const float vertices[] = {
        1.0f, 1.0f,
        -1.0f, 1.0f,
        -1.0f, -1.0f,
        1.0f, -1.0f,
    };
    static const float uv_coords[] = {
        1.0f, 0.0f,
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f,
    };
    static float from_adj[4] = {0.0, 0.0, 1.0, 1.0};
    if (from_image && from_image->adjustments)
    {
        from_adj[0] = from_image->adjustments->x;
        from_adj[1] = from_image->adjustments->y;
        from_adj[2] = from_image->adjustments->scale_x;
        from_adj[3] = from_image->adjustments->scale_y;
    }

    static float to_adj[4] = {0.0, 0.0, 1.0, 1.0};
    if (to_image && to_image->adjustments)
    {
        to_adj[0] = to_image->adjustments->x;
        to_adj[1] = to_image->adjustments->y;
        to_adj[2] = to_image->adjustments->scale_x;
        to_adj[3] = to_image->adjustments->scale_y;
    }

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(program);
    if (from_image)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, from_image->tex_id);
        GLuint from_tex_uniform = glGetUniformLocation(program, "bg_texture_from");
        glUniform1i(from_tex_uniform, 0);
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, to_image->tex_id);
    GLuint to_tex_uniform = glGetUniformLocation(program, "bg_texture_to");
    glUniform1i(to_tex_uniform, 1);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, uv_coords);
    GLuint progress = glGetUniformLocation(program, "progress");
    glUniform1f(progress, fade);
    GLuint from_adj_loc = glGetUniformLocation(program, "from_adj");
    glUniform4fv(from_adj_loc, 1, from_adj);
    GLuint to_adj_loc = glGetUniformLocation(program, "to_adj");
    glUniform4fv(to_adj_loc, 1, to_adj);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glUseProgram(0);

    if (fade.running())
    {
        Glib::signal_idle().connect([=] ()
        {
            this->queue_draw();
            return false;
        });
    }

    return true;
}
