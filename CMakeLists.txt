cmake_minimum_required(VERSION 3.18)

# API selection option - MUST be before project()
option(FB_CPP_FIREBIRD_LEGACY "Use Firebird 2.5 legacy C API instead of 3.0+ OO API" OFF)

# Set project name based on Firebird version
if(FB_CPP_FIREBIRD_LEGACY)
	set(_project_name "fb2-cpp")
else()
	set(_project_name "fb5-cpp")
endif()

project(${_project_name}
	VERSION 0.0.2
	DESCRIPTION "C++ client library for Firebird"
	HOMEPAGE_URL "https://github.com/asfernandes/fb-cpp"
	LANGUAGES CXX
)

###############
# ASV default settings
list(APPEND CMAKE_MODULE_PATH "$ENV{ASV_CMAKE}")
include(asv_cmake_defaults)
asv_cmake_defaults( )
###############

# Project-specific overrides
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

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
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/fb-cppConfig.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	INSTALL_DESTINATION lib/cmake/${PROJECT_NAME}
)

install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
	"${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
	DESTINATION lib/cmake/${PROJECT_NAME}
)
