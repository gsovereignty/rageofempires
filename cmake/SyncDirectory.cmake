if(NOT DEFINED SOURCE OR NOT IS_DIRECTORY "${SOURCE}")
    message(FATAL_ERROR "SOURCE must name an existing directory")
endif()
if(NOT DEFINED DESTINATION OR DESTINATION STREQUAL "")
    message(FATAL_ERROR "DESTINATION must name a directory")
endif()
if(NOT DEFINED DEPLOYMENT_ROOT OR DEPLOYMENT_ROOT STREQUAL "")
    message(FATAL_ERROR "DEPLOYMENT_ROOT must name the allowed build root")
endif()
cmake_path(IS_PREFIX DEPLOYMENT_ROOT "${DESTINATION}" NORMALIZE destination_allowed)
cmake_path(COMPARE "${DEPLOYMENT_ROOT}" EQUAL "${DESTINATION}" destination_is_root)
if(NOT destination_allowed OR destination_is_root)
    message(FATAL_ERROR "DESTINATION must be below DEPLOYMENT_ROOT")
endif()

file(MAKE_DIRECTORY "${DESTINATION}")

file(
    GLOB_RECURSE source_entries
    LIST_DIRECTORIES true
    RELATIVE "${SOURCE}"
    "${SOURCE}/*"
)
file(
    GLOB_RECURSE destination_entries
    LIST_DIRECTORIES true
    RELATIVE "${DESTINATION}"
    "${DESTINATION}/*"
)

# Remove children before parents. Only entries below explicit deployment root
# are considered; deployment root itself is never removed.
list(SORT destination_entries ORDER DESCENDING)
foreach(relative_path IN LISTS destination_entries)
    if(NOT EXISTS "${SOURCE}/${relative_path}")
        file(REMOVE_RECURSE "${DESTINATION}/${relative_path}")
    endif()
endforeach()

foreach(relative_path IN LISTS source_entries)
    if(IS_DIRECTORY "${SOURCE}/${relative_path}")
        file(MAKE_DIRECTORY "${DESTINATION}/${relative_path}")
    else()
        get_filename_component(relative_directory "${relative_path}" DIRECTORY)
        if(NOT relative_directory STREQUAL "")
            file(MAKE_DIRECTORY "${DESTINATION}/${relative_directory}")
        endif()
        file(
            COPY_FILE
            "${SOURCE}/${relative_path}"
            "${DESTINATION}/${relative_path}"
            ONLY_IF_DIFFERENT
        )
    endif()
endforeach()
