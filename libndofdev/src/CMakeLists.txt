# -*- cmake -*-

project(libndofdev)

set(libndofdev_SOURCE_FILES
    ndofdev.c
)

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(libndofdev_HEADER_FILES
        ndofdev_hidutils.h
        ndofdev_hidutils_err.h
        ndofdev_internal_osx.h
    )
    list(APPEND libndofdev_SOURCE_FILES
        ndofdev_hidutils.c
        ndofdev_hidutils_err.c
        ndofdev_osx.c
    )
    add_definitions(-DTARGET_OS_MAC)
    
    find_library(CORE_FOUNDATION_FRAMEWORK CoreFoundation)
    find_library(CORE_SERVICES_FRAMEWORK CoreServices)
    find_library(IOKIT_LIBRARY IOKit)
    set(libndofdev_LIBRARIES 
        ${IOKIT_LIBRARY}
        ${CORE_FOUNDATION_FRAMEWORK}
        ${CORE_SERVICES_FRAMEWORK}
    )
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    message(FATAL_ERROR "Windows configuration not implemented.") 
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    message(FATAL_ERROR "Linux configuration not implemented.") 
endif()

set_source_files_properties(${libndofdev_HEADER_FILES} PROPERTIES HEADER_FILE_ONLY TRUE)
list(APPEND libndofdev_SOURCE_FILES ${libndofdev_HEADER_FILES})

add_library (ndofdev SHARED ${libndofdev_SOURCE_FILES})
target_link_libraries(ndofdev
    ${libndofdev_LIBRARIES}
)
