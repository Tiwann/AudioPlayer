cmake_minimum_required(VERSION 4.0)

project(AudioPlayer)

set(NOVA_ENGINE_BUILD_EXAMPLES OFF)
set(NOVA_ENGINE_CUSTOM_ENTRY_POINT OFF)
set(NOVA_ENGINE_INCLUDE_AUDIO ON)
set(NOVA_ENGINE_INCLUDE_PHYSICS ON)
add_subdirectory(Vendor/NovaEngine)
add_subdirectory(AudioPlayer)