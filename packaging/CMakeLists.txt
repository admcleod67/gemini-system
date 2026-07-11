# Packaging install rules (Milestone 17 Stages 3 and 6).

configure_file(
        ${CMAKE_SOURCE_DIR}/packaging/systemd/gemini.service.in
        ${CMAKE_BINARY_DIR}/packaging/systemd/gemini.service
        @ONLY)

install(FILES ${CMAKE_BINARY_DIR}/packaging/systemd/gemini.service
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system
        COMPONENT Service)

install(FILES ${CMAKE_SOURCE_DIR}/packaging/gemini/daemon.conf
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/gemini
        COMPONENT Service)

install(FILES ${CMAKE_SOURCE_DIR}/packaging/gemini/daemon.conf.example
        DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/gemini
        COMPONENT Service)
