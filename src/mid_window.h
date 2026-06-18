/*
 *
 * Mid Window Header
 *
 */
#ifndef MID_WINDOW_H
#define MID_WINDOW_H

#include <stdint.h>
#include <stdatomic.h>

#include "mid_common.h"

/*
 * Globals
 */
#ifndef DEFAULT_WIDTH
#define DEFAULT_WIDTH 1920
#endif
#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT 1080
#endif
#ifndef DEFAULT_WINDOW_X_POSITION
#define DEFAULT_WINDOW_X_POSITION 0
#endif
#ifndef DEFAULT_WINDOW_Y_POSITION
#define DEFAULT_WINDOW_Y_POSITION 0
#endif

#define MID_KEY_A keyChar['A' - '0']
#define MID_KEY_D keyChar['D' - '0']
#define MID_KEY_F keyChar['F' - '0']
#define MID_KEY_R keyChar['R' - '0']
#define MID_KEY_S keyChar['S' - '0']
#define MID_KEY_W keyChar['W' - '0']

typedef enum MidInputPhase : u8 {
	MID_PHASE_NONE,
	MID_PHASE_PRESS,
	MID_PHASE_HELD,
	MID_PHASE_RELEASE,
	MID_PHASE_CANCEL,
	MID_PHASE_DOUBLE_CLICK,
	MID_PHASE_COUNT,
} MidInputPhase;

typedef enum MidInputLock : u8 {
	MID_INPUT_LOCK_CURSOR_DISABLED,
	MID_INPUT_LOCK_CURSOR_ENABLED,
	MID_INPUT_LOCK_COUNT,
} MidInputLock;

typedef struct MidWindowInput {
	int iMouseX;
	int iMouseY;
	int iMouseDeltaX;
	int iMouseDeltaY;

	float fMouseX;
	float fMouseY;
	float fMouseDeltaX;
	float fMouseDeltaY;

	MidInputPhase leftMouse;
	MidInputPhase rightMouse;
	MidInputPhase middleMouse;

	MidInputPhase keyChar['Z' - '0'];

	double deltaTime;

	MidInputLock cursorLocked;
} MidWindowInput;

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct MidWindow {
	HINSTANCE hInstance;
	HWND      hWnd;
	bool      windowResized;
	int       windowWidth, windowHeight;
	int       clientWidth, clientHeight;
	POINT     localCenter, globalCenter;
	uint64_t  frequency, start, current;
	_Atomic bool running;
} MidWindow;

static inline uint64_t midQueryPerformanceCounter()
{
	LARGE_INTEGER value = {};
	QueryPerformanceCounter(&value);
	value.QuadPart *= 1000000;
	value.QuadPart /= midWindow.frequency;
	return value.QuadPart;
}

#else // Linux / Wayland

#include <wayland-client.h>
#include <libdecor.h>
#include <time.h>

typedef struct MidWindow {
	struct wl_display*    display;
	struct wl_registry*   registry;
	struct wl_compositor* compositor;
	struct wl_surface*    surface;
	struct libdecor*      decorCtx;
	struct libdecor_frame* decorFrame;
	struct wl_seat*       seat;
	struct wl_pointer*    pointer;
	struct wl_keyboard*   keyboard;
	bool     windowResized;
	int      windowWidth, windowHeight;
	int      clientWidth, clientHeight;
	int      pointerCenterX, pointerCenterY;
	struct timespec start;
	_Atomic bool running;
} MidWindow;

#endif // _WIN32 / Linux

extern MidWindow      midWindow;
extern MidWindowInput midWindowInput;
extern double         gpuTimeQueryMs;
extern double         cpuTimeQueryMs;

/* Events */
extern void (*midWindowExitEvent)();

/* Methods */
void midUpdateWindowInput();
void midCreateWindow();
void midWindowLockCursor();
void midWindowReleaseCursor();

#endif // MID_WINDOW_H

/*
 *
 * Mid Window Implementation
 *
 */
#if defined(MID_WINDOW_IMPLEMENTATION) || defined(MID_IDE_ANALYSIS)

#define WINDOW_NAME "moxaic"

MidWindowInput midWindowInput;
MidWindow      midWindow;
double         gpuTimeQueryMs;
double         cpuTimeQueryMs;
void (*midWindowExitEvent)();

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#define NOCOMM
#include <windowsx.h>
#include <stdio.h>

#define CLASS_NAME "MoxaicWindowClass"

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
		case WM_SIZE: if (wParam != SIZE_MINIMIZED) {
				RECT clientRect;
				GetClientRect(hWnd, &clientRect);
				if (midWindow.clientWidth != clientRect.right ||
					midWindow.clientHeight != clientRect.bottom)
					midWindow.windowResized = true;
				midWindow.clientWidth = clientRect.right;
				midWindow.clientHeight = clientRect.bottom;

				RECT windowRect;
				GetWindowRect(hWnd, &windowRect);
				if (midWindow.windowWidth != windowRect.right - windowRect.left ||
					midWindow.windowHeight != windowRect.bottom - windowRect.top)
					midWindow.windowResized = true;
				midWindow.windowWidth = windowRect.right - windowRect.left;
				midWindow.windowHeight = windowRect.bottom - windowRect.top;
			}
			return 0;

		case WM_SETCURSOR:
			switch (LOWORD(lParam)) {
				case HTCLIENT:
					SetCursor(LoadCursor(NULL, IDC_ARROW));
					return TRUE;
			}
			return DefWindowProc(hWnd, uMsg, wParam, lParam);

		case WM_MOUSEMOVE: {
			int newX = GET_X_LPARAM(lParam);
			int newY = GET_Y_LPARAM(lParam);
			switch(midWindowInput.cursorLocked) {
				case MID_INPUT_LOCK_CURSOR_ENABLED:
					midWindowInput.iMouseDeltaX = newX - midWindow.localCenter.x;
					midWindowInput.iMouseDeltaY = newY - midWindow.localCenter.y;
					SetCursorPos(midWindow.globalCenter.x, midWindow.globalCenter.y);
					break;
				case MID_INPUT_LOCK_CURSOR_DISABLED:
					midWindowInput.iMouseDeltaX = newX - midWindowInput.iMouseX;
					midWindowInput.iMouseDeltaY = newY - midWindowInput.iMouseY;
					break;
				default: break;
			}
			midWindowInput.iMouseX = newX;
			midWindowInput.iMouseY = newY;
			midWindowInput.fMouseDeltaX = (float)midWindowInput.iMouseDeltaX;
			midWindowInput.fMouseDeltaY = (float)midWindowInput.iMouseDeltaY;
			midWindowInput.fMouseX = (float)midWindowInput.iMouseX;
			midWindowInput.fMouseY = (float)midWindowInput.iMouseY;
			return 0;
		}
		case WM_KEYDOWN:
			if (wParam >= '0' && wParam <= 'Z')
				midWindowInput.keyChar[wParam - '0'] = MID_PHASE_PRESS;
			return 0;

		case WM_KEYUP:
			if (wParam >= '0' && wParam <= 'Z')
				midWindowInput.keyChar[wParam - '0'] = MID_PHASE_RELEASE;
			return 0;

		case WM_CLOSE:
			atomic_store_explicit(&midWindow.running, false, memory_order_release);
			if (midWindowExitEvent != NULL) midWindowExitEvent();
			return 0;

#define MOUSE_PHASE(macro_prefix, button_prefix)                      \
	case WM_##macro_prefix##BUTTONDOWN:                               \
		midWindowInput.button_prefix##Mouse = MID_PHASE_PRESS;        \
		return 0;                                                     \
	case WM_##macro_prefix##BUTTONUP:                                 \
		midWindowInput.button_prefix##Mouse = MID_PHASE_RELEASE;      \
		return 0;                                                     \
	case WM_##macro_prefix##BUTTONDBLCLK:                             \
		midWindowInput.button_prefix##Mouse = MID_PHASE_DOUBLE_CLICK; \
		return 0;

		MOUSE_PHASE(L, left)
		MOUSE_PHASE(R, right)
		MOUSE_PHASE(M, middle)

#undef MOUSE_PHASE

		default:
			return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

void midUpdateWindowInput()
{
	if (midWindowInput.leftMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.leftMouse = MID_PHASE_NONE;
	if (midWindowInput.rightMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.rightMouse = MID_PHASE_NONE;
	if (midWindowInput.middleMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.middleMouse = MID_PHASE_NONE;

	if (midWindowInput.leftMouse == MID_PHASE_PRESS) midWindowInput.leftMouse = MID_PHASE_HELD;
	if (midWindowInput.rightMouse == MID_PHASE_PRESS) midWindowInput.rightMouse = MID_PHASE_HELD;
	if (midWindowInput.middleMouse == MID_PHASE_PRESS) midWindowInput.middleMouse = MID_PHASE_HELD;

	if (midWindowInput.leftMouse == MID_PHASE_RELEASE) midWindowInput.leftMouse = MID_PHASE_NONE;
	if (midWindowInput.rightMouse == MID_PHASE_RELEASE) midWindowInput.rightMouse = MID_PHASE_NONE;
	if (midWindowInput.middleMouse == MID_PHASE_RELEASE) midWindowInput.middleMouse = MID_PHASE_NONE;

	midWindowInput.fMouseDeltaX = 0;
	midWindowInput.fMouseDeltaY = 0;

	static MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	u64 prior = midWindow.current;
	QueryPerformanceCounter((LARGE_INTEGER*)&midWindow.current);
	u64 delta = ((midWindow.current - prior) * 1000000) / midWindow.frequency;
	midWindowInput.deltaTime = (double)delta * 0.000001f;

#define TITLE_BUFFER_SIZE 64
	static double accumulated = 0.0;
	static int titleUpdateRate = 64;
	accumulated += midWindowInput.deltaTime;
	if (!--titleUpdateRate) {
		titleUpdateRate = 64;
		static char titleBuffer[TITLE_BUFFER_SIZE];
		snprintf(titleBuffer, TITLE_BUFFER_SIZE, "%s | FPS=%.2f | GPU=%.4f | CPU=%.4f", WINDOW_NAME, 64.0 / accumulated, gpuTimeQueryMs, cpuTimeQueryMs);
		accumulated = 0.0;
		SetWindowText(midWindow.hWnd, titleBuffer);
	}
}

void midCreateWindow()
{
	CHECK(midWindow.hInstance != NULL, "Window already created!");
	midWindow.hInstance = GetModuleHandle(NULL);
	atomic_store_explicit(&midWindow.running, true, memory_order_release);

	WNDCLASS wc = {.lpfnWndProc = WindowProc, .hInstance = midWindow.hInstance, .lpszClassName = CLASS_NAME};
	RegisterClass(&wc);

	DWORD windowStyle = WS_OVERLAPPEDWINDOW;
	DWORD windowExStyle = WS_EX_APPWINDOW;
	midWindow.clientWidth = DEFAULT_WIDTH;
	midWindow.clientHeight = DEFAULT_HEIGHT;
	RECT rect = {.right = midWindow.clientWidth, .bottom = midWindow.clientHeight};
	AdjustWindowRectEx(&rect, windowStyle, FALSE, windowExStyle);

	midWindow.windowWidth = rect.right - rect.left;
	midWindow.windowHeight = rect.bottom - rect.top;
	midWindow.hWnd = CreateWindowEx(windowExStyle, CLASS_NAME, WINDOW_NAME, windowStyle,
									DEFAULT_WINDOW_X_POSITION, DEFAULT_WINDOW_Y_POSITION,
									midWindow.windowWidth, midWindow.windowHeight,
									NULL, NULL, midWindow.hInstance, NULL);
	CHECK(midWindow.hWnd == NULL, "Failed to create window.");

	ShowWindow(midWindow.hWnd, SW_SHOW);
	UpdateWindow(midWindow.hWnd);

	QueryPerformanceFrequency((LARGE_INTEGER*)&midWindow.frequency);
	QueryPerformanceCounter((LARGE_INTEGER*)&midWindow.start);
}

void midWindowLockCursor()
{
	ShowCursor(FALSE);
	SetCapture(midWindow.hWnd);
	RECT rect;
	GetClientRect(midWindow.hWnd, &rect);
	midWindow.globalCenter = midWindow.localCenter = (POINT){(rect.right - rect.left) / 2, (rect.bottom - rect.top) / 2};
	ClientToScreen(midWindow.hWnd, (POINT*)&midWindow.globalCenter);
	SetCursorPos(midWindow.globalCenter.x, midWindow.globalCenter.y);
	midWindowInput.cursorLocked = MID_INPUT_LOCK_CURSOR_ENABLED;
}

void midWindowReleaseCursor()
{
	ShowCursor(TRUE);
	ReleaseCapture();
	midWindowInput.cursorLocked = MID_INPUT_LOCK_CURSOR_DISABLED;
}

#else // Linux / Wayland

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <linux/input-event-codes.h>

/* libdecor frame callbacks */
static void frameConfigured(struct libdecor_frame* frame,
                             struct libdecor_configuration* config, void* data)
{
	(void)data;
	int width = 0, height = 0;
	if (!libdecor_configuration_get_content_size(config, frame, &width, &height)) {
		width  = midWindow.clientWidth  ? midWindow.clientWidth  : DEFAULT_WIDTH;
		height = midWindow.clientHeight ? midWindow.clientHeight : DEFAULT_HEIGHT;
	}
	if (width > 0 && height > 0) {
		if (midWindow.clientWidth != width || midWindow.clientHeight != height)
			midWindow.windowResized = true;
		midWindow.clientWidth  = midWindow.windowWidth  = width;
		midWindow.clientHeight = midWindow.windowHeight = height;
		midWindow.pointerCenterX = width  / 2;
		midWindow.pointerCenterY = height / 2;
	}
	struct libdecor_state* state = libdecor_state_new(midWindow.clientWidth, midWindow.clientHeight);
	libdecor_frame_commit(frame, state, config);
	libdecor_state_free(state);
	wl_surface_commit(midWindow.surface);
}
static void frameClose(struct libdecor_frame* frame, void* data)
{
	(void)frame; (void)data;
	atomic_store_explicit(&midWindow.running, false, memory_order_release);
	if (midWindowExitEvent != NULL) midWindowExitEvent();
}
static void frameCommit(struct libdecor_frame* frame, void* data)
{
	(void)frame; (void)data;
	wl_surface_commit(midWindow.surface);
}
static void frameDismissPopup(struct libdecor_frame* frame, const char* seat_name, void* data)
{
	(void)frame; (void)seat_name; (void)data;
}
static struct libdecor_frame_interface frameInterface = {
	.configure     = frameConfigured,
	.close         = frameClose,
	.commit        = frameCommit,
	.dismiss_popup = frameDismissPopup,
};

static void decorError(struct libdecor* ctx, enum libdecor_error error, const char* message)
{
	(void)ctx;
	LOG_ERROR("libdecor error %d: %s\n", error, message);
}
static struct libdecor_interface decorInterface = {
	.error = decorError,
};

/* Pointer events */
static void pointerEnter(void* data, struct wl_pointer* pointer, uint32_t serial,
                          struct wl_surface* surface, wl_fixed_t sx, wl_fixed_t sy)
{
	(void)data; (void)pointer; (void)serial; (void)surface;
	/* Seed position so the first motion event produces a zero delta */
	midWindowInput.fMouseX = (float)wl_fixed_to_double(sx);
	midWindowInput.fMouseY = (float)wl_fixed_to_double(sy);
	midWindowInput.iMouseX = wl_fixed_to_int(sx);
	midWindowInput.iMouseY = wl_fixed_to_int(sy);
}
static void pointerLeave(void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface)
{
	(void)data; (void)pointer; (void)serial; (void)surface;
}
static void pointerMotion(void* data, struct wl_pointer* pointer, uint32_t time,
                           wl_fixed_t sx, wl_fixed_t sy)
{
	(void)data; (void)pointer; (void)time;
	/* Use sub-pixel precision and accumulate across multiple events in one frame */
	float newX = (float)wl_fixed_to_double(sx);
	float newY = (float)wl_fixed_to_double(sy);
	midWindowInput.fMouseDeltaX += newX - midWindowInput.fMouseX;
	midWindowInput.fMouseDeltaY += newY - midWindowInput.fMouseY;
	midWindowInput.iMouseDeltaX = (int)midWindowInput.fMouseDeltaX;
	midWindowInput.iMouseDeltaY = (int)midWindowInput.fMouseDeltaY;
	midWindowInput.fMouseX = newX;
	midWindowInput.fMouseY = newY;
	midWindowInput.iMouseX = (int)newX;
	midWindowInput.iMouseY = (int)newY;
}
static void pointerButton(void* data, struct wl_pointer* pointer, uint32_t serial,
                           uint32_t time, uint32_t button, uint32_t state)
{
	(void)data; (void)pointer; (void)serial; (void)time;
	MidInputPhase phase = (state == WL_POINTER_BUTTON_STATE_PRESSED) ? MID_PHASE_PRESS : MID_PHASE_RELEASE;
	if      (button == BTN_LEFT)   midWindowInput.leftMouse   = phase;
	else if (button == BTN_RIGHT)  midWindowInput.rightMouse  = phase;
	else if (button == BTN_MIDDLE) midWindowInput.middleMouse = phase;
}
static void pointerAxis(void* data, struct wl_pointer* pointer, uint32_t time,
                         uint32_t axis, wl_fixed_t value)
{
	(void)data; (void)pointer; (void)time; (void)axis; (void)value;
}
static void pointerFrame(void* data, struct wl_pointer* pointer)
{
	(void)data; (void)pointer;
}
static void pointerAxisSource(void* data, struct wl_pointer* pointer, uint32_t axis_source)
{
	(void)data; (void)pointer; (void)axis_source;
}
static void pointerAxisStop(void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis)
{
	(void)data; (void)pointer; (void)time; (void)axis;
}
static void pointerAxisDiscrete(void* data, struct wl_pointer* pointer, uint32_t axis, int32_t discrete)
{
	(void)data; (void)pointer; (void)axis; (void)discrete;
}
static const struct wl_pointer_listener pointerListener = {
	.enter         = pointerEnter,
	.leave         = pointerLeave,
	.motion        = pointerMotion,
	.button        = pointerButton,
	.axis          = pointerAxis,
	.frame         = pointerFrame,
	.axis_source   = pointerAxisSource,
	.axis_stop     = pointerAxisStop,
	.axis_discrete = pointerAxisDiscrete,
};

/* Keyboard events */
static void keyboardKeymap(void* data, struct wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size)
{
	(void)data; (void)keyboard; (void)format; (void)fd; (void)size;
	close(fd);
}
static void keyboardEnter(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                           struct wl_surface* surface, struct wl_array* keys)
{
	(void)data; (void)keyboard; (void)serial; (void)surface; (void)keys;
}
static void keyboardLeave(void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface)
{
	(void)data; (void)keyboard; (void)serial; (void)surface;
}
static void keyboardKey(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                         uint32_t time, uint32_t key, uint32_t state)
{
	(void)data; (void)keyboard; (void)serial; (void)time;
	/* Map Linux evdev keycodes to ASCII chars for MID_KEY_* macros.
	   KEY_A=30 maps to 'A'=65. Offset = KEY_A - 'A' = 30-65 = -35 in reverse.
	   Linux keycodes: A=30,B=48,C=46,D=32,E=18,F=33,R=19,S=31,W=17 */
	static const uint8_t keyToAscii[] = {
		[KEY_A]=65, [KEY_B]=66, [KEY_C]=67, [KEY_D]=68, [KEY_E]=69,
		[KEY_F]=70, [KEY_G]=71, [KEY_H]=72, [KEY_I]=73, [KEY_J]=74,
		[KEY_K]=75, [KEY_L]=76, [KEY_M]=77, [KEY_N]=78, [KEY_O]=79,
		[KEY_P]=80, [KEY_Q]=81, [KEY_R]=82, [KEY_S]=83, [KEY_T]=84,
		[KEY_U]=85, [KEY_V]=86, [KEY_W]=87, [KEY_X]=88, [KEY_Y]=89,
		[KEY_Z]=90,
	};
	if (key < sizeof(keyToAscii) && keyToAscii[key] != 0) {
		uint8_t ch = keyToAscii[key];
		if (ch >= '0' && ch <= 'Z') {
			midWindowInput.keyChar[ch - '0'] = (state == WL_KEYBOARD_KEY_STATE_PRESSED)
				? MID_PHASE_PRESS : MID_PHASE_RELEASE;
		}
	}
	if (key == KEY_ESC && state == WL_KEYBOARD_KEY_STATE_PRESSED) {
		atomic_store_explicit(&midWindow.running, false, memory_order_release);
		if (midWindowExitEvent != NULL) midWindowExitEvent();
	}
}
static void keyboardModifiers(void* data, struct wl_keyboard* keyboard, uint32_t serial,
                               uint32_t modsDepressed, uint32_t modsLatched,
                               uint32_t modsLocked, uint32_t group)
{
	(void)data; (void)keyboard; (void)serial;
	(void)modsDepressed; (void)modsLatched; (void)modsLocked; (void)group;
}
static void keyboardRepeatInfo(void* data, struct wl_keyboard* keyboard,
                                int32_t rate, int32_t delay)
{
	(void)data; (void)keyboard; (void)rate; (void)delay;
}
static const struct wl_keyboard_listener keyboardListener = {
	.keymap      = keyboardKeymap,
	.enter       = keyboardEnter,
	.leave       = keyboardLeave,
	.key         = keyboardKey,
	.modifiers   = keyboardModifiers,
	.repeat_info = keyboardRepeatInfo,
};

/* Seat capabilities */
static void seatCapabilities(void* data, struct wl_seat* seat, uint32_t capabilities)
{
	(void)data;
	if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && midWindow.pointer == NULL) {
		midWindow.pointer = wl_seat_get_pointer(seat);
		wl_pointer_add_listener(midWindow.pointer, &pointerListener, NULL);
	}
	if ((capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && midWindow.keyboard == NULL) {
		midWindow.keyboard = wl_seat_get_keyboard(seat);
		wl_keyboard_add_listener(midWindow.keyboard, &keyboardListener, NULL);
	}
}
static void seatName(void* data, struct wl_seat* seat, const char* name)
{
	(void)data; (void)seat; (void)name;
}
static const struct wl_seat_listener seatListener = {
	.capabilities = seatCapabilities,
	.name         = seatName,
};

/* Registry global binding */
static void registryGlobal(void* data, struct wl_registry* registry, uint32_t name,
                             const char* interface, uint32_t version)
{
	(void)data; (void)version;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		midWindow.compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, wl_seat_interface.name) == 0) {
		midWindow.seat = wl_registry_bind(registry, name, &wl_seat_interface, 5);
		wl_seat_add_listener(midWindow.seat, &seatListener, NULL);
	}
}
static void registryGlobalRemove(void* data, struct wl_registry* registry, uint32_t name)
{
	(void)data; (void)registry; (void)name;
}
static const struct wl_registry_listener registryListener = {
	.global        = registryGlobal,
	.global_remove = registryGlobalRemove,
};

void midCreateWindow()
{
	REQUIRE(midWindow.display == NULL, "Window already created!");

	midWindow.display = wl_display_connect(NULL);
	REQUIRE(midWindow.display != NULL, "Failed to connect to Wayland display");

	midWindow.registry = wl_display_get_registry(midWindow.display);
	wl_registry_add_listener(midWindow.registry, &registryListener, NULL);

	wl_display_roundtrip(midWindow.display);
	REQUIRE(midWindow.compositor != NULL, "No wl_compositor found");

	midWindow.surface = wl_compositor_create_surface(midWindow.compositor);

	midWindow.decorCtx = libdecor_new(midWindow.display, &decorInterface);
	REQUIRE(midWindow.decorCtx != NULL, "Failed to create libdecor context");

	midWindow.clientWidth  = midWindow.windowWidth  = DEFAULT_WIDTH;
	midWindow.clientHeight = midWindow.windowHeight = DEFAULT_HEIGHT;
	midWindow.pointerCenterX = DEFAULT_WIDTH  / 2;
	midWindow.pointerCenterY = DEFAULT_HEIGHT / 2;

	midWindow.decorFrame = libdecor_decorate(midWindow.decorCtx, midWindow.surface,
	                                          &frameInterface, NULL);
	REQUIRE(midWindow.decorFrame != NULL, "Failed to create libdecor frame");

	libdecor_frame_set_title(midWindow.decorFrame, WINDOW_NAME);
	libdecor_frame_set_app_id(midWindow.decorFrame, "moxaic");
	libdecor_frame_map(midWindow.decorFrame);

	/* Dispatch until we receive the initial configure (gets us valid content size) */
	while (!libdecor_frame_is_floating(midWindow.decorFrame) &&
	       midWindow.clientWidth == DEFAULT_WIDTH) {
		if (libdecor_dispatch(midWindow.decorCtx, -1) < 0) break;
	}
	libdecor_dispatch(midWindow.decorCtx, 0);

	atomic_store_explicit(&midWindow.running, true, memory_order_release);
	clock_gettime(CLOCK_MONOTONIC, &midWindow.start);
}

void midUpdateWindowInput()
{
	if (midWindowInput.leftMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.leftMouse = MID_PHASE_NONE;
	if (midWindowInput.rightMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.rightMouse = MID_PHASE_NONE;
	if (midWindowInput.middleMouse == MID_PHASE_DOUBLE_CLICK) midWindowInput.middleMouse = MID_PHASE_NONE;

	if (midWindowInput.leftMouse == MID_PHASE_PRESS) midWindowInput.leftMouse = MID_PHASE_HELD;
	if (midWindowInput.rightMouse == MID_PHASE_PRESS) midWindowInput.rightMouse = MID_PHASE_HELD;
	if (midWindowInput.middleMouse == MID_PHASE_PRESS) midWindowInput.middleMouse = MID_PHASE_HELD;

	if (midWindowInput.leftMouse == MID_PHASE_RELEASE) midWindowInput.leftMouse = MID_PHASE_NONE;
	if (midWindowInput.rightMouse == MID_PHASE_RELEASE) midWindowInput.rightMouse = MID_PHASE_NONE;
	if (midWindowInput.middleMouse == MID_PHASE_RELEASE) midWindowInput.middleMouse = MID_PHASE_NONE;

	midWindowInput.iMouseDeltaX = 0;
	midWindowInput.iMouseDeltaY = 0;
	midWindowInput.fMouseDeltaX = 0;
	midWindowInput.fMouseDeltaY = 0;

	libdecor_dispatch(midWindow.decorCtx, 0);

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	double delta = (double)(now.tv_sec - midWindow.start.tv_sec)
	               + (double)(now.tv_nsec - midWindow.start.tv_nsec) * 1e-9;
	midWindowInput.deltaTime = delta;
	midWindow.start = now;

#define TITLE_BUFFER_SIZE 64
	static double accumulated = 0.0;
	static int titleUpdateRate = 64;
	accumulated += midWindowInput.deltaTime;
	if (!--titleUpdateRate) {
		titleUpdateRate = 64;
		static char titleBuffer[TITLE_BUFFER_SIZE];
		snprintf(titleBuffer, TITLE_BUFFER_SIZE, "%s | FPS=%.2f | GPU=%.4f | CPU=%.4f",
		         WINDOW_NAME, 64.0 / accumulated, gpuTimeQueryMs, cpuTimeQueryMs);
		accumulated = 0.0;
		libdecor_frame_set_title(midWindow.decorFrame, titleBuffer);
	}
}

void midWindowLockCursor()
{
	midWindowInput.cursorLocked = MID_INPUT_LOCK_CURSOR_ENABLED;
}

void midWindowReleaseCursor()
{
	midWindowInput.cursorLocked = MID_INPUT_LOCK_CURSOR_DISABLED;
}

#endif // _WIN32 / Linux

#undef MID_WINDOW_IMPLEMENTATION
#endif // MID_WINDOW_IMPLEMENTATION
