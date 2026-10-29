cmake_minimum_required(VERSION 3.16)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

project(BurningRubber VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
    pkg_check_modules(SDL2_TTF IMPORTED_TARGET SDL2_ttf)
endif()
if(NOT SDL2_FOUND)
    find_package(SDL2 REQUIRED)
endif()
if(NOT SDL2_TTF_FOUND)
    find_package(SDL2_ttf REQUIRED)
endif()

set(SOURCES
    src/main.c
    src/resources.c
    src/qd.c
    src/text.c
    src/platform.c
    src/sound.c
    src/prefs.c
    src/toolbox.c
    src/tools.c
    src/shell.c
    src/powershell.c
    src/game.c
    src/gametools.c
    src/car.c
    src/objects.c
    src/animations.c
    src/display.c
    src/roaddraw.c
)

set(GAME_DATA
    ${CMAKE_SOURCE_DIR}/Race.rsrc
    ${CMAKE_SOURCE_DIR}/Levels.rsrc
)
set(GAME_FONT ${CMAKE_SOURCE_DIR}/fonts/ChicagoFLF.ttf)

if(APPLE)
    set(MACOSX_BUNDLE_BUNDLE_NAME "Burning Rubber")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.jonasechterhoff.burningrubber")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
    add_executable(BurningRubber MACOSX_BUNDLE ${SOURCES} ${GAME_DATA} ${GAME_FONT})
    set_source_files_properties(${GAME_DATA} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_source_files_properties(${GAME_FONT} PROPERTIES MACOSX_PACKAGE_LOCATION Resources/fonts)
else()
    add_executable(BurningRubber WIN32 ${SOURCES})
    add_custom_command(TARGET BurningRubber POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GAME_DATA} $<TARGET_FILE_DIR:BurningRubber>
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:BurningRubber>/fonts
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GAME_FONT} $<TARGET_FILE_DIR:BurningRubber>/fonts
    )
endif()

if(TARGET PkgConfig::SDL2)
    target_link_libraries(BurningRubber PRIVATE PkgConfig::SDL2)
else()
    if(TARGET SDL2::SDL2main)
        target_link_libraries(BurningRubber PRIVATE SDL2::SDL2main)
    endif()
    target_link_libraries(BurningRubber PRIVATE SDL2::SDL2)
endif()
if(TARGET PkgConfig::SDL2_TTF)
    target_link_libraries(BurningRubber PRIVATE PkgConfig::SDL2_TTF)
else()
    target_link_libraries(BurningRubber PRIVATE SDL2_ttf::SDL2_ttf)
endif()

if(NOT WIN32)
    target_link_libraries(BurningRubber PRIVATE m)
endif()

if(MSVC)
    target_compile_options(BurningRubber PRIVATE /W3)
    target_compile_definitions(BurningRubber PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(BurningRubber PRIVATE -Wall -Wextra)
endif()
