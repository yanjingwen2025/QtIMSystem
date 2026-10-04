find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

qt_add_executable(QtIMServer
    main.cpp

    include/network/tcpserver.h
    src/network/tcpserver.cpp

    include/database/databasemanager.h
    src/database/databasemanager.cpp
    include/network/clientsession.h
    src/network/clientsession.cpp
)

target_include_directories(QtIMServer PRIVATE
    include
)

target_link_libraries(QtIMServer PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    im_common
)

