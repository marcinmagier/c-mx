
add_lib_cflags(-fPIC)


add_lib_sources("buffer.c")
add_lib_sources("log.c")
add_lib_sources("memory.c")
add_lib_sources("net.c")
add_lib_sources("rand.c")
add_lib_sources("repeater.c")
add_lib_sources("socket.c")
add_lib_sources("string.c")
add_lib_sources("url.c")
add_lib_sources("timer.c")

add_lib_sources("base64.c")
add_lib_sources("sha1.c")

add_lib_sources("idler.c")
add_lib_sources("observer.c")
add_lib_sources("stream.c")

add_lib_sources("ssl.c")
add_lib_sources("stream_ssl.c")

add_lib_sources("websocket.c")
add_lib_sources("stream_ws.c")

add_lib_sources("mqtt.c")
add_lib_sources("stream_mqtt.c")

add_lib_sources("http.c")

