# Keep the embedded baseline as default. The PSRAM candidate failed extended
# audio acceptance on 2026-10-23; external modes remain diagnostic options only.
set(SPRING_BACKGROUND_STORAGE "embedded" CACHE STRING "Background storage: embedded, flash, psram")
set_property(CACHE SPRING_BACKGROUND_STORAGE PROPERTY STRINGS embedded flash psram)
set(SPRING_STORAGE_DEFS "")
if(SPRING_BACKGROUND_STORAGE STREQUAL "embedded")
    set(SPRING_STORAGE_DEFS SPRING_BACKGROUND_EMBEDDED=1)
elseif(SPRING_BACKGROUND_STORAGE STREQUAL "psram")
    set(SPRING_STORAGE_DEFS SPRING_BACKGROUND_PSRAM=1)
elseif(NOT SPRING_BACKGROUND_STORAGE STREQUAL "flash")
    message(FATAL_ERROR "Unknown SPRING_BACKGROUND_STORAGE=${SPRING_BACKGROUND_STORAGE}")
endif()
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/spring_art_data.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/boards/${BOARD_DIR}/spring_external_art.cc"
    PROPERTIES COMPILE_DEFINITIONS "${SPRING_STORAGE_DEFS}")
if(SPRING_BACKGROUND_STORAGE STREQUAL "embedded")
    return()
endif()
# Keep experimental external builds/flashes paired with the background.
get_filename_component(SPRING_GAME_ROOT "${PROJECT_DIR}/../.." ABSOLUTE)
set(SPRING_PACKER "${SPRING_GAME_ROOT}/tools/pack-spring-background.py")
set(SPRING_BACKGROUND "${SPRING_GAME_ROOT}/assets/art-v1/device/Background.bin")
if(NOT EXISTS "${SPRING_PACKER}" OR NOT EXISTS "${SPRING_BACKGROUND}")
    message(FATAL_ERROR "Spring background packer/pixels missing; use the complete game checkout")
endif()
set(SPRING_PAIRED_ASSETS "${CMAKE_BINARY_DIR}/spring_assets.bin")
add_custom_command(
    OUTPUT "${SPRING_PAIRED_ASSETS}"
    COMMAND "${PYTHON}" "${SPRING_PACKER}" --base "${GENERATED_ASSETS_LOCAL_FILE}"
            --output "${SPRING_PAIRED_ASSETS}"
    DEPENDS "${GENERATED_ASSETS_LOCAL_FILE}" "${SPRING_PACKER}" "${SPRING_BACKGROUND}"
            "${PROJECT_DIR}/scripts/build_default_assets.py"
    COMMENT "Preserving XiaoZhi assets and packing the Spring background"
    VERBATIM
)
add_custom_target(spring_paired_assets ALL DEPENDS "${SPRING_PAIRED_ASSETS}")
add_dependencies(flash spring_paired_assets)
set(GENERATED_ASSETS_LOCAL_FILE "${SPRING_PAIRED_ASSETS}")
