cmake_minimum_required (VERSION 3.10)
project (ls3render)

set (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffile-prefix-map=${CMAKE_SOURCE_DIR}=CMAKE_SOURCE_DIR")

add_subdirectory(parser)
generate_zusi_parser(zusi_parser ${CMAKE_CURRENT_BINARY_DIR}/zusi_parser
  WHITELIST
    Zusi::Landschaft

    Landschaft::lsb
    Landschaft::SubSet
    Landschaft::Verknuepfte
    Landschaft::VerknAnimation
    Landschaft::MeshAnimation
    Landschaft::Animation

    Material::C
    Material::CA
    Material::E
    Material::Ca
    Material::Cd
    Material::Ce
    Material::zBias
    Material::Textur
    Material::RenderFlags
    Material::TypLs3

    SubSet::NachtEinstellung
    SubSet::MeshI
    SubSet::MeshV
    SubSet::Face
    SubSet::Vertex

    Vertex::p
    Vertex::n

    AnimationsDeklaration::AniID
    AnimationsDeklaration::AniNrs

    AnimationsDefinition::AniNr
    AnimationsDefinition::AniIndex
    AnimationsDefinition::AniPunkt

    AniPunkt::p
    AniPunkt::q

    Textur::Datei

    Verknuepfte::Datei
    Verknuepfte::p
    Verknuepfte::phi
    Verknuepfte::sk

    Dateiverknuepfung::Dateiname

    AniPunkt::AniZeit

    AniNrs::AniNr

    Vertex::U
    Vertex::U2
    Vertex::V
    Vertex::V2

    Face::i

    RenderFlags::TexVoreinstellung

    Vec3::X
    Vec3::Y
    Vec3::Z

    Quaternion::X
    Quaternion::Y
    Quaternion::Z
    Quaternion::W

    Verknuepfte::SichtbarAb
  USE_GLM
  IGNORE_UNKNOWN)

# TODO: per-target
set (CMAKE_CXX_STANDARD 17)
if (ENABLE_SANITIZERS)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined")
endif ()

find_package(PkgConfig)
pkg_check_modules(OPENGL REQUIRED gl glew glu glfw3)
if (OPENGL_glew_VERSION VERSION_LESS 2.1.0)
  message(FATAL_ERROR "Glew >= 2.1.0 expected, got ${OPENGL_glew_VERSION}")
endif()

find_package(glm QUIET)
if (NOT ${GLM_FOUND})
  pkg_check_modules(glm REQUIRED glm)
endif()

include (GenerateExportHeader)
add_library(ls3render ls3render.cpp scene.cpp render_object.cpp shader_manager.cpp)
GENERATE_EXPORT_HEADER(ls3render
  BASE_NAME ls3render
  EXPORT_MACRO_NAME ls3render_EXPORT
  EXPORT_FILE_NAME ls3render_Export.h
  STATIC_DEFINE ls3render_BUILT_AS_STATIC
)
ADD_COMPILER_EXPORT_FLAGS(ls3render)

if (USE_BOOST_FILESYSTEM)
  find_package(Boost COMPONENTS filesystem REQUIRED)
  target_include_directories(ls3render PRIVATE ${Boost_INCLUDE_DIRS})
  target_link_libraries(ls3render PRIVATE ${Boost_LIBRARIES})
  target_compile_definitions(ls3render PRIVATE USE_BOOST_FILESYSTEM ZUSI_PARSER_USE_BOOST_FILESYSTEM)
else()
  # TODO: Bei x-compilation wird GCC-Version falsch erkannt
  if (NOT CMAKE_CROSSCOMPILING)
    target_link_libraries(ls3render PRIVATE $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.0>>:stdc++fs>)
  endif()
endif()

target_include_directories(ls3render PUBLIC ${OPENGL_INCLUDE_DIRS} ${glm_INCLUDE_DIRS})
target_link_libraries(ls3render PUBLIC ${OPENGL_LIBRARIES} ${glm_LIBRARIES})
target_link_libraries(ls3render PUBLIC zusi_parser)
target_compile_definitions(ls3render PRIVATE -Dls3render_EXPORTS)
target_compile_definitions(ls3render PUBLIC -DGLM_ENABLE_EXPERIMENTAL)
target_compile_options(ls3render PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(ls3render PROPERTIES CXX_VISIBILITY_PRESET hidden)
set_target_properties(ls3render PROPERTIES VISIBILITY_INLINES_HIDDEN ON)
install(TARGETS ls3render DESTINATION bin)
IF (MINGW)
    SET_TARGET_PROPERTIES(ls3render PROPERTIES LINK_FLAGS "-Wl,--output-def,libls3render.def")
    INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/libls3render.def DESTINATION lib)
ENDIF (MINGW)

# i686-w64-mingw32.static-strip -K _ls3render_AddFahrzeug -K _ls3render_Cleanup -K _ls3render_GetAusgabepufferGroesse -K _ls3render_GetBildbreite -K _ls3render_GetBildhoehe -K _ls3render_Init -K _ls3render_Render -K _ls3render_Reset -K _ls3render_SetPixelProMeter libls3render.dll

if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  add_executable (gldemo gldemo.cc)
  target_link_libraries(gldemo PRIVATE ls3render)
  install(TARGETS gldemo DESTINATION bin)

  add_executable (renderall renderall.cc)
  target_link_libraries(renderall PRIVATE ls3render)
  install(TARGETS renderall DESTINATION bin)
endif()
