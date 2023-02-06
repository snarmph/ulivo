#define ULIVO_BACKEND_DATA
#include "../ulivo.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <windowsx.h> // GET_X_LPARAM(), GET_Y_LPARAM()

#ifndef DONT_USE_COLLA
    #include <tracelog.h>
#endif

#pragma comment(lib, "user32.lib")

#define WINDOW_CLASS_NAME TEXT("ULIVO-BACKEND-WIN32")
#define VK_NUMPAD_ENTER (VK_RETURN + KF_EXTENDED)

static LRESULT wndProc(HWND, UINT, WPARAM, LPARAM);
static bool ansiToWide(const char *cstr, size_t src_len, wchar_t *buf, size_t buflen);
static int win32ToMouse(uintptr_t virtual_key);
static int win32ToKeys(uintptr_t virtual_key);
static LONGLONG timer_muldiv(LONGLONG value, LONGLONG numer, LONGLONG denom);

static HINSTANCE hinstance = NULL;
static HWND hwnd = NULL;
static float frame_time = 1.f / 60.f;
static LARGE_INTEGER timer_start = {0};
static LARGE_INTEGER timer_freq = {0};
static uint64_t timer_last = 0;

void *uv__backend_create_window(const char *name, int width, int height) {
    hinstance = GetModuleHandle(NULL);

    RegisterClassEx(&(WNDCLASSEX){
        .cbSize = sizeof(WNDCLASSEX),
        .style = CS_CLASSDC,
        .lpfnWndProc = wndProc,
        .hInstance = hinstance,
        .lpszClassName = WINDOW_CLASS_NAME
    });

#if UNICODE
    wchar_t window_name[255] = {0};
    if (!ansiToWide(name, strlen(name), window_name, 255)) {
		fatal("couldn't convert window name (%s) to wchar", window_name);
    }
#else
	const char *window_name = name;
#endif

    hwnd = CreateWindow(
        WINDOW_CLASS_NAME,
        window_name,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        width, height,
        NULL, NULL,
        hinstance,
        NULL
    );

    ShowWindow(hwnd, SW_SHOWDEFAULT);
    UpdateWindow(hwnd);

	// initialize timer

	QueryPerformanceFrequency(&timer_freq);
	QueryPerformanceCounter(&timer_start);

    return hwnd;
}

void uv__backend_destroy_window(void *win_data) {
    DestroyWindow(hwnd);
    UnregisterClass(WINDOW_CLASS_NAME, hinstance);
    hwnd = NULL;
    hinstance = NULL;
}

bool uv__backend_poll_input(void *win_data) {
    bool is_open = true;

    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        if (msg.message == WM_QUIT) {
            is_open = false;
        }
    }

	// update timer (copied straight from sokol_time.h)

	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	LONGLONG dt = 1;
	if (timer_last && now.QuadPart > timer_last) {
		dt = now.QuadPart - timer_last;
	}
	timer_last = now.QuadPart;
	frame_time = (float)((double)dt / (double)timer_freq.QuadPart);

    return is_open;
}

float uv__backend_get_delta_time(void *win_data) {
    return frame_time;
}

// == STATIC FUNCTIONS ========================================================

static LRESULT wndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SIZE:
        uvOnWindowResize(LOWORD(lparam), HIWORD(lparam));
    #if 0
		if (gfx::device && wparam != SIZE_MINIMIZED) {
			win_size = (vec2i){ LOWORD(lparam), HIWORD(lparam) };
			
			// gfx::cleanupImGuiRTV();
			// gfx::createImGuiRTV();
		}
    #endif
		return 0;

	case WM_SYSCOMMAND:
		if ((wparam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	{
		bool is_key_down = (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN);
		uintptr_t vk = wparam;
		if ((wparam == VK_RETURN) && (HIWORD(lparam) & KF_EXTENDED))
			vk = VK_RETURN + KF_EXTENDED;
		
        uvSetKeyState(win32ToKeys(vk), is_key_down);
		return 0;
	}

	case WM_MOUSEMOVE:
	{
        uvSetMousePosition((vec2i){ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
		return 0;
	}

	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK:
	case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:
	case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:
	{
        uvSetMouseState(win32ToMouse(msg), true);
		return 0;
	}
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
        uvSetMouseState(win32ToMouse(msg), false);
		return 0;
	}
	case WM_MOUSEWHEEL:
		// float mouse_wheel = (float)GET_WHEEL_DELTA_WPARAM(wparam) / (float)WHEEL_DELTA;
		return 0;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

static bool ansiToWide(const char *cstr, size_t src_len, wchar_t *buf, size_t buflen) {
    if (src_len == 0) src_len = strlen(cstr);
    int result_len = MultiByteToWideChar(CP_UTF8, 0, cstr, (int)src_len, buf, (int)buflen);
    return result_len <= (int)buflen;
}


static int win32ToMouse(uintptr_t virtual_key) {
	switch (virtual_key) {
	case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: case WM_LBUTTONUP: return UV_MOUSE_LEFT;
	case WM_RBUTTONDOWN: case WM_RBUTTONDBLCLK:	case WM_RBUTTONUP: return UV_MOUSE_RIGHT;
	case WM_MBUTTONDOWN: case WM_MBUTTONDBLCLK:	case WM_MBUTTONUP: return UV_MOUSE_MIDDLE;
	}
	return UV_MOUSE_NONE;
}

static int win32ToKeys(uintptr_t virtual_key) {
	switch (virtual_key) {
	case '0': return UV_KEY_0;
	case '1': return UV_KEY_1;
	case '2': return UV_KEY_2;
	case '3': return UV_KEY_3;
	case '4': return UV_KEY_4;
	case '5': return UV_KEY_5;
	case '6': return UV_KEY_6;
	case '7': return UV_KEY_7;
	case '8': return UV_KEY_8;
	case '9': return UV_KEY_9;
	case 'A': return UV_KEY_A;
	case 'B': return UV_KEY_B;
	case 'C': return UV_KEY_C;
	case 'D': return UV_KEY_D;
	case 'E': return UV_KEY_E;
	case 'F': return UV_KEY_F;
	case 'G': return UV_KEY_G;
	case 'H': return UV_KEY_H;
	case 'I': return UV_KEY_I;
	case 'J': return UV_KEY_J;
	case 'K': return UV_KEY_K;
	case 'L': return UV_KEY_L;
	case 'M': return UV_KEY_M;
	case 'N': return UV_KEY_N;
	case 'O': return UV_KEY_O;
	case 'P': return UV_KEY_P;
	case 'Q': return UV_KEY_Q;
	case 'R': return UV_KEY_R;
	case 'S': return UV_KEY_S;
	case 'T': return UV_KEY_T;
	case 'U': return UV_KEY_U;
	case 'V': return UV_KEY_V;
	case 'W': return UV_KEY_W;
	case 'X': return UV_KEY_X;
	case 'Y': return UV_KEY_Y;
	case 'Z': return UV_KEY_Z;
	case VK_F1:  return UV_KEY_F1;
	case VK_F2:  return UV_KEY_F2;
	case VK_F3:  return UV_KEY_F3;
	case VK_F4:  return UV_KEY_F4;
	case VK_F5:  return UV_KEY_F5;
	case VK_F6:  return UV_KEY_F6;
	case VK_F7:  return UV_KEY_F7;
	case VK_F8:  return UV_KEY_F8;
	case VK_F9:  return UV_KEY_F9;
	case VK_F10: return UV_KEY_F10;
	case VK_F11: return UV_KEY_F11;
	case VK_F12: return UV_KEY_F12;
	case VK_F13: return UV_KEY_F13;
	case VK_F14: return UV_KEY_F14;
	case VK_F15: return UV_KEY_F15;
	case VK_F16: return UV_KEY_F16;
	case VK_F17: return UV_KEY_F17;
	case VK_F18: return UV_KEY_F18;
	case VK_F19: return UV_KEY_F19;
	case VK_F20: return UV_KEY_F20;
	case VK_F21: return UV_KEY_F21;
	case VK_F22: return UV_KEY_F22;
	case VK_F23: return UV_KEY_F23;
	case VK_F24: return UV_KEY_F24;
	case VK_TAB:          return UV_KEY_TAB;
	case VK_LEFT:         return UV_KEY_LEFT;
	case VK_RIGHT:        return UV_KEY_RIGHT;
	case VK_UP:           return UV_KEY_UP;
	case VK_DOWN:         return UV_KEY_DOWN;
	case VK_PRIOR:        return UV_KEY_PRIOR;
	case VK_NEXT:         return UV_KEY_NEXT;
	case VK_HOME:         return UV_KEY_HOME;
	case VK_END:          return UV_KEY_END;
	case VK_INSERT:       return UV_KEY_INSERT;
	case VK_DELETE:       return UV_KEY_DELETE;
	case VK_BACK:         return UV_KEY_BACK;
	case VK_SPACE:        return UV_KEY_SPACE;
	case VK_RETURN:       return UV_KEY_RETURN;
	case VK_ESCAPE:       return UV_KEY_ESCAPE;
	case VK_OEM_7:        return UV_KEY_APOSTROPHE;
	case VK_OEM_COMMA:    return UV_KEY_COMMA;
	case VK_OEM_MINUS:    return UV_KEY_MINUS;
	case VK_OEM_PERIOD:   return UV_KEY_PERIOD;
	case VK_OEM_2:        return UV_KEY_SLASH;
	case VK_OEM_1:        return UV_KEY_SEMICOLON;
	case VK_OEM_PLUS:     return UV_KEY_EQUAL;
	case VK_OEM_4:        return UV_KEY_LBRACKET;
	case VK_OEM_6:        return UV_KEY_RBRACKER;
	case VK_OEM_5:        return UV_KEY_BACKSLASH;
	case VK_CAPITAL:      return UV_KEY_CAPSLOCK;
	case VK_SCROLL:       return UV_KEY_SCROLLLOCK;
	case VK_NUMLOCK:      return UV_KEY_NUMLOCK;
	case VK_PRINT:        return UV_KEY_PRINT;
	case VK_PAUSE:        return UV_KEY_PAUSE;
	case VK_NUMPAD0:      return UV_KEY_KEYPAD0;
	case VK_NUMPAD1:      return UV_KEY_KEYPAD1;
	case VK_NUMPAD2:      return UV_KEY_KEYPAD2;
	case VK_NUMPAD3:      return UV_KEY_KEYPAD3;
	case VK_NUMPAD4:      return UV_KEY_KEYPAD4;
	case VK_NUMPAD5:      return UV_KEY_KEYPAD5;
	case VK_NUMPAD6:      return UV_KEY_KEYPAD6;
	case VK_NUMPAD7:      return UV_KEY_KEYPAD7;
	case VK_NUMPAD8:      return UV_KEY_KEYPAD8;
	case VK_NUMPAD9:      return UV_KEY_KEYPAD9;
	case VK_NUMPAD_ENTER: return UV_KEY_KEYPADENTER;
	case VK_DECIMAL:      return UV_KEY_DECIMAL;
	case VK_MULTIPLY:     return UV_KEY_MULTIPLY;
	case VK_SUBTRACT:     return UV_KEY_SUBTRACT;
	case VK_ADD:          return UV_KEY_ADD;
	case VK_LSHIFT:       return UV_KEY_LSHIFT;
	case VK_LCONTROL:     return UV_KEY_LCTRL;
	case VK_LMENU:        return UV_KEY_LALT;
	case VK_LWIN:         return UV_KEY_LWIN;
	case VK_RSHIFT:       return UV_KEY_RSHIFT;
	case VK_RCONTROL:     return UV_KEY_RCTRL;
	case VK_RMENU:        return UV_KEY_RALT;
	case VK_RWIN:         return UV_KEY_RWIN;
	case VK_APPS:         return UV_KEY_APPS;
	}

	return UV_KEY_NONE;
}

// prevent 64-bit overflow when computing relative timestamp
// see https://gist.github.com/jspohr/3dc4f00033d79ec5bdaf67bc46c813e3
static LONGLONG timer_muldiv(LONGLONG value, LONGLONG numer, LONGLONG denom) {
	LONGLONG q = value / denom;
	LONGLONG r = value % denom;
	return q * numer + r * numer / denom;
}