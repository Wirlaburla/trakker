add_executable(trakker
    main.cpp
)
target_link_libraries(trakker PUBLIC ALSA::ALSA xmp ${CURSES_LIBRARY})