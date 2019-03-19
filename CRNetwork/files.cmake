set(header_include
)

# grouping
source_group("include" FILES ${header_include})

set(module_headers
)

set(source_src_main_gui
    "${PROJECT_SOURCE_DIR}/src/main_gui/main_gui.cpp"
    "${PROJECT_SOURCE_DIR}/src/main_gui/main_gui.hpp"
    "${PROJECT_SOURCE_DIR}/src/main_gui/openvr.cpp"
)

set(source_src
    "${PROJECT_SOURCE_DIR}/src/main.cpp"
    "${PROJECT_SOURCE_DIR}/src/main.h"
    "${PROJECT_SOURCE_DIR}/src/openvr_manager.cpp"
    "${PROJECT_SOURCE_DIR}/src/openvr_manager.hpp"
    "${PROJECT_SOURCE_DIR}/src/user.cpp"
    "${PROJECT_SOURCE_DIR}/src/user.h"
)

# grouping
source_group("src\\main_gui" FILES ${source_src_main_gui})
source_group("src" FILES ${source_src})

set(module_sources
    ${source_src_main_gui}
    ${source_src}
)
