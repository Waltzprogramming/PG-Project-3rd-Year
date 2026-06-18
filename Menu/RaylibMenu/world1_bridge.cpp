#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "../../headers/Map4.h"

namespace {
Mapa1 g_world1;
bool g_glewReady = false;
bool g_worldReady = false;
}

extern "C" bool PaperPinixWorld1PrepareOpenGL() {
    if (g_glewReady) {
        return true;
    }

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) {
        return false;
    }
    glGetError();
    g_glewReady = true;
    return true;
}

extern "C" bool PaperPinixWorld1Initialize(bool enableAudio) {
    if (!PaperPinixWorld1PrepareOpenGL()) {
        return false;
    }
    if (g_worldReady) {
        return true;
    }

    g_worldReady = g_world1.initialize(enableAudio);
    return g_worldReady;
}

extern "C" void PaperPinixWorld1Render(void* glfwWindow, float deltaTime) {
    if (!g_worldReady || glfwWindow == nullptr) {
        return;
    }
    g_world1.render(static_cast<GLFWwindow*>(glfwWindow), deltaTime);
}

extern "C" bool PaperPinixWorld1ShopOpen() {
    return g_worldReady && g_world1.shopOpen();
}

extern "C" void PaperPinixWorld1CloseShop() {
    if (g_worldReady) {
        g_world1.closeShop();
    }
}

extern "C" void PaperPinixWorld1Shutdown() {
    if (g_worldReady) {
        g_world1.shutdown();
        g_worldReady = false;
    }
}
