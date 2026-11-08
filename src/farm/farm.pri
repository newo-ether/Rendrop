# HTTP API, peer discovery and remote workers. Depends on core and config.

SOURCES += \
    $$PWD/http_server.cpp \
    $$PWD/load_monitor.cpp \
    $$PWD/node_agent.cpp \
    $$PWD/node_discovery.cpp \
    $$PWD/remote_worker.cpp

HEADERS += \
    $$PWD/http_server.h \
    $$PWD/load_monitor.h \
    $$PWD/node_agent.h \
    $$PWD/node_discovery.h \
    $$PWD/remote_worker.h \
    $$PWD/worker_auth.h
