# Embed the server root certificate into the final binary
#
# (If this was a component, we would set COMPONENT_EMBED_TXTFILES here.)
idf_component_register(SRCS main.c src/network_handler.c
                    INCLUDE_DIRS "inc"
                    EMBED_TXTFILES telegram_certificate.pem)