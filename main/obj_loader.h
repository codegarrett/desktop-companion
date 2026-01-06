/*
 * OBJ File Format Loader
 * Supports loading .obj files from SPIFFS/LittleFS or embedded data
 */

#ifndef OBJ_LOADER_H
#define OBJ_LOADER_H

#include "render3d.h"

/**
 * Load a mesh from OBJ file data in memory
 * @param obj_data Null-terminated string containing OBJ file contents
 * @param default_color Default color for faces without materials
 * @return Loaded mesh or NULL on failure
 */
mesh_t* obj_load_from_string(const char *obj_data, color_t default_color);

/**
 * Load a mesh from OBJ file on filesystem
 * @param filepath Path to .obj file
 * @param default_color Default color for faces
 * @return Loaded mesh or NULL on failure
 */
mesh_t* obj_load_from_file(const char *filepath, color_t default_color);

/**
 * Get loading error message
 */
const char* obj_get_error(void);

#endif // OBJ_LOADER_H

