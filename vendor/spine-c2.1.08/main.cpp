// Dear ImGui: standalone example application for GLFW + OpenGL 3, using programmable pipeline
// (GLFW is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "glad/glad.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h>

#include "spine/Version.h"
#include "spine/spine.h"
#include "spine-opengl.h"
#include "SkeletonRenderer.h"

// [Win32] Our example includes a copy of glfw3.lib pre-compiled with VS2010 to maximize ease of testing and compatibility with old VS compilers.
// To link with VS2010-era libraries, VS2015+ requires linking with legacy_stdio_definitions.lib, which we do using this pragma.
// Your own project should not be affected, as you are likely to link with a newer binary of GLFW that is adequate for your version of Visual Studio.
#if defined(_MSC_VER) && (_MSC_VER >= 1900) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#pragma comment(lib, "legacy_stdio_definitions")
#endif

// This example can also compile and run with Emscripten! See 'Makefile.emscripten' for details.
#ifdef __EMSCRIPTEN__
#include "../libs/emscripten/emscripten_mainloop_stub.h"
#endif

#ifdef _WIN32
#include <windows.h>
extern int main(int argc, char** argv);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    int argc = __argc;
    char** argv = __argv;
    return main(argc, argv);
}
#endif

struct SpineCocosApp {
    std::unique_ptr<spine::SkeletonRenderer> renderer;
    spAtlas *atlas = nullptr;
    spAnimationStateData *stateData = nullptr;
    spAnimationState *state = nullptr;
    std::vector<std::string> animationNames;
    int currentAnimation = 0;
    bool loop = true;
    float posX = 0.0f;
    float posY = 0.0f;
    float scaleX = 0.5f;
    float scaleY = 0.5f;
    float playbackSpeed = 1.0f;
    bool premultipliedAlpha = false;
    float assetScale = 1.0f;
};

static int width = 800;
static int height = 600;
static float contentScale = 1.0f;
static SpineCocosApp g_spineApp;

static GLuint g_spineShader = 0;
static GLint g_uProjection = -1;
static GLint g_uTexture = -1;
static float g_projection[16] = {0};

static void disposeSpine(SpineCocosApp &state);
static bool loadSpineAssets(SpineCocosApp &state, const std::string &atlasPath, const std::string &skeletonPath);
static void updateSpine(SpineCocosApp &state, float deltaSeconds);
static void renderSpine(SpineCocosApp &state);
static void ensureOrthoProjection(int fbWidth, int fbHeight);
static spine::BlendFunc blendFuncForState(bool premultipliedAlpha);
static void applyRendererState(SpineCocosApp &state);
static void syncRendererTransform(SpineCocosApp &state);
static const char *selectGLSLVersion();
static GLFWwindow *createMainWindow();
static bool isWindowIconified(GLFWwindow *window);
static void setupImGuiContext(GLFWwindow *window, const char *glsl_version);
static void shutdownImGuiContext();
static void renderControlPanel(ImVec4 &clear_color, ImGuiIO &io, SpineCocosApp &state);
static float computeDeltaTime(double &lastTime);
static bool extractDroppedPaths(int count, const char **paths, std::string &atlasPath, std::string &skeletonPath);
static void configureImGuiStyle(float dpiScale);
static GLuint compileShader(GLenum type, const char *source);
static bool initSpineShader();
static void destroySpineShader();
static void playAnimation(SpineCocosApp &state, int index);
static bool endsWith(const std::string &value, const std::string &suffix);
static void resetDefaultTransform(SpineCocosApp &state);

static void resizeViewport(GLFWwindow * /*window*/, int new_width, int new_height) {
    width = new_width;
    height = new_height;
    glViewport(0, 0, width, height);
    ensureOrthoProjection(width, height);
}

static void drop_callback(GLFWwindow * /*window*/, int count, const char **paths) {
    std::string atlasPath;
    std::string skeletonPath;

    if (!extractDroppedPaths(count, paths, atlasPath, skeletonPath)) {
        if (!atlasPath.empty() || !skeletonPath.empty()) {
            std::cout << "Please drop both .atlas and .skel/.json files to load a complete Spine animation\n";
        }
        return;
    }

    if (loadSpineAssets(g_spineApp, atlasPath, skeletonPath)) {
        std::cout << "Spine animation loaded successfully\n";
    } else {
        std::cout << "Failed to load Spine animation\n";
    }
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);

#ifdef _WIN32
    MessageBoxA(NULL, description, "GLFW Error", MB_OK | MB_ICONERROR);
#endif
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = selectGLSLVersion();

    contentScale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor());
    GLFWwindow* window = createMainWindow();
    if (!window)
        return 1;

    glfwSwapInterval(1); // Enable vsync
    if (!gladLoadGL()) {
        printf("Failed to load GL\n");
        return -1;
    }

    spBone_setYDown(1);
    if (!initSpineShader()) {
        fprintf(stderr, "Failed to create Spine shader program\n");
        return -1;
    }

    resetDefaultTransform(g_spineApp);
    ensureOrthoProjection((int)(width * contentScale), (int)(height * contentScale));

    setupImGuiContext(window, glsl_version);
    ImGuiIO& io = ImGui::GetIO();

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
    double lastTime = glfwGetTime();

#ifdef __EMSCRIPTEN__
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        float delta = computeDeltaTime(lastTime);
        glfwPollEvents();
        if (isWindowIconified(window))
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

        updateSpine(g_spineApp, delta);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        renderControlPanel(clear_color, io, g_spineApp);

        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        renderSpine(g_spineApp);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    shutdownImGuiContext();
    disposeSpine(g_spineApp);
    destroySpineShader();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

static void resetDefaultTransform(SpineCocosApp &state) {
    state.posX = width * 0.5f;
    state.posY = height * 0.75f;
    state.scaleX = 0.5f;
    state.scaleY = 0.5f;
}

static bool endsWith(const std::string &value, const std::string &suffix) {
    if (suffix.size() > value.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

static void disposeSpine(SpineCocosApp &state) {
    if (state.state) {
        spAnimationState_dispose(state.state);
        state.state = nullptr;
    }
    if (state.stateData) {
        spAnimationStateData_dispose(state.stateData);
        state.stateData = nullptr;
    }
    state.renderer.reset();
    if (state.atlas) {
        spAtlas_dispose(state.atlas);
        state.atlas = nullptr;
    }
    state.animationNames.clear();
    state.currentAnimation = 0;
}

static bool loadSpineAssets(SpineCocosApp &state, const std::string &atlasPath, const std::string &skeletonPath) {
    disposeSpine(state);
    spAtlas *atlas = spAtlas_createFromFile(atlasPath.c_str(), nullptr);
    if (!atlas) {
        fprintf(stderr, "Failed to load atlas: %s\n", atlasPath.c_str());
        return false;
    }
    spSkeletonData *skeletonData = nullptr;
    if (endsWith(skeletonPath, ".json")) {
        spSkeletonJson *json = spSkeletonJson_create(atlas);
        json->scale = state.assetScale;
        skeletonData = spSkeletonJson_readSkeletonDataFile(json, skeletonPath.c_str());
        if (!skeletonData) {
            fprintf(stderr, "Error reading JSON skeleton: %s\n", skeletonPath.c_str());
        }
        spSkeletonJson_dispose(json);
    } else if (endsWith(skeletonPath, ".skel")) {
        fprintf(stderr, "Binary .skel loading is not supported by the Spine %s runtime. Please export JSON instead.\n", SPINE_VERSION_STRING);
        spAtlas_dispose(atlas);
        return false;
    } else {
        fprintf(stderr, "Unknown skeleton file format: %s\n", skeletonPath.c_str());
        spAtlas_dispose(atlas);
        return false;
    }
    if (!skeletonData) {
        spAtlas_dispose(atlas);
        return false;
    }
    state.atlas = atlas;
    state.renderer.reset(spine::SkeletonRenderer::createWithData(skeletonData, true));
    if (!state.renderer) {
        spAtlas_dispose(atlas);
        return false;
    }
    if (state.posX == 0.0f && state.posY == 0.0f) {
        resetDefaultTransform(state);
    }
    syncRendererTransform(state);
    applyRendererState(state);
    state.stateData = spAnimationStateData_create(state.renderer->skeleton->data);
    state.state = spAnimationState_create(state.stateData);
    state.state->timeScale = state.playbackSpeed;
    state.animationNames.clear();
    spSkeletonData *data = state.renderer->skeleton->data;
    for (int i = 0; i < data->animationsCount; ++i) {
        state.animationNames.emplace_back(data->animations[i]->name);
    }
    if (!state.animationNames.empty()) {
        state.currentAnimation = 0;
        playAnimation(state, 0);
    }
    return true;
}

static void playAnimation(SpineCocosApp &state, int index) {
    if (!state.state || index < 0 || index >= (int)state.animationNames.size()) return;
    state.currentAnimation = index;
    spAnimationState_setAnimationByName(state.state, 0, state.animationNames[index].c_str(), state.loop);
}

static void updateSpine(SpineCocosApp &state, float deltaSeconds) {
    if (!state.state || !state.renderer) return;
    state.state->timeScale = state.playbackSpeed;
    spAnimationState_update(state.state, deltaSeconds);
    spAnimationState_apply(state.state, state.renderer->skeleton);
    spSkeleton_updateWorldTransform(state.renderer->skeleton);
}

static void renderSpine(SpineCocosApp &state) {
    if (!state.renderer || g_spineShader == 0) return;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glUseProgram(g_spineShader);
    glUniformMatrix4fv(g_uProjection, 1, GL_FALSE, g_projection);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_uTexture, 0);
    syncRendererTransform(state);
    applyRendererState(state);
    state.renderer->draw();
}

static void ensureOrthoProjection(int fbWidth, int fbHeight) {
    const float left_ = 0.0f;
    const float right_ = (float)fbWidth;
    const float top_ = 0.0f;
    const float bottom_ = (float)fbHeight;
    const float near_ = -1.0f;
    const float far_ = 1.0f;
    g_projection[0] = 2.0f / (right_ - left_);
    g_projection[1] = 0.0f;
    g_projection[2] = 0.0f;
    g_projection[3] = 0.0f;
    g_projection[4] = 0.0f;
    g_projection[5] = 2.0f / (top_ - bottom_);
    g_projection[6] = 0.0f;
    g_projection[7] = 0.0f;
    g_projection[8] = 0.0f;
    g_projection[9] = 0.0f;
    g_projection[10] = -2.0f / (far_ - near_);
    g_projection[11] = 0.0f;
    g_projection[12] = -(right_ + left_) / (right_ - left_);
    g_projection[13] = -(top_ + bottom_) / (top_ - bottom_);
    g_projection[14] = -(far_ + near_) / (far_ - near_);
    g_projection[15] = 1.0f;
}

static spine::BlendFunc blendFuncForState(bool premultipliedAlpha) {
    spine::BlendFunc func;
    func.src = premultipliedAlpha ? GL_ONE : GL_SRC_ALPHA;
    func.dst = GL_ONE_MINUS_SRC_ALPHA;
    return func;
}

static void applyRendererState(SpineCocosApp &state) {
    if (!state.renderer) return;
    state.renderer->setOpacityModifyRGB(state.premultipliedAlpha);
    state.renderer->premultipliedAlpha = state.premultipliedAlpha;
    state.renderer->setBlendFunc(blendFuncForState(state.premultipliedAlpha));
}

static const char *kSpineVertexShader = R"(
#version 330 core
layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;
layout(location = 2) in vec2 aTexCoord;

uniform mat4 uProjection;
out vec4 vColor;
out vec2 vTexCoord;

void main() {
    vColor = aColor;
    vTexCoord = aTexCoord;
    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
}
)";

static const char *kSpineFragmentShader = R"(
#version 330 core
in vec4 vColor;
in vec2 vTexCoord;
uniform sampler2D uTexture;
out vec4 FragColor;

void main() {
    vec4 texColor = texture(uTexture, vTexCoord);
    FragColor = texColor * vColor;
}
)";

static GLuint compileShader(GLenum type, const char *source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint success = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        fprintf(stderr, "Shader compile error: %s\n", infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static bool initSpineShader() {
    GLuint vert = compileShader(GL_VERTEX_SHADER, kSpineVertexShader);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, kSpineFragmentShader);
    if (!vert || !frag) {
        if (vert) glDeleteShader(vert);
        if (frag) glDeleteShader(frag);
        return false;
    }
    g_spineShader = glCreateProgram();
    glAttachShader(g_spineShader, vert);
    glAttachShader(g_spineShader, frag);
    glLinkProgram(g_spineShader);
    GLint linked = GL_FALSE;
    glGetProgramiv(g_spineShader, GL_LINK_STATUS, &linked);
    glDeleteShader(vert);
    glDeleteShader(frag);
    if (!linked) {
        char infoLog[512];
        glGetProgramInfoLog(g_spineShader, 512, nullptr, infoLog);
        fprintf(stderr, "Shader link error: %s\n", infoLog);
        glDeleteProgram(g_spineShader);
        g_spineShader = 0;
        return false;
    }
    glUseProgram(g_spineShader);
    g_uProjection = glGetUniformLocation(g_spineShader, "uProjection");
    g_uTexture = glGetUniformLocation(g_spineShader, "uTexture");
    glUniform1i(g_uTexture, 0);
    return true;
}

static void destroySpineShader() {
    if (g_spineShader) {
        glDeleteProgram(g_spineShader);
        g_spineShader = 0;
    }
}

static void syncRendererTransform(SpineCocosApp &state) {
    if (!state.renderer) return;
    state.renderer->setScale({state.scaleX, state.scaleY});
    state.renderer->setPosition({state.posX, state.posY});
}

static const char *selectGLSLVersion() {
#if defined(IMGUI_IMPL_OPENGL_ES2)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    return "#version 100";
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    return "#version 300 es";
#elif defined(__APPLE__)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    return "#version 150";
#else
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    return "#version 130";
#endif
}

static GLFWwindow *createMainWindow() {
    GLFWwindow *window = glfwCreateWindow((int)(width * contentScale), (int)(height * contentScale), "SpineViewer", nullptr, nullptr);
    if (!window) return nullptr;
    glfwMakeContextCurrent(window);
    glfwSetDropCallback(window, drop_callback);
    glfwSetFramebufferSizeCallback(window, resizeViewport);
    return window;
}

static bool isWindowIconified(GLFWwindow *window) {
    return glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
}

static void configureImGuiStyle(float dpiScale) {
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(dpiScale);
    style.FontScaleDpi = dpiScale;
}

static void setupImGuiContext(GLFWwindow *window, const char *glsl_version) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    configureImGuiStyle(contentScale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);
}

static void shutdownImGuiContext() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

static void renderControlPanel(ImVec4 &clear_color, ImGuiIO &io, SpineCocosApp &state) {
    static bool scaleTogether = true;

    ImGui::Begin("SpineViewer");
    ImGui::Text("Runtime Version: %s", SPINE_VERSION_STRING);
    if (ImGui::Checkbox("Premultiplied Alpha", &state.premultipliedAlpha)) {
        applyRendererState(state);
    }
    ImGui::Checkbox("Link Scale", &scaleTogether);
    if (scaleTogether) {
        if (ImGui::DragFloat("Scale", &state.scaleX, 0.01f, -5.0f, 5.0f)) {
            state.scaleY = state.scaleX;
        }
    } else {
        ImGui::DragFloat("Scale X", &state.scaleX, 0.01f, -5.0f, 5.0f);
        ImGui::DragFloat("Scale Y", &state.scaleY, 0.01f, -5.0f, 5.0f);
    }

    ImGui::DragFloat("Position X", &state.posX, 1.0f, 0.0f, (float)width);
    ImGui::DragFloat("Position Y", &state.posY, 1.0f, 0.0f, (float)height);
    ImGui::DragFloat("Time Scale", &state.playbackSpeed, 0.01f, -3.0f, 3.0f);
    ImGui::DragFloat("Asset Scale", &state.assetScale, 0.01f, 0.01f, 5.0f);

    const auto &animations = state.animationNames;
    if (ImGui::Checkbox("Loop", &state.loop) && !animations.empty()) {
        playAnimation(state, state.currentAnimation);
    }
    if (ImGui::ListBox("Animations", &state.currentAnimation,
        [](void *data, int idx, const char **out_text) {
            const auto &names = *static_cast<const std::vector<std::string> *>(data);
            if (idx < 0 || idx >= (int)names.size()) return false;
            *out_text = names[idx].c_str();
            return true;
        }, (void *)&animations, (int)animations.size(), 6)) {
        playAnimation(state, state.currentAnimation);
    }

    ImGui::ColorEdit4("Clear color", (float *)&clear_color);
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    ImGui::Text("Drag (.json+.atlas or .skel+atlas ) to load a Spine animation");
    ImGui::End();
}

static float computeDeltaTime(double &lastTime) {
    double currTime = glfwGetTime();
    float delta = static_cast<float>(currTime - lastTime);
    lastTime = currTime;
    return delta;
}

static bool extractDroppedPaths(int count, const char **paths, std::string &atlasPath, std::string &skeletonPath) {
    for (int i = 0; i < count; ++i) {
        const std::string file = paths[i];
        if (file.find(".atlas") != std::string::npos) {
            atlasPath = file;
        } else if (file.find(".json") != std::string::npos || file.find(".skel") != std::string::npos) {
            skeletonPath = file;
        }
    }

    return !atlasPath.empty() && !skeletonPath.empty();
}
