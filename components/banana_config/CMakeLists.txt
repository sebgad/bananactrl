# Config.hpp is a plain value type (used by the host-tested control layer). ConfigJson is host-tested too;
# ConfigStore adds the file access.
idf_component_register(SRCS "src/ConfigJson.cpp"
                            "src/ConfigStore.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES banana_core
                       PRIV_REQUIRES banana_storage bblanchon__arduinojson)
