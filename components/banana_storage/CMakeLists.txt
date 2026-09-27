idf_component_register(SRCS "src/LittleFs.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_core
                       PRIV_REQUIRES joltwallet__littlefs)
