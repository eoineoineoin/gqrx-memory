#include <XlibKeyConnection.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>
#include <algorithm>
#include <numeric>
#include <unistd.h>
#include <sys/timeb.h>

#include <iostream>

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
 * and squashing repeated press/release events from key repeat.
digraph G
{
	Released -> Pressed [label=KeyPress]
	Pressed -> CheckRelease [label=KeyRelease]
	CheckRelease -> Pressed [label=KeyPress]
	CheckRelease -> LOAD [label=Timeout]
	LOAD -> Released
	Pressed -> SAVE [label=Timeout]
	SAVE -> Triggered
	Triggered -> CheckRelease2 [label=KeyRelease]
	CheckRelease2 -> Triggered [label=KeyPress]
	CheckRelease2 -> Released [label=Timeout]
	SAVE [shape=box, style=dashed]
	LOAD [shape=box, style=dashed]
}
*/
struct ButtonState
{
	enum {
		RELEASED,
		PRESSED,
		CHECK_RELEASE,
		PRESSED_TRIGGERED,
		TRIGGERED_CHECK_RELEASE,
	} m_state = RELEASED;
	uint64_t m_stateEntranceMs = 0;

	const uint64_t repeatTimeout = 30;
	const uint64_t saveTimeout = 300;

	uint64_t getTimeoutMs(uint64_t now) const
	{
		if(m_state == CHECK_RELEASE || m_state == TRIGGERED_CHECK_RELEASE)
			return repeatTimeout - (now - m_stateEntranceMs);
		if(m_state == PRESSED)
			return saveTimeout - (now - m_stateEntranceMs);
		return ~(0ull);
	}

	void handleKeyDown(uint64_t now)
	{
		if(m_state == RELEASED) {
			m_state = PRESSED;
			// Only update the time when coming from RELEASED
			// to avoid false negatives key repeats (coming from CHECK_RELEASE)
			m_stateEntranceMs = now;
		}
		else if(m_state == CHECK_RELEASE) {
			m_state = PRESSED;
		}
		else if(m_state == TRIGGERED_CHECK_RELEASE) {
			m_state = PRESSED_TRIGGERED;
		}
	}

	void handleKeyUp(uint64_t now)
	{
		if(m_state == PRESSED) {
			m_state = CHECK_RELEASE;
			m_stateEntranceMs = now;
		}
		else if(m_state == PRESSED_TRIGGERED) {
			m_state = TRIGGERED_CHECK_RELEASE;
			m_stateEntranceMs = now;
		}
	}

	enum UserInput {
		LONG_PRESS,
		SHORT_PRESS,
		NOTHING
	};

	// Since the actions we care about are both triggered due to a "timeout"
	// edge, this returns a value to indicate any action the user has done:
	UserInput checkTimeout(uint64_t now) {
		if(m_state == PRESSED && (now - m_stateEntranceMs) > saveTimeout) {
			m_state = PRESSED_TRIGGERED;
			return LONG_PRESS;
		}
		if(m_state == CHECK_RELEASE && (now - m_stateEntranceMs) > repeatTimeout)
		{
			m_state = RELEASED;
			return SHORT_PRESS;
		}
		if(m_state == TRIGGERED_CHECK_RELEASE && (now - m_stateEntranceMs) > repeatTimeout)
		{
			m_state = RELEASED;
		}
		return NOTHING;
	}
};
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

		if(hasEvent) {
			KeySym key = XkbKeycodeToKeysym(display, ev.xkey.keycode, 0, 0);
			auto iter = std::find(std::begin(shortcutKeys), std::end(shortcutKeys), key);
			if(iter != std::end(shortcutKeys)) {
				int idx = std::distance(std::begin(shortcutKeys), iter);
				if(ev.type == KeyPress)
					states[idx].handleKeyDown(now);
				if(ev.type == KeyRelease)
					states[idx].handleKeyUp(now);
			}
		}

		for(auto iter = states.begin(); iter < states.end(); iter++) {
			ButtonState::UserInput result = iter->checkTimeout(now);
			if(result == ButtonState::LONG_PRESS)
				m_memoryCallback(Mode::SAVE, std::distance(states.begin(), iter));
			if(result == ButtonState::SHORT_PRESS)
				m_memoryCallback(Mode::LOAD, std::distance(states.begin(), iter));
		}
	}

	for(auto keySym : shortcutKeys)
	{
		auto keycode = XKeysymToKeycode(display, keySym);
		XUngrabKey(display, keycode, AnyModifier, root);
	}

    XCloseDisplay(display);
}
