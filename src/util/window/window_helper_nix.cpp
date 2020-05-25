#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <list>
#include <obs-module.h>
#include <string>
#include <util/platform.h>
#include <vector>

/*
    https://github.com/obsproject/obs-studio/blob/master/plugins/linux-capture/xcompcap-helper.cpp
*/
namespace x11util {
static Display* xdisplay = 0;

Display* disp()
{
    if (!xdisplay)
        xdisplay = XOpenDisplay(nullptr);

    return xdisplay;
}

bool ewmhIsSupported()
{
    Display* display = disp();
    Atom netSupportingWmCheck = XInternAtom(display, "_NET_SUPPORTING_WM_CHECK", true);
    Atom actualType;
    int format = 0;
    unsigned long num = 0, bytes = 0;
    unsigned char* data = nullptr;
    Window ewmh_window = 0;

    int status = XGetWindowProperty(display, DefaultRootWindow(display), netSupportingWmCheck, 0L, 1L, false, XA_WINDOW,
        &actualType, &format, &num, &bytes, &data);

    if (status == Success) {
        if (num > 0) {
            ewmh_window = ((Window*)data)[0];
        }
        if (data) {
            XFree(data);
            data = nullptr;
        }
    }

    if (ewmh_window) {
        status = XGetWindowProperty(display, ewmh_window, netSupportingWmCheck, 0L, 1L, false, XA_WINDOW, &actualType,
            &format, &num, &bytes, &data);
        if (status != Success || num == 0 || ewmh_window != ((Window*)data)[0]) {
            ewmh_window = 0;
        }
        if (status == Success && data) {
            XFree(data);
        }
    }

    return ewmh_window != 0;
}

std::list<Window> getTopLevelWindows()
{
    std::list<Window> res;

    if (!ewmhIsSupported()) {
        blog(LOG_WARNING, "Unable to query window list "
                          "because window manager "
                          "does not support extended "
                          "window manager Hints");
        return res;
    }

    Atom netClList = XInternAtom(disp(), "_NET_CLIENT_LIST", true);
    Atom actualType;
    int format;
    unsigned long num, bytes;
    Window* data = 0;

    for (int i = 0; i < ScreenCount(disp()); ++i) {
        Window rootWin = RootWindow(disp(), i);

        int status = XGetWindowProperty(disp(), rootWin, netClList, 0L, ~0L, false, AnyPropertyType, &actualType,
            &format, &num, &bytes, (uint8_t**)&data);

        if (status != Success) {
            blog(LOG_WARNING, "Failed getting root "
                              "window properties");
            continue;
        }

        for (unsigned long i = 0; i < num; ++i)
            res.push_back(data[i]);

        XFree(data);
    }

    return res;
}

std::string getWindowAtom(Window win, const char* atom)
{
    Atom netWmName = XInternAtom(disp(), atom, false);
    int n;
    char** list = 0;
    XTextProperty tp;
    std::string res = "unknown";

    XGetTextProperty(disp(), win, &tp, netWmName);

    if (!tp.nitems)
        XGetWMName(disp(), win, &tp);

    if (!tp.nitems)
        return "error";

    if (tp.encoding == XA_STRING) {
        res = (char*)tp.value;
    } else {
        int ret = XmbTextPropertyToTextList(disp(), &tp, &list, &n);

        if (ret >= Success && n > 0 && *list) {
            res = *list;
            XFreeStringList(list);
        }
    }

    char* conv = nullptr;
    if (os_mbs_to_utf8_ptr(res.c_str(), 0, &conv))
        res = conv;
    bfree(conv);

    XFree(tp.value);

    return res;
}

inline std::string getWindowName(Window win)
{
    return getWindowAtom(win, "_NET_WM_NAME");
}

inline std::string getWindowClass(Window win)
{
    return getWindowAtom(win, "WM_CLASS");
}

} // namespace x11util

void GetWindowList(std::vector<std::string>& windows)
{
    std::list<Window> top_level = x11util::getTopLevelWindows();
    for (const auto& window : top_level) {
        windows.emplace_back(x11util::getWindowName(window));
    }
}
