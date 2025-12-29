set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(Boost_NO_WARN_NEW_VERSIONS 1)

project(fb-cpp-test CXX)

file(GLOB_RECURSE SRC
	"*.h"
	"*.cpp"
)

add_executable(${PROJECT_NAME}
	${SRC}
)

# Find Boost with ASV ports path
find_package(Boost CONFIG REQUIRED
    PATHS ${ASV_PLAT_PORTS}/boost NO_DEFAULT_PATH
    COMPONENTS
		filesystem
		unit_test_framework
)

asv_print_package_properties(
    Boost::filesystem
    Boost::unit_test_framework
)

target_include_directories(${PROJECT_NAME}
	PUBLIC ${Boost_INCLUDE_DIR}
)

target_link_libraries(${PROJECT_NAME}
	PRIVATE fb-cpp
	PRIVATE Firebird2::fbclient
	PRIVATE Boost::filesystem
	PRIVATE Boost::unit_test_framework
)

add_test(
	NAME ${PROJECT_NAME}
	COMMAND $<TARGET_FILE:${PROJECT_NAME}>
)

set_tests_properties(${PROJECT_NAME}
	PROPERTIES
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		ENVIRONMENT "FB_CPP_BUILD_DIR=${CMAKE_BINARY_DIR};FB_CPP_BUILD_CONFIG=$<CONFIG>"
)
