#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <malloc.h>
#include "../../shared/constants.h"
#include "../../shared/data.h"
#include "../../shared/platform-interface.h"
#include "../../shared/debug.h"
#include "web-audio.h"
#include "web-input.h"

static struct {
    int32_t width;
    int32_t height;
} windowState = {};

float lastTime = 0.0f;


static EM_BOOL loop(double time, void *userData) {
    if (lastTime == 0.0f) {
        lastTime = (float) time;
    }

    float dt = time - lastTime;
    lastTime = time;

    web_updateAudio();
    game_update(dt);
    game_draw();

    return EM_TRUE;
}

static EM_BOOL onResize(int eventType, const EmscriptenUiEvent *uiEvent, void *userData) {
    windowState.width = uiEvent->windowInnerWidth;
    windowState.height = uiEvent->windowInnerHeight;
    game_resize(windowState.width, windowState.height);
    emscripten_set_canvas_element_size("#canvas", (double) windowState.width, (double) windowState.height);

    return EM_TRUE;
}



static void initialize(void) {
    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_FALSE, NULL);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_FALSE, NULL); 
    emscripten_set_gamepadconnected_callback(NULL, EM_FALSE, NULL);
    emscripten_set_gamepaddisconnected_callback(NULL, EM_FALSE, NULL);

    web_initAudio();

    if (!game_init(& (Game_InitOptions) {
        .keyboardFullscreenInstructions = true,
        .hideQuitInstructions = true
    })) {
        return;
    }

    game_resize(windowState.width, windowState.height);
    emscripten_request_animation_frame_loop(loop, NULL);

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_FALSE, web_onKeyDown);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_FALSE, web_onKeyUp); 
    emscripten_set_gamepadconnected_callback(NULL, EM_FALSE, web_onGamepadConnected);
    emscripten_set_gamepaddisconnected_callback(NULL, EM_FALSE, web_onGamepadDisconnected);
    emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW, NULL, EM_FALSE, onResize);

    emscripten_set_element_css_size("#start-text", 0.0, 0.0);
}

static EM_BOOL onGamepadInitialize(int eventType, const EmscriptenGamepadEvent *gamepadEvent, void *userData) {
    initialize();
    return web_onGamepadConnected(eventType, gamepadEvent, userData);
}

static EM_BOOL onKeyboardInitialize(int eventType, const EmscriptenKeyboardEvent *keyEvent, void *userData) {
    initialize();
    return web_onKeyDown(eventType, keyEvent, userData);
}

int32_t main() {
    double windowWidth = 0.0;
    double windowHeight = 0.0;
    emscripten_get_element_css_size("#canvas", &windowWidth, &windowHeight);
    windowState.width = (int32_t) windowWidth;
    windowState.height = (int32_t) windowHeight;

    emscripten_set_canvas_element_size("#canvas", windowWidth, windowHeight);
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl = emscripten_webgl_create_context("#canvas", & (EmscriptenWebGLContextAttributes) {
        .majorVersion = 2,
        .minorVersion = 0
    });
    emscripten_webgl_make_context_current(gl);

    emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_FALSE, onKeyboardInitialize);
    emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, NULL, EM_FALSE, onKeyboardInitialize); 
    emscripten_set_gamepadconnected_callback(NULL, EM_FALSE, onGamepadInitialize);
    emscripten_set_gamepaddisconnected_callback(NULL, EM_FALSE, onGamepadInitialize);

    return 0;
}

void platform_debugMessage(const char* message) {
    int32_t length = 0;
    while(message[length]) {
        ++length;
    }

    write(STDERR_FILENO, message, length);
    write(STDERR_FILENO, "\n", 1);
}

void platform_userMessage(const char* message) {
    platform_debugMessage(message);
}

bool platform_loadFile(const char* fileName, Data_Buffer* buffer, bool nullTerminate) {
    int32_t fd = open(fileName, O_RDONLY);
    uint8_t* data = 0;

    if (fd == -1) {
        DEBUG_LOG("platform_loadFile: Failed to open file.");
        goto ERROR_NO_RESOURCES;
    }

    int32_t size = lseek(fd, 0, SEEK_END);
    int32_t allocation = size;

    if (nullTerminate) {
        allocation += 1;
    }

    if (size == -1) {
        DEBUG_LOG("platform_loadFile: Failed to get file size.");
        goto ERROR_FILE_OPENED;
    }

    if (lseek(fd, 0, SEEK_SET) == -1) {
        DEBUG_LOG("platform_loadFile: Failed to reset file cursor.");
        goto ERROR_FILE_OPENED;
    }

    data = (uint8_t*) malloc(allocation);

    if (!data) {
        DEBUG_LOG("platform_loadFile: Failed to allocate data.");
        goto ERROR_FILE_OPENED;
    }

    if (read(fd, data, size) == -1) {
        DEBUG_LOG("platform_loadFile: Failed to read data.");
        goto ERROR_DATA_ALLOCATED;
    }

    if (nullTerminate) {
        data[allocation - 1] = 0;
    }

    buffer->data = data;
    buffer->size = allocation;
    close(fd);

    return true;

    ERROR_DATA_ALLOCATED:
    free(data);
    
    ERROR_FILE_OPENED:
    close(fd);
    
    ERROR_NO_RESOURCES:
    return false;
}