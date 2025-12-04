idf_component_register(
    SRCS "PN5180.cpp" "PN5180ISO15693.cpp" "pn5180_wrapper.cpp"
    INCLUDE_DIRS "."
    REQUIRES driver esp_timer log freertos
)
