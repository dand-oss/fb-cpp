set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(fb-cpp CXX)

option(FB_CPP_USE_BOOST_DLL "Enable Boost.DLL support for loading fbclient at runtime" ON)
option(FB_CPP_USE_BOOST_MULTIPRECISION "Enable Boost.Multiprecision helpers for INT128 and DECFLOAT types" ON)
option(FB_CPP_FIREBIRD_LEGACY "Use Firebird 2.5 legacy C API instead of 3.0+ OO API" OFF)

# Common headers
set(HEADERS
	fb-api.h
	config.h
	Descriptor.h
)

# Select implementation based on API choice
if(FB_CPP_FIREBIRD_LEGACY)
	# Firebird 2.5 Legacy C API
	set(IMPL_SOURCES
		Client_legacy.cpp
		Attachment_legacy.cpp
		Transaction_legacy.cpp
		Statement_legacy.cpp
		Exception_legacy.cpp
	)
	set(IMPL_HEADERS
		Client_legacy.h
		Attachment.h
		Transaction.h
		Statement_legacy.h
		Exception_legacy.h
	)
else()
	# Firebird 3.0+ OO API (default)
	set(IMPL_SOURCES
		Client.cpp
		Attachment.cpp
		Transaction.cpp
		Statement.cpp
		Exception.cpp
		Blob.cpp
		EventListener.cpp
	)
	set(IMPL_HEADERS
		Client.h
		Attachment.h
		Transaction.h
		Statement.h
		Exception.h
		Blob.h
		EventListener.h
		SmartPtrs.h
		NumericConverter.h
		CalendarConverter.h
		StructBinding.h
		VariantTypeTraits.h
		types.h
	)
endif()

set(_fb_cpp_boost_components "")
if(FB_CPP_USE_BOOST_DLL)
	list(APPEND _fb_cpp_boost_components dll)
endif()
if(FB_CPP_USE_BOOST_MULTIPRECISION)
	list(APPEND _fb_cpp_boost_components multiprecision)
endif()

if(_fb_cpp_boost_components)
	find_package(Boost REQUIRED
		COMPONENTS ${_fb_cpp_boost_components}
	)
endif()

# Firebird dependency
# Parent CMakeLists.txt may provide FIREBIRD_TARGET (ASV ports) or
# FIREBIRD_INCLUDE_DIR/FIREBIRD_LIBRARY (manual paths) or
# nothing (use vcpkg find_package)
if(FIREBIRD_TARGET)
	# ASV ports: parent already found the target, just use it
	set(_firebird_link_target ${FIREBIRD_TARGET})
elseif(FIREBIRD_INCLUDE_DIR AND FIREBIRD_LIBRARY)
	# Manual paths provided
	add_library(firebird INTERFACE IMPORTED)
	target_include_directories(firebird INTERFACE ${FIREBIRD_INCLUDE_DIR})
	target_link_libraries(firebird INTERFACE ${FIREBIRD_LIBRARY})
	set(_firebird_link_target firebird)
elseif(FB_CPP_FIREBIRD_LEGACY)
	message(FATAL_ERROR "FB_CPP_FIREBIRD_LEGACY requires FIREBIRD_INCLUDE_DIR and FIREBIRD_LIBRARY")
else()
	find_package(firebird CONFIG REQUIRED)
	set(_firebird_link_target firebird)
endif()

add_library(${PROJECT_NAME}
	${HEADERS}
	${IMPL_HEADERS}
	${IMPL_SOURCES}
)

target_include_directories(${PROJECT_NAME}
	PUBLIC
		$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/..>
		$<INSTALL_INTERFACE:include>
)

if(NOT DEFINED FB_CPP_USE_BOOST_DLL_VALUE)
	if(FB_CPP_USE_BOOST_DLL)
		set(FB_CPP_USE_BOOST_DLL_VALUE 1)
	else()
		set(FB_CPP_USE_BOOST_DLL_VALUE 0)
	endif()
endif()

if(NOT DEFINED FB_CPP_USE_BOOST_MULTIPRECISION_VALUE)
	if(FB_CPP_USE_BOOST_MULTIPRECISION)
		set(FB_CPP_USE_BOOST_MULTIPRECISION_VALUE 1)
	else()
		set(FB_CPP_USE_BOOST_MULTIPRECISION_VALUE 0)
	endif()
endif()

# Set compile definitions
if(FB_CPP_FIREBIRD_LEGACY)
	target_compile_definitions(${PROJECT_NAME} PUBLIC FB_CPP_LEGACY_API=1)
else()
	target_compile_definitions(${PROJECT_NAME} PUBLIC FB_CPP_LEGACY_API=0)
endif()

target_compile_definitions(${PROJECT_NAME}
	PUBLIC
		FB_CPP_USE_BOOST_DLL=${FB_CPP_USE_BOOST_DLL_VALUE}
		FB_CPP_USE_BOOST_MULTIPRECISION=${FB_CPP_USE_BOOST_MULTIPRECISION_VALUE}
)

target_link_libraries(${PROJECT_NAME}
	PUBLIC
		${_firebird_link_target}
	PRIVATE
		${CMAKE_DL_LIBS}
)

if(FB_CPP_USE_BOOST_MULTIPRECISION)
	target_link_libraries(${PROJECT_NAME}
		PRIVATE Boost::multiprecision
	)
endif()

unset(_fb_cpp_boost_components)

file(GLOB_RECURSE HEADER_FILES
	"${CMAKE_CURRENT_LIST_DIR}/*.h"
	"${CMAKE_CURRENT_LIST_DIR}/*.hpp"
)

foreach(HEADER_FILE ${HEADER_FILES})
	file(RELATIVE_PATH REL_PATH ${CMAKE_CURRENT_LIST_DIR} ${HEADER_FILE})
	get_filename_component(REL_DIR ${REL_PATH} DIRECTORY)
	if(REL_DIR)
		install(FILES ${HEADER_FILE}
			DESTINATION include/fb-cpp/${REL_DIR}
		)
	else()
		install(FILES ${HEADER_FILE}
			DESTINATION include/fb-cpp
		)
	endif()
endforeach()

install(TARGETS ${PROJECT_NAME}
	EXPORT ${PROJECT_NAME}Targets
	LIBRARY DESTINATION lib
	ARCHIVE DESTINATION lib
	RUNTIME DESTINATION bin
)

install(EXPORT ${PROJECT_NAME}Targets
	FILE ${PROJECT_NAME}Targets.cmake
	NAMESPACE ${PROJECT_NAME}::
	DESTINATION fb-cpp/cmake/${PROJECT_NAME}
)
