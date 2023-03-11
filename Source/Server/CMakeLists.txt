project(server VERSION 1.0.0 DESCRIPTION "server is a static library for Servers")

file(GLOB_RECURSE SERVER_FILES "*.cpp" "*.h")

add_library(${PROJECT_NAME} ${SERVER_FILES})
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_compile_definitions(${PROJECT_NAME} PUBLIC NOMINMAX _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(${PROJECT_NAME} PUBLIC
	engine::dependencies
)

set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER ${ROOT_FOLDER} LINKER_LANGUAGE CXX UNITY_BUILD ON)
create_vs_filters(${SERVER_FILES})