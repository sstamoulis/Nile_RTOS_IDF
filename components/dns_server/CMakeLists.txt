idf_component_register(SRC_DIRS "."
                        INCLUDE_DIRS "include"
                        PRIV_INCLUDE_DIRS "."
                        REQUIRES "tcpip_adapter" "esp_http_server" "http_parser")