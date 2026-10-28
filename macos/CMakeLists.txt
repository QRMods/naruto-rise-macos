cmake_minimum_required(VERSION 3.25)
project(narutorise LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES arm64 CACHE STRING "Target architecture")

get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(UPSTREAM "${ROOT}/upstream/naruto-rise-recomp")
set(APP "${UPSTREAM}/app")
set(CODEGEN "${ROOT}/build/codegen")
set(GENERATED "${CODEGEN}/generated")
find_package(rexglue 0.10.0 REQUIRED CONFIG)
add_subdirectory("${UPSTREAM}/third_party/nlohmann_json" json)

include("${GENERATED}/default/sources.cmake")
add_library(narutorise_recomp OBJECT ${GENERATED_SOURCES})
target_include_directories(narutorise_recomp PRIVATE
    "${CODEGEN}" "${GENERATED}/default" "${APP}/src")
target_link_libraries(narutorise_recomp PRIVATE rex::runtime)
target_precompile_headers(narutorise_recomp PRIVATE "${GENERATED}/default/narutorise_pch.h")
target_compile_options(narutorise_recomp PRIVATE -O2 -gline-tables-only -fno-strict-aliasing -ffp-model=strict)

add_executable(narutorise
    main.cpp platform.cpp
    "${APP}/src/intro_skip.cpp" "${APP}/src/ultrawide.cpp"
    "${APP}/src/save_thumbnail.cpp" "${APP}/src/engine_free.cpp"
    "${APP}/src/input_replay.cpp" "${APP}/src/window_title.cpp"
    "${APP}/src/mouse_camera.cpp" "${APP}/src/mods/translation_data.cpp"
    "${APP}/src/mods/translation_mod.cpp"
    "${ROOT}/upstream/naruto-ps5/ps5/app/loader_thread.cpp"
    "${ROOT}/upstream/naruto-ps5/ps5/app/save_picture.cpp")
target_include_directories(narutorise PRIVATE
    "${CODEGEN}" "${GENERATED}/default" "${APP}/src")
target_link_libraries(narutorise PRIVATE narutorise_recomp rex::runtime rex::gpu-xenos narutorise::json)
target_compile_definitions(narutorise PRIVATE NARUTORISE_VERSION="macOS-arm64-dev")
rexglue_configure_target(narutorise GPU_PLUGINS xenos)

# The PC port's native renderer: the game's D3D calls on Vulkan (MoltenVK)
# through Plume, with the shaders translated at build time (tools/build.py runs
# the shader chain into build/native-shaders). Xenos stays as the alternative.
option(NARUTORISE_NATIVE "Build the native renderer next to the Xenos plugin" ON)
if(NARUTORISE_NATIVE)
    enable_language(C OBJCXX)
    set(GPU "${APP}/src/gpu")
    target_sources(narutorise PRIVATE
        "${GPU}/host/host.cpp" "${GPU}/host/kernel_video.cpp" "${GPU}/host/kernel_threading.cpp"
        "${GPU}/host/kernel_modules.cpp" "${GPU}/host/journal.cpp" "${GPU}/host/stop.cpp"
        "${GPU}/host/engines.cpp" "${GPU}/host/graphics.cpp" "${GPU}/host/frame_completion.cpp"
        "${GPU}/host/guest_hooks.cpp" "${GPU}/host/exports.cpp" "${GPU}/host/title_loading.cpp"
        "${GPU}/backend/engine.cpp" "${GPU}/backend/failure_channel.cpp" "${GPU}/backend/pipeline_keys.cpp"
        "${GPU}/backend/pipeline_set.cpp" "${GPU}/backend/dispatcher.cpp" "${GPU}/backend/title_witness.cpp"
        "${GPU}/d3d/hooks.cpp" "${GPU}/d3d/device.cpp" "${GPU}/d3d/resources.cpp" "${GPU}/d3d/pass.cpp"
        "${GPU}/d3d/draw.cpp" "${GPU}/d3d/state.cpp" "${GPU}/d3d/present.cpp" "${GPU}/d3d/frame.cpp"
        "${GPU}/d3d/close.cpp" "${GPU}/d3d/query.cpp"
        "${GPU}/shaders/shaders.cpp" "${GPU}/shaders/package.cpp" "${GPU}/shaders/pipelines.cpp"
        "${GPU}/textures/textures.cpp" "${GPU}/textures/write_watch.cpp" "${GPU}/textures/formats.cpp"
        "${GPU}/textures/layout.cpp" "${GPU}/buffers/buffers.cpp" "${GPU}/buffers/vertex_layout.cpp"
        "${GPU}/diagnostics/frame_state.cpp" "${GPU}/diagnostics/census.cpp" "${GPU}/diagnostics/frame_trace.cpp"
        "${GPU}/diagnostics/start_evidence.cpp" "${GPU}/diagnostics/frame_capture.cpp"
        "${GPU}/diagnostics/measure.cpp" "${GPU}/diagnostics/present_proof.cpp"
        native/diagnostics_macos.cpp native/pipeline_disk_cache.cpp "${GPU}/diagnostics/motion.cpp" "${GPU}/diagnostics/shader_keys.cpp"
        "${GPU}/interpolation/tick.cpp" "${GPU}/interpolation/transforms.cpp" "${GPU}/settings.cpp"
        "${GPU}/shaders/shader_cache_data.cpp"
        "${GPU}/backend/vulkan/backend.cpp" "${GPU}/backend/vulkan/completion.cpp"
        "${GPU}/backend/vulkan/shader_contract.cpp")
    # Win32 names of the renderer on POSIX and Mach (native/include/windows.h).
    target_include_directories(narutorise PRIVATE native/include)
    target_compile_definitions(narutorise PRIVATE NARUTORISE_NATIVE_RENDERER_BOOTSTRAP=1
        NARUTORISE_NATIVE_D3D12_DISABLED=1 NARUTORISE_NATIVE_VULKAN=1 NARUTORISE_SHADER_CACHE_SPIRV=1)

    set(NARUTORISE_NATIVE_D3D12 OFF)
    set(NARUTORISE_NATIVE_VULKAN ON)
    set(NATIVE_RENDERER_PLUME_SOURCE "${ROOT}/upstream/plume" CACHE PATH "" FORCE)
    include("${UPSTREAM}/tools/native-renderer/plume-engine.cmake")
    # volk searches only dyld's default paths for the Vulkan loader; when that
    # fails, Plume takes the loader bundled with the app (main.cpp).
    file(READ "${CMAKE_BINARY_DIR}/plume-native-vulkan.cpp" plume_vulkan_text)
    set(plume_volk_call "        VkResult res = volkInitialize();\n")
    string(FIND "${plume_vulkan_text}" "${plume_volk_call}" plume_volk_at)
    if(plume_volk_at EQUAL -1)
        message(FATAL_ERROR "Plume's volkInitialize call was not found")
    endif()
    string(REPLACE "${plume_volk_call}" "${plume_volk_call}        if (res != VK_SUCCESS) {
            if (void* get_instance_proc_addr = NarutoriseVulkanGetInstanceProcAddr()) {
                volkInitializeCustom(PFN_vkGetInstanceProcAddr(get_instance_proc_addr));
                res = VK_SUCCESS;
            }
        }
" plume_vulkan_text "${plume_vulkan_text}")
    file(WRITE "${CMAKE_BINARY_DIR}/plume-native-vulkan-macos.cpp.new"
         "extern \"C\" void* NarutoriseVulkanGetInstanceProcAddr();\n${plume_vulkan_text}")
    file(COPY_FILE "${CMAKE_BINARY_DIR}/plume-native-vulkan-macos.cpp.new"
         "${CMAKE_BINARY_DIR}/plume-native-vulkan-macos.cpp" ONLY_IF_DIFFERENT)
    set_property(TARGET native_renderer_plume_vulkan PROPERTY SOURCES
                 "${CMAKE_BINARY_DIR}/plume-native-vulkan-macos.cpp"
                 "${NATIVE_RENDERER_PLUME_SOURCE}/plume_apple.mm")
    target_link_libraries(native_renderer_plume_vulkan PUBLIC "-framework Cocoa" "-framework QuartzCore"
        "-framework Metal")
    target_link_libraries(narutorise PRIVATE native_renderer_plume_vulkan)

    # The shader packages and their decoders (smol-v, Zstandard).
    set(XENOS_THIRDPARTY "${UPSTREAM}/tools/xenosrecomp/thirdparty")
    set(ZSTD_BUILD_SHARED OFF)
    set(ZSTD_BUILD_STATIC ON)
    set(ZSTD_BUILD_PROGRAMS OFF)
    set(ZSTD_BUILD_TESTS OFF)
    set(ZSTD_LEGACY_SUPPORT OFF)
    set(ZSTD_MULTITHREAD_SUPPORT OFF)
    add_subdirectory("${XENOS_THIRDPARTY}/zstd/build/cmake" shader-zstd EXCLUDE_FROM_ALL)
    add_library(narutorise_smolv STATIC "${XENOS_THIRDPARTY}/smol-v/source/smolv.cpp")
    target_include_directories(narutorise_smolv PUBLIC "${XENOS_THIRDPARTY}/smol-v/source")
    target_link_libraries(narutorise PRIVATE narutorise_smolv libzstd_static)
    set(NATIVE_SHADERS "${ROOT}/build/native-shaders")
    set_source_files_properties("${GPU}/shaders/shader_cache_data.cpp" PROPERTIES
        COMPILE_OPTIONS "-Wno-c23-extensions;--embed-dir=${NATIVE_SHADERS}"
        OBJECT_DEPENDS "${NATIVE_SHADERS}/shader_cache.bin;${NATIVE_SHADERS}/shader_cache_spirv.bin")
endif()

include("${GENERATED}/ai2c/sources.cmake")
add_library(narutorise_ai2c SHARED ${GENERATED_SOURCES} "${APP}/src/mouse_camera_script.cpp")
target_include_directories(narutorise_ai2c PRIVATE "${CODEGEN}" "${GENERATED}/ai2c" "${APP}/src")
target_link_libraries(narutorise_ai2c PRIVATE rex::runtime)
target_compile_definitions(narutorise_ai2c PRIVATE NARUTORISE_CAMERA_ORBIT=sub_882227F8)
target_precompile_headers(narutorise_ai2c PRIVATE "${GENERATED}/ai2c/narutorise_pch.h")
target_compile_options(narutorise_ai2c PRIVATE -O2 -gline-tables-only -fno-strict-aliasing -ffp-model=strict)
set_target_properties(narutorise_ai2c PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(narutorise_ai2c PROPERTIES
    INSTALL_RPATH "@loader_path" BUILD_WITH_INSTALL_RPATH ON)
rexglue_configure_module_target(narutorise_ai2c HOST narutorise)

configure_file(narutorise.toml "${CMAKE_CURRENT_BINARY_DIR}/narutorise.toml" COPYONLY)
