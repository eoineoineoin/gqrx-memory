#include <XlibKeyConnection.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <algorithm>
#include <unistd.h>

XlibKeyConnection::XlibKeyConnection()
{
	m_memoryCallback = [](Mode, int){}; // Dummy callback, which does nothing
}

XlibKeyConnection::~XlibKeyConnection()
{
}

void XlibKeyConnection::run()
{
	Display* display = XOpenDisplay(0);
	Window root = DefaultRootWindow(display);

	XEvent      ev;

	KeySym shortcutKeys[] = {XK_F1, XK_F2, XK_F3, XK_F4};
	for(auto keySym : shortcutKeys)
	{
		auto keycode = XKeysymToKeycode(display, keySym);
		XGrabKey(display, keycode, AnyModifier, root, False, GrabModeAsync, GrabModeAsync);
	}

    XSelectInput(display, root, KeyPressMask);
	Time timeOfPress;
    while(true)
    {
        XNextEvent(display, &ev);
        if(ev.type == KeyPress)
		{
			// Remember when the button started being pressed:
			timeOfPress = ev.xkey.time;
		}
		if(ev.type == KeyRelease)
		{
			// Check for a false positive due to key repeats.
			// In the case of a repeat, we'll see multiple release-then-press events
			usleep(1000);
			XEvent checkRepeat;
			Bool repeated = XCheckWindowEvent(display, root, KeyPressMask, &checkRepeat);
			if(repeated)
			{
				continue;
			}

			Time pressDuration = ev.xkey.time - timeOfPress;
			Time saveThreshold = 300;

			Mode operation = pressDuration > saveThreshold ? Mode::SAVE : Mode::LOAD;
			KeySym key = XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, 0);
			auto iter = std::find(std::begin(shortcutKeys), std::end(shortcutKeys), key);
			if(iter != std::end(shortcutKeys))
			{
				m_memoryCallback(operation, std::distance(std::begin(shortcutKeys), iter));
			}
		}
    }
	
	for(auto keySym : shortcutKeys)
	{
		auto keycode = XKeysymToKeycode(display, keySym);
		XUngrabKey(display, keycode, AnyModifier, root);
	}

    XCloseDisplay(display);
}
