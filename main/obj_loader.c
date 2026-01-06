/*
 * OBJ File Format Loader Implementation
 * Parses Wavefront OBJ format (vertices, faces, normals)
 */

#include "obj_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char error_msg[128] = "";

const char* obj_get_error(void) {
    return error_msg;
}

// Skip whitespace
static const char* skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p) && *p != '\n') p++;
    return p;
}

// Parse a float value
static float parse_float(const char **pp) {
    const char *p = skip_ws(*pp);
    char *end;
    float val = strtof(p, &end);
    *pp = end;
    return val;
}

// Parse an integer value
static int parse_int(const char **pp) {
    const char *p = skip_ws(*pp);
    char *end;
    int val = (int)strtol(p, &end, 10);
    *pp = end;
    return val;
}

// Parse face index (handles v, v/vt, v/vt/vn, v//vn formats)
static int parse_face_index(const char **pp) {
    int idx = parse_int(pp);
    const char *p = *pp;
    
    // Skip texture coordinate if present
    if (*p == '/') {
        p++;
        if (*p != '/') {
            // Skip texture index
            strtol(p, (char**)&p, 10);
        }
        // Skip normal index if present
        if (*p == '/') {
            p++;
            strtol(p, (char**)&p, 10);
        }
    }
    
    *pp = p;
    return idx;
}

// Count elements in OBJ data for pre-allocation
static void count_elements(const char *data, int *verts, int *faces) {
    *verts = 0;
    *faces = 0;
    
    const char *p = data;
    while (*p) {
        // Skip to start of line
        p = skip_ws(p);
        
        if (*p == 'v' && p[1] == ' ') {
            (*verts)++;
        } else if (*p == 'f' && p[1] == ' ') {
            // Count vertices in face (triangulate later)
            const char *line = p + 2;
            int face_verts = 0;
            while (*line && *line != '\n') {
                line = skip_ws(line);
                if (*line && *line != '\n') {
                    parse_face_index(&line);
                    face_verts++;
                }
            }
            // Triangulate: n-gon creates (n-2) triangles
            if (face_verts >= 3) {
                *faces += face_verts - 2;
            }
        }
        
        // Skip to next line
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
}

mesh_t* obj_load_from_string(const char *obj_data, color_t default_color) {
    if (!obj_data) {
        snprintf(error_msg, sizeof(error_msg), "Null data pointer");
        return NULL;
    }
    
    // Count elements
    int vert_count, face_count;
    count_elements(obj_data, &vert_count, &face_count);
    
    if (vert_count == 0) {
        snprintf(error_msg, sizeof(error_msg), "No vertices found");
        return NULL;
    }
    
    if (vert_count > MAX_VERTICES || face_count > MAX_FACES) {
        snprintf(error_msg, sizeof(error_msg), 
                 "Model too large: %d verts, %d faces (max %d/%d)",
                 vert_count, face_count, MAX_VERTICES, MAX_FACES);
        return NULL;
    }
    
    // Allocate mesh
    mesh_t *mesh = mesh_create(vert_count + 1, face_count + 1);
    if (!mesh) {
        snprintf(error_msg, sizeof(error_msg), "Failed to allocate mesh");
        return NULL;
    }
    
    // Temporary storage for face vertex indices during triangulation
    int face_indices[16]; // Max 16-gon
    
    // Parse OBJ data
    const char *p = obj_data;
    int vi = 0, fi = 0;
    
    while (*p) {
        p = skip_ws(p);
        
        // Vertex position
        if (*p == 'v' && p[1] == ' ') {
            p += 2;
            float x = parse_float(&p);
            float y = parse_float(&p);
            float z = parse_float(&p);
            mesh->vertices[vi++] = vec3_create(x, y, z);
        }
        // Face
        else if (*p == 'f' && p[1] == ' ') {
            p += 2;
            
            // Parse all vertex indices
            int nv = 0;
            while (*p && *p != '\n' && nv < 16) {
                p = skip_ws(p);
                if (*p && *p != '\n' && !isspace((unsigned char)*p)) {
                    int idx = parse_face_index(&p);
                    // OBJ indices are 1-based, convert to 0-based
                    // Negative indices are relative to current vertex count
                    if (idx < 0) idx = vi + idx;
                    else idx = idx - 1;
                    
                    if (idx >= 0 && idx < vi) {
                        face_indices[nv++] = idx;
                    }
                }
            }
            
            // Triangulate (fan triangulation for convex polygons)
            for (int i = 1; i < nv - 1 && fi < face_count; i++) {
                mesh->faces[fi].v[0] = face_indices[0];
                mesh->faces[fi].v[1] = face_indices[i];
                mesh->faces[fi].v[2] = face_indices[i + 1];
                mesh->faces[fi].color = default_color;
                fi++;
            }
        }
        
        // Skip to next line
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    
    mesh->vertex_count = vi;
    mesh->face_count = fi;
    
    // Calculate normals
    mesh_calculate_normals(mesh);
    
    snprintf(error_msg, sizeof(error_msg), "OK: %d verts, %d faces", vi, fi);
    return mesh;
}

mesh_t* obj_load_from_file(const char *filepath, color_t default_color) {
    // Note: This requires SPIFFS/LittleFS to be mounted
    // For embedded systems, prefer obj_load_from_string with compiled-in data
    
    FILE *f = fopen(filepath, "r");
    if (!f) {
        snprintf(error_msg, sizeof(error_msg), "Cannot open file: %s", filepath);
        return NULL;
    }
    
    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (size <= 0 || size > 64 * 1024) { // Max 64KB
        fclose(f);
        snprintf(error_msg, sizeof(error_msg), "Invalid file size: %ld", size);
        return NULL;
    }
    
    // Read file
    char *data = (char*)malloc(size + 1);
    if (!data) {
        fclose(f);
        snprintf(error_msg, sizeof(error_msg), "Out of memory");
        return NULL;
    }
    
    size_t read = fread(data, 1, size, f);
    fclose(f);
    data[read] = '\0';
    
    // Parse
    mesh_t *mesh = obj_load_from_string(data, default_color);
    free(data);
    
    return mesh;
}

