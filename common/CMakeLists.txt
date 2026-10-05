find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(im_common STATIC
    include/common/messagetype.h
    include/common/packet.h
    include/common/packetcodec.h
    src/packetcodec.cpp
    include/common/messagestatus.h
    include/common/filemeta.h
)

target_include_directories(im_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(im_common PUBLIC
    Qt6::Core
)