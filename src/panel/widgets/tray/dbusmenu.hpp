#ifndef TRAY_DBUSMENU_HPP
#define TRAY_DBUSMENU_HPP

#include <giomm.h>
#include <gtkmm.h>
#include <glibmm.h>
#include <gmodule.h>

#include <wf-option-wrap.hpp>
#ifdef HAVE_DBUS_MENU_GTK
    #include <libdbusmenu-glib/dbusmenu-glib.h>
#endif

#include <optional>

using type_signal_action_group = sigc::signal<void (void)>;

class DbusMenuModel
{
#ifdef HAVE_DBUS_MENU_GTK
    DbusmenuClient *client;
#endif
    std::string prefix;
    type_signal_action_group signal;

    std::string label_to_action_name(std::string, int counter);

  public:
    explicit DbusMenuModel();
    ~DbusMenuModel();
    void connect(const Glib::ustring & service, const Glib::ustring & menu_path,
        const Glib::ustring & prefix);
#ifdef HAVE_DBUS_MENU_GTK
    void layout_updated(DbusmenuMenuitem *item);
    void reconstitute(DbusmenuMenuitem *item);
    int iterate_children(Gio::Menu *parent_menu, DbusmenuMenuitem *parent, int counter);
#endif
    Glib::RefPtr<Gio::SimpleActionGroup> get_action_group();
    Glib::RefPtr<Gio::Menu> get_menu();

    Glib::RefPtr<Gio::SimpleActionGroup> actions;
    Glib::RefPtr<Gio::Menu> menu;

    type_signal_action_group signal_action_group();
};

#endif
