# will be implictly used for any compilation unit if not overrided by SYSTEM_INCLUDE_FOLDERS parameter
# these directories go as -isystem <dir> to avoid warnings from code of third-party libraries

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/eonio.bios/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/eonio.bios/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/eonio.msig/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/eonio.msig/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/eonio.system/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/eonio.system/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/eonio.token/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/eonio.token/)
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/eonio.wrap/ DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/eonio.wrap/)
