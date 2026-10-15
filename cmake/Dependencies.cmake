# The timed module connection uses the C API discovered by AzerothCore on Windows/Linux.
if(TARGET modules)
    target_link_libraries(modules PRIVATE mysql)
endif()
GetProjectNameOfModuleName(${SOURCE_MODULE} PRESTIGIO_MODULE_TARGET)
if(TARGET ${PRESTIGIO_MODULE_TARGET} AND NOT PRESTIGIO_MODULE_TARGET STREQUAL "modules")
    target_link_libraries(${PRESTIGIO_MODULE_TARGET} PRIVATE mysql)
endif()
