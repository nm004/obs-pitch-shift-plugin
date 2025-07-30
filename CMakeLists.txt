cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

set(AUDACITY_ROOT ${PROJECT_SOURCE_DIR}/audacity/au3)

add_library(pffft
	STATIC
	${AUDACITY_ROOT}/lib-src/pffft/pffft.c)

target_include_directories(pffft
	PUBLIC
	${AUDACITY_ROOT}/lib-src/pffft)

set_property(TARGET pffft PROPERTY POSITION_INDEPENDENT_CODE ON)

add_library(audacity-time-and-pitch
	STATIC
	${AUDACITY_ROOT}/libraries/lib-time-and-pitch/FormantShifter.cpp
	${AUDACITY_ROOT}/libraries/lib-time-and-pitch/StaffPad/FourierTransform_pffft.cpp
	${AUDACITY_ROOT}/libraries/lib-time-and-pitch/StaffPad/TimeAndPitch.cpp)

target_include_directories(audacity-time-and-pitch
	PUBLIC
	${AUDACITY_ROOT}/libraries/lib-time-and-pitch
	${AUDACITY_ROOT}/libraries/lib-utility)

target_link_libraries(audacity-time-and-pitch PRIVATE pffft)
set_property(TARGET audacity-time-and-pitch PROPERTY POSITION_INDEPENDENT_CODE ON)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.cc)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE audacity-time-and-pitch)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
