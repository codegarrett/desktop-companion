idf_component_register(SRCS "desktoy_main.c" "ssd1306.c" "sprites.c" "render3d.c" "obj_loader.c" "buzzer.c"
                       PRIV_REQUIRES driver
                       INCLUDE_DIRS ".")
