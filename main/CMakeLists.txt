idf_component_register(
    SRCS "main.c" "test_acquisition.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES acc_driver log
)
