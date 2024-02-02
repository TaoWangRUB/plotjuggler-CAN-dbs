cmake_minimum_required(VERSION 3.7)

project(DataStreamCAN)

file(GLOB_RECURSE SRC_FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(${PROJECT_NAME} SHARED
    ${SRC_FILES}
)
    
target_link_libraries(${PROJECT_NAME}
    ${LIBRARIES}
    CanFrameProcessor
    Qt5::SerialBus
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin  )

