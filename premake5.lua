
workspace "SpineViewerProject"
    configurations { "Debug", "Release" }
    location "projects"

local macos_deployment_target = "11.0"

spine_versions = { 
    "2.1.25", "2.1.08",
    "4.2", 
    "3.7.94", "3.7", "3.8", '3.6.53'
}
for k,version in ipairs(spine_versions) do

    project_name = "SpineViewer" .. version
    print(project_name)
    project (project_name)
        -- kind "ConsoleApp"
        kind "WindowedApp"
        local bundle_identifier = "com.trve." .. project_name

        language "C++"
        cppdialect "C++11"
        files { 
            "src/*.cpp", "src/*.h",
        }
        includedirs { "src" }


        -- [[
        -- spine 3.8
        local spineVersion = version
        -- get spine-cpp version from spine version
        local version_parts = {}
        for part in string.gmatch(spineVersion, "[^%.]+") do
            table.insert(version_parts, part)
        end
        local version_major = tonumber(version_parts[1])
        local version_minor = tonumber(version_parts[2])
        if version_major <= 3 and  version_minor < 7 then
            includedirs {
                "vendor/spine-c"..spineVersion.."/include",
                "vendor/spine-c"..spineVersion,
            }
            files {
                "vendor/spine-c"..spineVersion.."/src/spine/*.c",
                "vendor/spine-c"..spineVersion.."/src/spine/*.h",
                "vendor/spine-c"..spineVersion.."/*.cpp",
                "vendor/spine-c"..spineVersion.."/*.h",
            }
            removefiles {
                "src/spine-glfw.cpp", "src/spine-glfw.h", 
                "src/SpineManager.cpp", "src/SpineManager.h",
                "src/main.cpp",
            }
        else
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
        end
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
                ALWAYS_SEARCH_USER_PATHS = "YES",
                GENERATE_INFOPLIST_FILE = "YES",
                PRODUCT_BUNDLE_IDENTIFIER = bundle_identifier,
                MACOSX_DEPLOYMENT_TARGET = macos_deployment_target,
            }
        filter "system:windows"
            includedirs { "vendor/glfw/win32/include" }
            libdirs { "vendor/glfw/win32/lib-vc2019" }
            links { "glfw3.lib", "opengl32.lib", "gdi32.lib", "user32.lib", "shell32.lib" }

        filter {}
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
