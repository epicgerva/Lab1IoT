idf_component_register(SRCS "http.c"
                    INCLUDE_DIRS "include"
                    EMBED_FILES "index.html" "script.js" "style.css"
                    REQUIRES esp_http_server)
