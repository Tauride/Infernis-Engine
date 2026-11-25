# SDL3, used only by the editors (window, input, ImGui platform backend). Fetched at configure time from the
# official release archive, pinned by hash; the same release IX-Ray builds its editors with.
# To build offline, pass -DFETCHCONTENT_SOURCE_DIR_SDL3=<path to an extracted SDL release-3.2.22>.
include(FetchContent)

FetchContent_Declare(SDL3
    URL https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.2.22.tar.gz
    URL_HASH SHA256=911C2D5C3D22EFC29AF9A2CEF6CDFACBA30D965242C383D997C93078E6474B01
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

set(SDL_SHARED ON CACHE BOOL "" FORCE)
set(SDL_STATIC OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL_REVISION "3.2.22" CACHE STRING "" FORCE)

FetchContent_MakeAvailable(SDL3)

foreach(SDL_TARGET SDL3-shared SDL_uclibc)
    if (TARGET ${SDL_TARGET})
        set_target_properties(${SDL_TARGET} PROPERTIES FOLDER editors/3rd-party)
    endif()
endforeach()
