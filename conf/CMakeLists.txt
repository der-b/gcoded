
###
### gcoded
###
install(TARGETS gcoded
        TYPE RUNTIME)


install(DIRECTORY
        CONFIGURATIONS Release
        DESTINATION /var/lib/gcoded
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_WRITE GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
if (CMAKE_BUILD_TYPE STREQUAL "Release")

    ## set CAP_SYS_NICE capability to installed gcoded
    install(CODE
        "execute_process(COMMAND sh -c \"setcap cap_sys_nice=+pe $(which gcoded) || echo \\\"WARNING: Could not set CAP_SYS_NICE capability on\\\" $<TARGET_FILE:gcoded> 1>&2\"
                             WORKING_DIRECTORY \"${PROJECT_BUILD_DIR}\")")


    ## create gcoded user
    install(CODE "execute_process(COMMAND useradd --no-create-home --system gcoded)")

    ## set the correct user to the local state of the service
    install(CODE "execute_process(COMMAND sh -c \"chown gcoded:gcoded /var/lib/gcoded || true\")")
endif()


###
### gcoded
###
install(TARGETS gcode
        TYPE RUNTIME)

###
### gcoded.conf
###
install(FILES "gcoded.conf"
        CONFIGURATIONS Debug
        DESTINATION etc)

install(FILES "gcoded.conf"
        CONFIGURATIONS Release
        DESTINATION /etc)

###
### gcoded.service
###
configure_file(gcoded.service.in ${CMAKE_BINARY_DIR}/gcoded.service)

install(FILES ${CMAKE_BINARY_DIR}/gcoded.service
        CONFIGURATIONS Release
        DESTINATION /lib/systemd/system)

install(CODE "execute_process(COMMAND systemd deamon-reaload)")
