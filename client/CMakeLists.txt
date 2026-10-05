find_package(Qt6 REQUIRED COMPONENTS Widgets Network Sql)

qt_add_executable(QtIMClient
    main.cpp

    include/ui/mainwindow.h
    src/ui/mainwindow.cpp
    forms/mainwindow.ui

    include/ui/loginwindow.h
    src/ui/loginwindow.cpp
    forms/loginwindow.ui

    include/network/tcpclient.h
    src/network/tcpclient.cpp
    include/network/filetransfersession.h
    src/network/filetransfersession.cpp
    include/network/filesender.h
    src/network/filesender.cpp
    include/network/filereceiver.h
    src/network/filereceiver.cpp
    include/network/filetransfermanager.h
    src/network/filetransfermanager.cpp
    include/network/filereaderworker.h
    src/network/filereaderworker.cpp
)

target_include_directories(QtIMClient PRIVATE
    include
    include/ui
)

target_link_libraries(QtIMClient PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Sql
    im_common
)

set_property(TARGET QtIMClient PROPERTY
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/forms"
)