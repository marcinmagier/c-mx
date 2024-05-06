
add_lib_defines("DEBUG_STREAM=1")
add_lib_defines("DEBUG_STREAM_MQTT")
add_lib_defines("DEBUG_STREAM_WS")

add_app_sources(main.c)
add_app_sources(test_log.c)
add_app_sources(test_http.c)
add_app_sources(test_misc.c)
add_app_sources(test_stream.c)
add_app_sources(test_stream_mqtt.c)
add_app_sources(test_stream_ws.c)
add_app_sources(test_string.c)
add_app_sources(test_timer.c)

