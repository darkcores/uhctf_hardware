# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)

idf_component_register(SRCS ${app_sources})

target_add_binary_data(${COMPONENT_TARGET} "../data/app.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "../data/auth.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "../data/maintenance.html" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "../data/bootstrap.bundle.min.js" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "../data/bootstrap.min.css" TEXT)
target_add_binary_data(${COMPONENT_TARGET} "../data/webkey.pem" TEXT)