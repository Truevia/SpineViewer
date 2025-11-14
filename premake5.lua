
workspace "SpineViewerProject"
    configurations { "Debug", "Release" }
    location "build"

spine_versions = { "3.8", "4.2" }
for k,version in ipairs(spine_versions) do

    project_name = "SpineViewer" .. version
    print(project_name)
    project (project_name)
        kind "ConsoleApp"
        -- kind "WindowedApp"

        language "C++"
        cppdialect "C++11"
        files { 
            "src/*.cpp", "src/*.h",
        }
        includedirs { "src" }


        -- [[
        -- spine 3.8
        local spineVersion = version
        includedirs {
            "vendor/spine-cpp"..spineVersion.."/spine-cpp/include",
            "vendor/spine-cpp"..spineVersion.."/spine-cpp/src",
            "vendor/spine-cpp"..spineVersion,
        }
        files {
            "vendor/spine-cpp"..spineVersion.."/spine-cpp/src/spine/*.cpp",
            "vendor/spine-cpp"..spineVersion.."/spine-cpp/src/spine/*.h",
            "vendor/spine-cpp"..spineVersion.."/*.cpp",
            "vendor/spine-cpp"..spineVersion.."/*.h",
        }
        --]]

        -- imgui
        files {
            "vendor/imgui/*.cpp", "vendor/imgui/*.h",
            "vendor/imgui/backends/*.cpp", "vendor/imgui/backends/*.h",
        }

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

end
