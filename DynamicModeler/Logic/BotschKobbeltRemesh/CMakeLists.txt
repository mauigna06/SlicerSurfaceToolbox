project(BotschKobbeltRemesh)

set(SRCS
  RemeshMesh.cxx
  RemeshMesh.h
  )

add_library(${PROJECT_NAME} STATIC ${SRCS})
set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  )

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    ${VTK_LIBRARIES}
  )

set_property(GLOBAL APPEND PROPERTY Slicer_TARGETS ${PROJECT_NAME})
