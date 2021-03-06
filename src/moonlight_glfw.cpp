#include <stdio.h>
#include <stdlib.h>
#include <GLFW/glfw3.h>
#include "glsym/glsym.h"
#include "Application.hpp"
#include "Settings.hpp"
#include "Limelight.h"
#include "libretro.h"
#include "InputController.hpp"

extern retro_input_state_t input_state_cb;

static int mouse_x = 0, mouse_y = 0;
static int mouse_l = 0, mouse_r = 0;

static int glfw_keyboard_state[GLFW_KEY_LAST];

#define GLFW_KEY_TO_RETRO(RETRO, KEY) \
    if (id == RETRO) return glfw_keyboard_state[KEY];

static int16_t glfw_input_state_cb(unsigned port, unsigned device, unsigned index, unsigned id) {
    if (device == RETRO_DEVICE_MOUSE) {
        if (id == RETRO_DEVICE_ID_MOUSE_X) {
            return mouse_x;
        } else if (id == RETRO_DEVICE_ID_MOUSE_Y) {
            return mouse_y;
        } else if (id == RETRO_DEVICE_ID_MOUSE_LEFT) {
            return mouse_l;
        } else if (id == RETRO_DEVICE_ID_MOUSE_RIGHT) {
            return mouse_r;
        }
    } else if (device == RETRO_DEVICE_JOYPAD) {
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_UP, GLFW_KEY_UP);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_DOWN, GLFW_KEY_DOWN);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_LEFT, GLFW_KEY_LEFT);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_RIGHT, GLFW_KEY_RIGHT);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_L, GLFW_KEY_Q);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_R, GLFW_KEY_E);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_L2, GLFW_KEY_Z);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_R2, GLFW_KEY_C);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_A, GLFW_KEY_A);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_B, GLFW_KEY_B);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_X, GLFW_KEY_X);
        GLFW_KEY_TO_RETRO(RETRO_DEVICE_ID_JOYPAD_Y, GLFW_KEY_Y);
    }
    return 0;
}

int main(int argc, const char * argv[]) {
    input_state_cb = glfw_input_state_cb;
    
    glfwInit();
    
    glfwSetErrorCallback([](int i, const char *error) {
        printf("GLFW error: %s\n", error);
    });
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Test", NULL, NULL);
    glfwMakeContextCurrent(window);
    rglgen_resolve_symbols(glfwGetProcAddress);
    glfwSwapInterval(1);
    
    glfwSetCursorPosCallback(window, [](GLFWwindow *w, double x, double y) {
        mouse_x = x;
        mouse_y = y;
    });

    glfwSetMouseButtonCallback(window, [](GLFWwindow *w, int button, int action, int modifiers) {
        if (button == 0) {
            mouse_l = action;
        } else if (button == 1) {
            mouse_r = action;
        }
    });
    
    glfwSetScrollCallback(window, [](GLFWwindow *w, double x, double y) {
        nanogui::scroll_callback_event(x, y);
    });
    
    glfwSetKeyCallback(window, [](GLFWwindow *w, int key, int scancode, int action, int mods) {
        glfw_keyboard_state[key] = action != GLFW_RELEASE;
    });
    
    int width, height, fb_width, fb_height;
    glfwGetWindowSize(window, &width, &height);
    glfwGetFramebufferSize(window, &fb_width, &fb_height);
    
    Settings::settings()->set_working_dir("/Users/rock88/Documents/RetroArch/system/moonlight");
    
    nanogui::init();
    nanogui::ref<Application> app = new Application(Size(width, height), Size(fb_width, fb_height));
    
    nanogui::setup(1.0 / 15.0);
    
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        InputController::controller()->handle_input(width, height);
        
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);
        
        nanogui::draw();
        
        glfwSwapBuffers(window);
    }
    return 0;
}
