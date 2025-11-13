
workspace "SpineViewerProject"
    configurations { "Debug", "Release" }
    location "build"

project "SpineViewer"
    kind "ConsoleApp"
    -- kind "WindowedApp"

    language "C++"
    cppdialect "C++11"
    files { 
        "src/*.cpp", "src/*.h",

        -- imgui
        "vendor/imgui/*.cpp", "vendor/imgui/*.h",
        "vendor/imgui/backends/*.cpp", "vendor/imgui/backends/*.h",
    }
    includedirs { "src" }

    -- spine
    includedirs { "vendor/spine-cpp/spine-cpp/include", "vendor/spine-cpp/spine-cpp/src" }
    files { "vendor/spine-cpp/spine-cpp/src/spine/*.cpp", "vendor/spine-cpp/spine-cpp/src/spine/*.h" }

    -- // include glfw
    includedirs { "vendor/glfw/include", "vendor/imgui" }
    filter "system:macosx"
        libdirs { "vendor/glfw/lib" }
        links { "Cocoa.framework", "OpenGL.framework", "IOKit.framework", "CoreVideo.framework", "glfw3" }
        xcodebuildsettings {
            ALWAYS_SEARCH_USER_PATHS = "YES"
        }
    defines { "GLFW_INCLUDE_NONE", "IMGUI_IMPL_OPENGL_LOADER_CUSTOM" }

    -- glad
    includedirs { "vendor/glad/include" }
    files { "vendor/glad/src/glad.c", "vendor/glad/include/glad/glad.h" }

    filter "configurations:Debug"
        defines { "DEBUG" }
        symbols "On"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
