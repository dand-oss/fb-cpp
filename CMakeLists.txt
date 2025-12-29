cmake_minimum_required(VERSION 3.18)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

set(CMAKE_MODULE_PATH ${CMAKE_BINARY_DIR} ${CMAKE_MODULE_PATH})
set(CMAKE_PREFIX_PATH ${CMAKE_BINARY_DIR} ${CMAKE_PREFIX_PATH})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/out/bin)

project(fb-cpp-main
	VERSION 0.0.2
	DESCRIPTION "C++ client library for Firebird"
	HOMEPAGE_URL "https://github.com/asfernandes/fb-cpp"
	LANGUAGES CXX
)

# API selection option
option(FB_CPP_FIREBIRD_LEGACY "Use Firebird 2.5 legacy C API instead of 3.0+ OO API" OFF)

###############
# ASI default settings
list(APPEND CMAKE_MODULE_PATH "$ENV{ASV_CMAKE}")
include(asv_cmake_defaults)
asv_cmake_defaults( )
###############

# Find Firebird based on API choice
if(FB_CPP_FIREBIRD_LEGACY)
	find_package(Firebird2 CONFIG REQUIRED PATHS ${ASV_PLAT_PORTS}/firebird2 NO_DEFAULT_PATH)
	set(FIREBIRD_TARGET Firebird2::fbclient)
	get_target_property(FIREBIRD_INCLUDE_DIR ${FIREBIRD_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
	get_target_property(FIREBIRD_LIBRARY ${FIREBIRD_TARGET} IMPORTED_LOCATION_RELEASE)
	if(NOT FIREBIRD_LIBRARY)
		get_target_property(FIREBIRD_LIBRARY ${FIREBIRD_TARGET} IMPORTED_LOCATION_DEBUG)
	endif()
else()
	find_package(Firebird5 CONFIG REQUIRED PATHS ${ASV_PLAT_PORTS}/firebird5 NO_DEFAULT_PATH)
	set(FIREBIRD_TARGET Firebird5::fbclient)
	get_target_property(FIREBIRD_INCLUDE_DIR ${FIREBIRD_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
	get_target_property(FIREBIRD_LIBRARY ${FIREBIRD_TARGET} IMPORTED_LOCATION_RELEASE)
	if(NOT FIREBIRD_LIBRARY)
		get_target_property(FIREBIRD_LIBRARY ${FIREBIRD_TARGET} IMPORTED_LOCATION_DEBUG)
	endif()
endif()

# Find Boost with ASV ports path
find_package(Boost CONFIG REQUIRED
    PATHS ${ASV_PLAT_PORTS}/boost NO_DEFAULT_PATH
    COMPONENTS
        headers
)

# Debug output
asv_print_package_properties(
    Boost::headers
    ${FIREBIRD_TARGET}
)

# Boost.DLL is header-only, included in Boost::headers
# Set to OFF - we link fbclient directly, no dynamic loading needed
set(FB_CPP_USE_BOOST_DLL OFF)

enable_language(CXX)

# Disable testing - requires boost_unit_test_framework
set(BUILD_TESTING OFF)
include(CTest)

if(MSVC)
	add_compile_options(
		/W4
		/permissive-
		/w44062			# enumerator 'identifier' in switch of enum 'enumeration' is not handled
		/w44265			# 'class': class has virtual functions, but destructor is not virtual
		# disable
		/wd4100			# 'identifier': unreferenced parameter
	)
	add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
	add_compile_options(
		-Wall
		-Wextra
		-Wpedantic
		-Wconversion
		-Wno-unused-parameter
	)
endif()

add_subdirectory(src/fb-cpp)
add_subdirectory(doc)

if(BUILD_TESTING)
	add_subdirectory(src/test)
endif()


set(FB_CPP_USE_BOOST_DLL_VALUE 0)
if(FB_CPP_USE_BOOST_DLL)
	set(FB_CPP_USE_BOOST_DLL_VALUE 1)
endif()

set(FB_CPP_USE_BOOST_MULTIPRECISION_VALUE 0)
if(FB_CPP_USE_BOOST_MULTIPRECISION)
	set(FB_CPP_USE_BOOST_MULTIPRECISION_VALUE 1)
endif()

set(FB_CPP_FIREBIRD_LEGACY_VALUE 0)
if(FB_CPP_FIREBIRD_LEGACY)
	set(FB_CPP_FIREBIRD_LEGACY_VALUE 1)
endif()


include(CMakePackageConfigHelpers)
write_basic_package_version_file(
	"${CMAKE_CURRENT_BINARY_DIR}/fb-cppConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/fb-cppConfig.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/fb-cppConfig.cmake"
	INSTALL_DESTINATION cmake/fb-cpp
)

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/fb-cppConfig.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/fb-cppConfigVersion.cmake"
	DESTINATION cmake/fb-cpp
)
