idf_component_register(
    SRCS "webserver_tool.c" "dns_server.c"
    INCLUDE_DIRS "include"
    REQUIRES freertos log esp_http_server json lwip
)