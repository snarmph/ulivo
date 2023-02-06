#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>

typedef unsigned int uint;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef size_t    usize;
typedef ptrdiff_t isize;
typedef uintptr_t uptr;

typedef union {
    struct { float x, y; };
    struct { float u, v; };
#ifdef __cplusplus
	vec2 operator+(const vec2 &v) { return v2add(*this, v); }
	vec2 operator-(const vec2 &v) { return v2sub(*this, v); }
	vec2 operator*(const vec2 &v) { return v2mul(*this, v); }
	vec2 operator/(const vec2 &v) { return v2div(*this, v); }
	vec2 &operator+=(const vec2 &v) { x += v.x; y += v.y; return *this; }
	vec2 &operator-=(const vec2 &v) { x -= v.x; y -= v.y; return *this; }
	vec2 &operator*=(const vec2 &v) { x *= v.x; y *= v.y; return *this; }
	vec2 &operator/=(const vec2 &v) { x /= v.x; y /= v.y; return *this; }
#endif
} vec2;

typedef union {
    struct { float x, y, z; };
    struct { float r, g, b; };
#ifdef __cplusplus
	vec3 operator+(const vec3 &v) { return v3add(*this, v); }
	vec3 operator-(const vec3 &v) { return v3sub(*this, v); }
	vec3 operator*(const vec3 &v) { return v3mul(*this, v); }
	vec3 operator/(const vec3 &v) { return v3div(*this, v); }
	vec3 &operator+=(const vec3 &v) { x += v.x; y += v.y; z += v.z; return *this; }
	vec3 &operator-=(const vec3 &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
	vec3 &operator*=(const vec3 &v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
	vec3 &operator/=(const vec3 &v) { x /= v.x; y /= v.y; z /= v.z; return *this; }
#endif
} vec3;

typedef union {
    struct { float x, y, z, w; };
    struct { float r, g, b, a; };
#ifdef __cplusplus
	vec4 operator+(const vec4 &v) { return v4add(*this, v); }
	vec4 operator-(const vec4 &v) { return v4sub(*this, v); }
	vec4 operator*(const vec4 &v) { return v4mul(*this, v); }
	vec4 operator/(const vec4 &v) { return v4div(*this, v); }
	vec4 &operator+=(const vec4 &v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
	vec4 &operator-=(const vec4 &v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
	vec4 &operator*=(const vec4 &v) { x *= v.x; y *= v.y; z *= v.z; w *= v.w; return *this; }
	vec4 &operator/=(const vec4 &v) { x /= v.x; y /= v.y; z /= v.z; w /= v.w; return *this; }
#endif
} vec4;

typedef struct { int x, y; } vec2i;
typedef struct { int x, y, z; } vec3i;
typedef struct { int x, y, z, w; } vec4i;

typedef struct { uint x, y; } vec2u;
typedef struct { uint x, y, z; } vec3u;
typedef struct { uint x, y, z, w; } vec4u;

typedef vec4 colour_t;
typedef uptr texture_t;

typedef struct {
	u8 *data;
	u32 width, height;
} image_t;

typedef struct {
    int nothing;
} uv_options_t;

void uvCreateWindow(const char *name, int width, int height, const uv_options_t *options);
void uvCloseWindow(void);
void uvCleanup(void);
bool uvIsOpen(void);
vec2i uvGetWindowSize(void);
float uvGetDeltaTime(void);

void uvSetClearColour(colour_t colour);
void uvEndFrame(void);

bool uvIsKeyDown(int key);
bool uvIsKeyUp(int key);
bool uvIsKeyPressed(int key);
bool uvIsMouseDown(int mouse);
bool uvIsMouseUp(int mouse);
vec2i uvGetMousePos(void);
vec2i uvGetMousePosRel(void);
float uvGetMouseWheel(void);

image_t uvLoadImage(const char *filename);
image_t uvLoadImageFromMemory(const u8 *buffer, size_t buflen);
void uvFreeImage(image_t *image);

texture_t uvLoadTexture(const char *filename);
texture_t uvLoadTextureFromImage(const image_t *img);
void uvFreeTexture(texture_t texture);

void uvSetTexture(texture_t texture);
void uvClearTexture(void);
// void uvDrawVertices(vertex_t *vertices, u32 count);
// void uvDrawIndices(vertex_t *vertices, u32 vtx_count, index_t *indices, u32 idx_count);
void uvDrawLine(vec2 start, vec2 end, float thichness, colour_t colour);
void uvDrawLineBezier(vec2 start, vec2 end, float thickness, colour_t colour);
void uvDrawLineBezierQuad(vec2 start, vec2 end, vec2 control_point, float thickness, colour_t colour);
void uvDrawLineBeziercubic(vec2 start, vec2 end, vec2 start_control, vec2 end_control, float thickness, colour_t colour);
void uvDrawCircle(vec2 centre, float radius, colour_t colour);
void uvDrawCircleEx(vec2 centre, float radius, colour_t colour, uint segments);
void uvDrawArc(vec2 centre, float radius, float start_angle, float end_angle, uint segments, colour_t colour);
void uvDrawArcLines(vec2 centre, float radius, float start_angle, float end_angle, uint segments, colour_t colour, float thickness);
void uvDrawRing(vec2 centre, float inner_radius, float outer_radius, float start_angle, float end_angle, uint segments, colour_t colour);
void uvDrawQuad(vec2 position, vec2 size, colour_t colour);
void uvDrawQuadRot(vec2 position, vec2 size, colour_t colour, float rotation);
void uvDrawQuadLines(vec2 position, vec2 size, colour_t colour, float rotation, float thickness);
void uvDrawQuadRounded(vec2 position, vec2 size, colour_t colour, float rotation, float roundness, uint segments);
void uvDrawQuadRoundedLines(vec2 position, vec2 size, colour_t colour, float rotation, float roundness, uint segments, float thickness);
void uvDrawTriangle(vec2 v1, vec2 v2, vec2 v3, colour_t colour);
void uvDrawTriangleLines(vec2 v1, vec2 v2, vec2 v3, colour_t colour, float thickness);

// void gfxDrawSprite(gfx_t *ctx);

// == USEFUL MATH STUFF ==================================================================

#define v2(x, y)       (vec2){ x, y }
#define v3(x, y, z)    (vec3){ x, y, z }
#define v4(x, y, z, w) (vec4){ x, y, z, w }

#define v2add(a, b) v2((a).x + (b).x, (a).y + (b).y)
#define v2sub(a, b) v2((a).x - (b).x, (a).y - (b).y)
#define v2mul(a, b) v2((a).x * (b).x, (a).y * (b).y)
#define v2div(a, b) v2((a).x / (b).x, (a).y / (b).y)

#define v3add(a, b) v3((a).x + (b).x, (a).y + (b).y, (a).z + (b).z)
#define v3sub(a, b) v3((a).x - (b).x, (a).y - (b).y, (a).z - (b).z)
#define v3mul(a, b) v3((a).x * (b).x, (a).y * (b).y, (a).z * (b).z)
#define v3div(a, b) v3((a).x / (b).x, (a).y / (b).y, (a).z / (b).z)

#define v4add(a, b) v4((a).x + (b).x, (a).y + (b).y, (a).z + (b).z, (a).w + (b).w)
#define v4sub(a, b) v4((a).x - (b).x, (a).y - (b).y, (a).z - (b).z, (a).w - (b).w)
#define v4mul(a, b) v4((a).x * (b).x, (a).y * (b).y, (a).z * (b).z, (a).w * (b).w)
#define v4div(a, b) v4((a).x / (b).x, (a).y / (b).y, (a).z / (b).z, (a).w / (b).w)

#define v2mag2(v)       ((v).x * (v).x + (v).y * (v).y)
#define v2mag(v)        sqrtf(v2mag2(v))
#define v2lerp(s, e, t) v2((s).x + t * ((e).x - (s).x), (s).y + t * ((e).y - (s).y))

#define v3mag2(v)       ((v).x * (v).x + (v).y * (v).y + (v).z * (v).z)
#define v3mag(v)        sqrtf(v3mag2(v))
#define v3lerp(s, e, t) v3((s).x + t * ((e).x - (s).x), (s).y + t * ((e).y - (s).y), (s).z + t * ((e).z - (s).z))

#define v4mag2(v)       ((v).x * (v).x + (v).y * (v).y + (v).z * (v).z + (v).w * (v).w)
#define v4mag(v)        sqrtf(v4mag2(v))
#define v4lerp(s, e, t) v4((s).x + t * ((e).x - (s).x), (s).y + t * ((e).y - (s).y), (s).z + t * ((e).z - (s).z), (s).w + t * ((e).w - (s).w))

// == COLOURS ============================================================================

#define UV_WHITE    v4(1, 1, 1, 1)
#define UV_BLACK    v4(0, 0, 0, 1)

#define UV_RED      v4(1, 0, 0, 1)
#define UV_GREEN    v4(0, 1, 0, 1)
#define UV_BLUE     v4(0, 0, 1, 1)

// == INPUT ==============================================================================

enum Mouse {
	UV_MOUSE_NONE,
	UV_MOUSE_LEFT,
	UV_MOUSE_RIGHT,
	UV_MOUSE_MIDDLE,
    UV_MOUSE__COUNT,
};

enum Keys {
	UV_KEY_NONE,
	UV_KEY_0,
	UV_KEY_1,
	UV_KEY_2,
	UV_KEY_3,
	UV_KEY_4,
	UV_KEY_5,
	UV_KEY_6,
	UV_KEY_7,
	UV_KEY_8,
	UV_KEY_9,
	UV_KEY_A,
	UV_KEY_B,
	UV_KEY_C,
	UV_KEY_D,
	UV_KEY_E,
	UV_KEY_F,
	UV_KEY_G,
	UV_KEY_H,
	UV_KEY_I,
	UV_KEY_J,
	UV_KEY_K,
	UV_KEY_L,
	UV_KEY_M,
	UV_KEY_N,
	UV_KEY_O,
	UV_KEY_P,
	UV_KEY_Q,
	UV_KEY_R,
	UV_KEY_S,
	UV_KEY_T,
	UV_KEY_U,
	UV_KEY_V,
	UV_KEY_W,
	UV_KEY_X,
	UV_KEY_Y,
	UV_KEY_Z,
	UV_KEY_F1,
	UV_KEY_F2,
	UV_KEY_F3,
	UV_KEY_F4,
	UV_KEY_F5,
	UV_KEY_F6,
	UV_KEY_F7,
	UV_KEY_F8,
	UV_KEY_F9,
	UV_KEY_F10,
	UV_KEY_F11,
	UV_KEY_F12,
	UV_KEY_F13,
	UV_KEY_F14,
	UV_KEY_F15,
	UV_KEY_F16,
	UV_KEY_F17,
	UV_KEY_F18,
	UV_KEY_F19,
	UV_KEY_F20,
	UV_KEY_F21,
	UV_KEY_F22,
	UV_KEY_F23,
	UV_KEY_F24,
	UV_KEY_TAB,
	UV_KEY_LEFT,
	UV_KEY_RIGHT,
	UV_KEY_UP,
	UV_KEY_DOWN,
	UV_KEY_PRIOR,
	UV_KEY_NEXT,
	UV_KEY_HOME,
	UV_KEY_END,
	UV_KEY_INSERT,
	UV_KEY_DELETE,
	UV_KEY_BACK,
	UV_KEY_SPACE,
	UV_KEY_RETURN,
	UV_KEY_ESCAPE,
	UV_KEY_APOSTROPHE,
	UV_KEY_COMMA,
	UV_KEY_MINUS,
	UV_KEY_PERIOD,
	UV_KEY_SLASH,
	UV_KEY_SEMICOLON,
	UV_KEY_EQUAL,
	UV_KEY_LBRACKET,
	UV_KEY_RBRACKER,
	UV_KEY_BACKSLASH,
	UV_KEY_CAPSLOCK,
	UV_KEY_SCROLLLOCK,
	UV_KEY_NUMLOCK,
	UV_KEY_PRINT,
	UV_KEY_PAUSE,
	UV_KEY_KEYPAD0,
	UV_KEY_KEYPAD1,
	UV_KEY_KEYPAD2,
	UV_KEY_KEYPAD3,
	UV_KEY_KEYPAD4,
	UV_KEY_KEYPAD5,
	UV_KEY_KEYPAD6,
	UV_KEY_KEYPAD7,
	UV_KEY_KEYPAD8,
	UV_KEY_KEYPAD9,
	UV_KEY_KEYPADENTER,
	UV_KEY_DECIMAL,
	UV_KEY_MULTIPLY,
	UV_KEY_SUBTRACT,
	UV_KEY_ADD,
	UV_KEY_LSHIFT,
	UV_KEY_LCTRL,
	UV_KEY_LALT,
	UV_KEY_LWIN,
	UV_KEY_RSHIFT,
	UV_KEY_RCTRL,
	UV_KEY_RALT,
	UV_KEY_RWIN,
	UV_KEY_APPS,
	UV_KEY__COUNT,
};

#ifdef ULIVO_BACKEND_DATA
typedef struct {
    vec2 pos;
    vec2 uv;
    colour_t col;
} uv_vertex_t;

typedef u32 uv_index_t;

typedef struct {
    u32 vtx_start, vtx_count;
    u32 idx_start, idx_count;
    texture_t texture;
} uv_batch_t;

typedef struct {
    uv_batch_t *batches;
    u32 batch_count;
    uv_vertex_t *vertices;
    uv_index_t *indices;
    u32 vtx_count;
    u32 idx_count;
} uv_drawdata_t;

extern void *uv__backend_create_window(const char *name, int width, int height);
extern void uv__backend_destroy_window(void *win_data);

extern bool uv__backend_poll_input(void *win_data);
extern float uv__backend_get_delta_time(void *win_data);

extern void uv__backend_init_gfx(void);
extern void uv__backend_cleanup_gfx(void);
extern void uv__backend_resize_gfx(int new_width, int new_height);
extern void uv__backend_draw(colour_t colour, uv_drawdata_t *data);
extern texture_t uv__backend_load_texture(const image_t *image);
extern void uv__backend_free_texture(texture_t texture);

void uvOnWindowResize(int new_width, int new_height);
void uvSetKeyState(int key, bool state);
void uvSetMouseState(int btn, bool state);
void uvSetMousePosition(vec2i position);

#endif