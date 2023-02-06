# ulivo

Single header/source pair frontend graphics library

To work, this libary needs a backend! (look at [backends](src/backend/))

## Example
```c
#include "ulivo.h"

int main() {
    uvCreateWindow("Example", 800, 600, NULL);

    vec2 line_start = v2(400, 300);
    float line_angle = 0.f;
    texture_t tex = uvLoadTexture("assets/image.jpg");
    if (!tex) {
        printf("[ERROR] Couldn't load texture\n");
        return 1;
    }

    colour_t col_start = UV_RED;
    colour_t col_end = UV_BLUE;
    float col_timer = 0.f;

    while (uvIsOpen()) {
        if (uvIsKeyPressed(UV_KEY_ESCAPE)) {
            uvCloseWindow();
        }

        col_timer += uvGetDeltaTime() * 0.5f;
        float t = sinf(col_timer) / 2.f + 0.5f;
        uvSetClearColour(v4lerp(col_start, col_end, t));

        uvDrawTriangle(v2(10, 20), v2(10, 100), v2(100, 100));

        line_angle += 0.01f;
        vec2 line_angle_end = v2(100 * cosf(line_angle), 100 * sinf(line_angle));
        vec2 line_end = v2add(line_start, line_angle_end);
        uvSetTexture(tex);
        uvDrawLine(line_start, line_end, 100, UV_WHITE);
        uvClearTexture();

        uvEndFrame();
    }

    uvCleanup();
}
```
