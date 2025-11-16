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
#define GL_SILENCE_DEPRECATION
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

#include "spine-glfw.h"
#include "spine/spine.h"
#include "spine/Version.h"
#include "SpineManager.h"

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

int width = 800, height = 600;
renderer_t *g_renderer = nullptr;
SpineManager *g_spineManager = nullptr;

static void framebuffer_size_callback(GLFWwindow* window, int new_width, int new_height) {
	width = new_width;
	height = new_height;
	if (g_renderer) {
		renderer_set_viewport_size(g_renderer, width, height);
	}
	if (g_spineManager) {
		g_spineManager->repositionSkeleton();
	}
	glViewport(0, 0, width, height);
}


static void drop_callback(GLFWwindow* window, int count, const char** paths) {
	std::string atlasPath, skelPath;
	
	// First pass: collect file paths
	for (int i = 0; i < count; i++) {
		std::cout << "Dropped file: " << paths[i] << std::endl;
		std::string filepath(paths[i]);
		
		if (filepath.find(".atlas") != std::string::npos) {
			atlasPath = filepath;
		} else if (filepath.find(".skel") != std::string::npos || 
				   filepath.find(".json") != std::string::npos) {
			skelPath = filepath;
		}
	}
	
	// Try to load if we have both atlas and skeleton files
	if (!atlasPath.empty() && !skelPath.empty() && g_spineManager) {
		std::cout << "Loading Spine animation..." << std::endl;
		std::cout << "Atlas: " << atlasPath << std::endl;
		std::cout << "Skeleton: " << skelPath << std::endl;
		
		if (g_spineManager->loadSpine(atlasPath, skelPath)) {
			std::cout << "Spine animation loaded successfully!" << std::endl;
		} else {
			std::cout << "Failed to load Spine animation!" << std::endl;
		}
	} else if (!atlasPath.empty() || !skelPath.empty()) {
		std::cout << "Please drop both .atlas and .skel/.json files to load a complete Spine animation" << std::endl;
	}
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Main code
int main(int, char**)
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE); // must come before glfwCreateWindow
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    GLFWwindow* window = glfwCreateWindow((int)(width * main_scale), (int)(height * main_scale), "SpineViewer", nullptr, nullptr);
    if (window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSetDropCallback(window, drop_callback);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    glfwSwapInterval(1); // Enable vsync
    if (!gladLoadGL()) {
        printf("Failed to load GL\n");
        return -1;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (using io.ConfigDpiScaleFonts=true makes this unnecessary. We leave both here for documentation purpose)

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details. If you like the default font but want it to scale better, consider using the 'ProggyVector' from the same author!
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    //style.FontSizeBase = 20.0f;
    //io.Fonts->AddFontDefault();
    //io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf");
    //io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf");
    //ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf");
    //IM_ASSERT(font != nullptr);

    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    spine::Bone::setYDown(true);
	// Create the renderer and set the viewport size to match the window size. This sets up a
	// pixel perfect orthogonal projection for 2D rendering.
	renderer_t *renderer = renderer_create();
	g_renderer = renderer;
	renderer_set_viewport_size(renderer, width * main_scale, height * main_scale);

	// Create the spine manager and load default animation
	g_spineManager = new SpineManager();
    
    
//	g_spineManager->loadSpine("data/spineboy-pma.atlas", "data/spineboy-pro.skel");
//    g_spineManager->loadSpine("./data37/spineboy.atlas", "./data37/spineboy-pro.json");


    double lastTime = glfwGetTime();
    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while (!glfwWindowShouldClose(window))
#endif
    {
        // Calculate the delta time in seconds
        double currTime = glfwGetTime();
        float delta = currTime - lastTime;
        lastTime = currTime;

        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
        {
            ImGui_ImplGlfw_Sleep(10);
            continue;
        }

		// Update spine animation
		if (g_spineManager) {
			g_spineManager->update(delta);
		}

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();


        // 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            static bool scalexy = true;

            ImGui::Begin("SpineViewer");                          // Create a window called "Hello, world!" and append into it.

            ImGui::Text("Runtime Version: %s", SPINE_VERSION_STRING);               // Display Spine version            
            ImGui::Checkbox("premultipliedAlpha", &g_spineManager->premultipliedAlpha);      // Edit bools storing our window open/close state

            ImGui::Text("Draw calls: %u", g_spineManager->drawcall);
            ImGui::Checkbox("Both", &scalexy);
            if (scalexy)
            {
                if (ImGui::DragFloat("scale", &g_spineManager->scalex, 0.01f, -5.0, 5.0f))
                {
                    g_spineManager->setScaleX(g_spineManager->scalex);
                    g_spineManager->setScaleY(g_spineManager->scalex);
                }
            }
            else
            {
                if (ImGui::DragFloat("scaleX", &g_spineManager->scalex, 0.01f, -5.0, 5.0f))
                {
                    if (scalexy)
                        g_spineManager->setScaleY(g_spineManager->scaley);
                    g_spineManager->setScaleX(g_spineManager->scalex);
                }
                
                if (ImGui::DragFloat("scaleY", &g_spineManager->scaley, 0.01f, -5.0, 5.0f))
                {
                    if (scalexy)
                        g_spineManager->setScaleX(g_spineManager->scalex);
                    g_spineManager->setScaleY(g_spineManager->scaley);
                }
            }

            // animation selection
            const auto &animationNames = g_spineManager->animationNames;
            if (ImGui::Checkbox("Loop", &g_spineManager->spineLoop))
            {
                if (animationNames.size() > 0)
                {
                    g_spineManager->setAnimationByName(animationNames[g_spineManager->currentAnimation], g_spineManager->spineLoop);
                }
            }
            if (ImGui::ListBox("##Spine##Animations", &g_spineManager->currentAnimation, [](void* data, int idx, const char** out_text) {
                const auto& names = *(const std::vector<std::string>*)data;
                if (idx < 0 || idx >= names.size()) return false;
                *out_text = names[idx].c_str();
                return true;
            }, (void*)&animationNames, animationNames.size(), 5))
            {
                g_spineManager->setAnimationByName(animationNames[g_spineManager->currentAnimation], g_spineManager->spineLoop);
            }

            if (ImGui::DragFloat("X", &g_spineManager->spinePosX, 1.0f, 0.0f, (float)width))
            {
                g_spineManager->setX(g_spineManager->spinePosX);
            }
            if (ImGui::DragFloat("Y", &g_spineManager->spinePosY, 1.0f, 0.0f, (float)height))
            {
                g_spineManager->setY(g_spineManager->spinePosY);
            }

            if (ImGui::DragFloat("TimeScale", &g_spineManager->spineEntryTimeScale, 0.01f, -3.0f, 3.0f))
            {
                g_spineManager->setTimeScale(g_spineManager->spineEntryTimeScale);
            }
            ImGui::ColorEdit4("clear color", (float*)&clear_color); // Edit 3 floats representing a color
            ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::Text("Drag (.json+.atlas or .skel+atlas ) to load a Spine animation");
            ImGui::End();
        }

        // int display_w, display_h;
        // glfwGetFramebufferSize(window, &display_w, &display_h);
        // glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        
        // Render the skeleton in its current pose
		if (g_spineManager) {
			g_spineManager->render();
		}

        // Rendering
        ImGui::Render();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());



        glfwSwapBuffers(window);
    }
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
