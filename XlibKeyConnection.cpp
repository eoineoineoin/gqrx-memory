#include <XlibKeyConnection.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <algorithm>
#include <numeric>
#include <unistd.h>
#include <sys/timeb.h>

namespace
{
// Wrapper around XNextEvent which waits for a maximum amount of time
// instead of blocking. Returns true if eventOut was populated
static bool NextEventWithTimeout(Display *display, XEvent *eventOut, uint64_t timeoutMs)
{
	timeval waitTime = {0};
	waitTime.tv_sec = timeoutMs / 1000;
	waitTime.tv_usec = (timeoutMs % 1000) * 1000;

	// AFAIK, there's no blocking call in xlib; we can get the underlying
	// socket, though, and call select() to see if there's data incoming
	int fd = ConnectionNumber(display);
	fd_set fds;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	int isReady = select(fd + 1, &fds, NULL, NULL, &waitTime);
	if(!isReady)
		return false;

	XNextEvent(display, eventOut);
	return true;
}

// Return the current time, in milliseconds
static uint64_t nowMs()
{
	timeb now;
	ftime(&now);
	return uint64_t(now.time) * 1000 + now.millitm;
}

/* This implements a simple state machine for managing each key
 * and signalling when the user has performed a short/long press.
digraph G
{
	Released -> Pressed [label=KeyPress]
	Pressed -> LOAD [label=KeyRelease]
	LOAD -> Released
	Released [label=KeyRelease]
	Pressed -> SAVE [label=Timeout]
	SAVE -> Triggered
	Triggered -> Released [label=KeyRelease]
	SAVE [shape=box, style=dashed]
	LOAD [shape=box, style=dashed]
}
*/
struct ButtonState
{
	enum {
		RELEASED,
		PRESSED,
		PRESSED_TRIGGERED,
	} m_state = RELEASED;
	uint64_t m_pressStateEntranceMs = 0;

	const uint64_t saveTimeout = 300;

	uint64_t getTimeoutMs(uint64_t now) const
	{
		if(m_state == PRESSED)
			return saveTimeout - (now - m_pressStateEntranceMs);
		return ~(0ull);
	}

	// Enum to indicate what kind of action a user has triggered
	enum UserInput {
		LONG_PRESS,
		SHORT_PRESS,
		NOTHING
	};

	UserInput handleKeyDown(uint64_t now)
	{
		if(m_state == RELEASED) {
			m_state = PRESSED;
			// Record the time to determine when a "jump" action becomes a "save"
			m_pressStateEntranceMs = now;
		}

		return NOTHING;
	}

	UserInput handleKeyUp(uint64_t now)
	{
		if(m_state == PRESSED) {
			m_state = RELEASED;
			return SHORT_PRESS;
		}
		else if(m_state == PRESSED_TRIGGERED) {
			m_state = RELEASED;
		}
		return NOTHING;
	}

	// Check for any "timeout" event if the user pressed and held a key
	// for the duration required to trigger a "save" action
	UserInput checkTimeout(uint64_t now) {
		if(m_state == PRESSED && (now - m_pressStateEntranceMs) > saveTimeout) {
			m_state = PRESSED_TRIGGERED;
			return LONG_PRESS;
		}
		return NOTHING;
	}
};

void fireUserAction(ButtonState::UserInput input, int idx, std::function<void(XlibKeyConnection::Mode, int)>& f)
{
	if(input == ButtonState::LONG_PRESS)
		f(XlibKeyConnection::Mode::SAVE, idx);
	if(input == ButtonState::SHORT_PRESS)
		f(XlibKeyConnection::Mode::LOAD, idx);
}
}

XlibKeyConnection::XlibKeyConnection()
{
	m_memoryCallback = [](Mode, int){}; // Dummy callback, which does nothing
}

XlibKeyConnection::~XlibKeyConnection() = default;

void XlibKeyConnection::run()
{
	Display* display = XOpenDisplay(0);
	Window root = DefaultRootWindow(display);
	XEvent ev;

	KeySym shortcutKeys[] = { XK_F1, XK_F2, XK_F3, XK_F4, XK_F5, XK_F6,
		XK_F7, XK_F8, XK_F9, XK_F10, XK_F11, XK_F12 };
	for(auto keySym : shortcutKeys)
	{
		auto keycode = XKeysymToKeycode(display, keySym);
		XGrabKey(display, keycode, AnyModifier, root, False, GrabModeAsync, GrabModeAsync);
	}

	std::array<ButtonState, sizeof(shortcutKeys) / sizeof(shortcutKeys[0])> states;

	XSelectInput(display, root, KeyPressMask | KeyReleaseMask);
	XFlush(display);

	while(true)
	{
		uint64_t now = nowMs();
		uint64_t minTimeout = std::accumulate(states.begin(), states.end(), ~0ull,
			[now](uint64_t minTime, const ButtonState& b){ return std::min(b.getTimeoutMs(now), minTime); });
		const uint64_t paddingTime = 10; // Extra time to work around event queue latency
		bool hasEvent = NextEventWithTimeout(display, &ev, minTimeout + paddingTime);
		now = nowMs(); //Re-read now, because we potentially waited a long time

		// Detect repeated KeyPress/KeyRelease events from key repeats:
		if(hasEvent && ev.type == KeyRelease && XEventsQueued(display, QueuedAfterReading))
		{
			XEvent nev;
			XPeekEvent(display, &nev);
			if (nev.type == KeyPress && nev.xkey.time == ev.xkey.time && nev.xkey.keycode == ev.xkey.keycode)
			{
				// This is a duplicated event, so we can ignore it:
				hasEvent = false;
				// And remove the next event from the queue too:
				XNextEvent(display, &nev);
			}
		}

		if(hasEvent) {
			KeySym key = XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, 0);
			auto iter = std::find(std::begin(shortcutKeys), std::end(shortcutKeys), key);
			if(iter != std::end(shortcutKeys)) {
				int idx = std::distance(std::begin(shortcutKeys), iter);
				if(ev.type == KeyPress)
					fireUserAction(states[idx].handleKeyDown(now), idx, m_memoryCallback);
				if(ev.type == KeyRelease)
					fireUserAction(states[idx].handleKeyUp(now), idx, m_memoryCallback);
			}
		}

		for(auto iter = states.begin(); iter < states.end(); iter++) {
			ButtonState::UserInput result = iter->checkTimeout(now);
			fireUserAction(result, std::distance(states.begin(), iter), m_memoryCallback);
		}
	}

	for(auto keySym : shortcutKeys)
	{
		auto keycode = XKeysymToKeycode(display, keySym);
		XUngrabKey(display, keycode, AnyModifier, root);
	}

    XCloseDisplay(display);
}


