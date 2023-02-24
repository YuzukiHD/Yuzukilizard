add_library(confparser confparser.c)
target_link_libraries(confparser ${SUNXIMPP_LIB} ${COMMON_LIB})