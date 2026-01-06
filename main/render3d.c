/*
 * Lightweight 3D Rendering Engine Implementation
 */

#include "render3d.h"
#include "ssd1306.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

#define DEG_TO_RAD(d) ((d) * M_PI / 180.0f)

// ============================================================================
// DITHERING PATTERNS
// ============================================================================

// Bayer 4x4 ordered dithering matrix (values 0-15)
const uint8_t DITHER_BAYER4[4][4] = {
    {  0,  8,  2, 10 },
    { 12,  4, 14,  6 },
    {  3, 11,  1,  9 },
    { 15,  7, 13,  5 }
};

bool dither_pixel(int x, int y, float brightness) {
    if (brightness <= 0.0f) return false;
    if (brightness >= 1.0f) return true;
    
    int threshold = DITHER_BAYER4[y & 3][x & 3];
    return (brightness * 16.0f) > threshold;
}

// Simple Bresenham line drawing for wireframe mode
void render3d_draw_line(int x0, int y0, int x1, int y1, bool on) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (1) {
        if (x0 >= 0 && x0 < SSD1306_WIDTH && y0 >= 0 && y0 < SSD1306_HEIGHT) {
            ssd1306_set_pixel(x0, y0, on);
        }
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// ============================================================================
// VECTOR OPERATIONS
// ============================================================================

vec3_t vec3_create(float x, float y, float z) {
    return (vec3_t){x, y, z};
}

vec3_t vec3_add(vec3_t a, vec3_t b) {
    return (vec3_t){a.x + b.x, a.y + b.y, a.z + b.z};
}

vec3_t vec3_sub(vec3_t a, vec3_t b) {
    return (vec3_t){a.x - b.x, a.y - b.y, a.z - b.z};
}

vec3_t vec3_mul(vec3_t v, float s) {
    return (vec3_t){v.x * s, v.y * s, v.z * s};
}

vec3_t vec3_cross(vec3_t a, vec3_t b) {
    return (vec3_t){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float vec3_dot(vec3_t a, vec3_t b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float vec3_length(vec3_t v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

vec3_t vec3_normalize(vec3_t v) {
    float len = vec3_length(v);
    if (len < 0.0001f) return (vec3_t){0, 0, 0};
    return vec3_mul(v, 1.0f / len);
}

// ============================================================================
// MATRIX OPERATIONS
// ============================================================================

mat4_t mat4_identity(void) {
    mat4_t m = {{{0}}};
    m.m[0][0] = m.m[1][1] = m.m[2][2] = m.m[3][3] = 1.0f;
    return m;
}

mat4_t mat4_multiply(mat4_t a, mat4_t b) {
    mat4_t result = {{{0}}};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        }
    }
    return result;
}

mat4_t mat4_translate(float x, float y, float z) {
    mat4_t m = mat4_identity();
    m.m[0][3] = x;
    m.m[1][3] = y;
    m.m[2][3] = z;
    return m;
}

mat4_t mat4_rotate_x(float angle_deg) {
    float rad = DEG_TO_RAD(angle_deg);
    float c = cosf(rad), s = sinf(rad);
    mat4_t m = mat4_identity();
    m.m[1][1] = c;  m.m[1][2] = -s;
    m.m[2][1] = s;  m.m[2][2] = c;
    return m;
}

mat4_t mat4_rotate_y(float angle_deg) {
    float rad = DEG_TO_RAD(angle_deg);
    float c = cosf(rad), s = sinf(rad);
    mat4_t m = mat4_identity();
    m.m[0][0] = c;  m.m[0][2] = s;
    m.m[2][0] = -s; m.m[2][2] = c;
    return m;
}

mat4_t mat4_rotate_z(float angle_deg) {
    float rad = DEG_TO_RAD(angle_deg);
    float c = cosf(rad), s = sinf(rad);
    mat4_t m = mat4_identity();
    m.m[0][0] = c;  m.m[0][1] = -s;
    m.m[1][0] = s;  m.m[1][1] = c;
    return m;
}

mat4_t mat4_scale(float x, float y, float z) {
    mat4_t m = mat4_identity();
    m.m[0][0] = x;
    m.m[1][1] = y;
    m.m[2][2] = z;
    return m;
}

mat4_t mat4_perspective(float fov_deg, float aspect, float near, float far) {
    mat4_t m = {{{0}}};
    float tan_half_fov = tanf(DEG_TO_RAD(fov_deg) / 2.0f);
    
    m.m[0][0] = 1.0f / (aspect * tan_half_fov);
    m.m[1][1] = 1.0f / tan_half_fov;
    m.m[2][2] = -(far + near) / (far - near);
    m.m[2][3] = -(2.0f * far * near) / (far - near);
    m.m[3][2] = -1.0f;
    
    return m;
}

mat4_t mat4_look_at(vec3_t eye, vec3_t target, vec3_t up) {
    vec3_t f = vec3_normalize(vec3_sub(target, eye));
    vec3_t r = vec3_normalize(vec3_cross(f, up));
    vec3_t u = vec3_cross(r, f);
    
    mat4_t m = mat4_identity();
    m.m[0][0] = r.x;  m.m[0][1] = r.y;  m.m[0][2] = r.z;
    m.m[1][0] = u.x;  m.m[1][1] = u.y;  m.m[1][2] = u.z;
    m.m[2][0] = -f.x; m.m[2][1] = -f.y; m.m[2][2] = -f.z;
    m.m[0][3] = -vec3_dot(r, eye);
    m.m[1][3] = -vec3_dot(u, eye);
    m.m[2][3] = vec3_dot(f, eye);
    
    return m;
}

vec3_t mat4_transform_point(mat4_t m, vec3_t p) {
    float w = m.m[3][0] * p.x + m.m[3][1] * p.y + m.m[3][2] * p.z + m.m[3][3];
    if (fabsf(w) < 0.0001f) w = 0.0001f;
    
    return (vec3_t){
        (m.m[0][0] * p.x + m.m[0][1] * p.y + m.m[0][2] * p.z + m.m[0][3]) / w,
        (m.m[1][0] * p.x + m.m[1][1] * p.y + m.m[1][2] * p.z + m.m[1][3]) / w,
        (m.m[2][0] * p.x + m.m[2][1] * p.y + m.m[2][2] * p.z + m.m[2][3]) / w
    };
}

vec3_t mat4_transform_direction(mat4_t m, vec3_t d) {
    return (vec3_t){
        m.m[0][0] * d.x + m.m[0][1] * d.y + m.m[0][2] * d.z,
        m.m[1][0] * d.x + m.m[1][1] * d.y + m.m[1][2] * d.z,
        m.m[2][0] * d.x + m.m[2][1] * d.y + m.m[2][2] * d.z
    };
}

// ============================================================================
// COLOR UTILITIES
// ============================================================================

color_t color_create(uint8_t r, uint8_t g, uint8_t b) {
    return (color_t){r, g, b};
}

color_t color_scale(color_t c, float factor) {
    if (factor < 0) factor = 0;
    if (factor > 1) factor = 1;
    return (color_t){
        (uint8_t)(c.r * factor),
        (uint8_t)(c.g * factor),
        (uint8_t)(c.b * factor)
    };
}

uint16_t color_to_rgb565(color_t c) {
    return ((c.r >> 3) << 11) | ((c.g >> 2) << 5) | (c.b >> 3);
}

uint8_t color_to_gray(color_t c) {
    return (uint8_t)((c.r * 77 + c.g * 150 + c.b * 29) >> 8);
}

// ============================================================================
// RENDERING CORE
// ============================================================================

bool render3d_init(render_ctx_t *ctx, int width, int height) {
    memset(ctx, 0, sizeof(render_ctx_t));
    ctx->width = width;
    ctx->height = height;
    
    // Allocate zbuffer
    ctx->zbuffer = (float*)malloc(width * height * sizeof(float));
    if (!ctx->zbuffer) return false;
    
#if DISPLAY_COLOR_MODE == 1
    ctx->colorbuffer = (uint16_t*)malloc(width * height * sizeof(uint16_t));
    if (!ctx->colorbuffer) {
        free(ctx->zbuffer);
        return false;
    }
#else
    // Monochrome: use the SSD1306's buffer directly (or allocate one)
    ctx->framebuffer = NULL; // Will draw directly to SSD1306
#endif
    
    // Default camera
    ctx->camera.position = vec3_create(0, 0, 5);
    ctx->camera.target = vec3_create(0, 0, 0);
    ctx->camera.up = vec3_create(0, 1, 0);
    ctx->camera.fov = 60.0f;
    ctx->camera.near_plane = 0.1f;
    ctx->camera.far_plane = 100.0f;
    
    // Default light
    ctx->light.direction = vec3_normalize(vec3_create(-0.5f, -1.0f, -0.7f));
    ctx->light.intensity = 1.0f;
    ctx->light.ambient = 0.2f;
    
    return true;
}

void render3d_free(render_ctx_t *ctx) {
    if (ctx->zbuffer) free(ctx->zbuffer);
    if (ctx->colorbuffer) free(ctx->colorbuffer);
    if (ctx->framebuffer) free(ctx->framebuffer);
    memset(ctx, 0, sizeof(render_ctx_t));
}

void render3d_clear(render_ctx_t *ctx) {
    // Clear zbuffer to very far (positive = far in our coord system)
    for (int i = 0; i < ctx->width * ctx->height; i++) {
        ctx->zbuffer[i] = 1000.0f;  // Large positive value
    }
    
#if DISPLAY_COLOR_MODE == 1
    memset(ctx->colorbuffer, 0, ctx->width * ctx->height * sizeof(uint16_t));
#else
    ssd1306_clear();
#endif
}

void render3d_set_camera(render_ctx_t *ctx, camera_t *camera) {
    ctx->camera = *camera;
    ctx->view_matrix = mat4_look_at(camera->position, camera->target, camera->up);
    float aspect = (float)ctx->width / ctx->height;
    ctx->proj_matrix = mat4_perspective(camera->fov, aspect, camera->near_plane, camera->far_plane);
}

void render3d_set_light(render_ctx_t *ctx, light_t *light) {
    ctx->light = *light;
    ctx->light.direction = vec3_normalize(light->direction);
}

// Project 3D point to screen coordinates
static vec3_t project_point(render_ctx_t *ctx, vec3_t p, mat4_t mvp) {
    vec3_t clip = mat4_transform_point(mvp, p);
    
    // Clip space to screen space
    float x = (clip.x + 1.0f) * 0.5f * ctx->width;
    float y = (1.0f - clip.y) * 0.5f * ctx->height; // Flip Y
    
    return vec3_create(x, y, clip.z);
}

// Calculate face normal
static vec3_t calculate_face_normal(vec3_t v0, vec3_t v1, vec3_t v2) {
    vec3_t edge1 = vec3_sub(v1, v0);
    vec3_t edge2 = vec3_sub(v2, v0);
    return vec3_normalize(vec3_cross(edge1, edge2));
}

// Draw horizontal line with z-buffer test
static void draw_scanline(render_ctx_t *ctx, int y, 
                          float x1, float x2, float z1, float z2,
                          float brightness, color_t base_color) {
    if (y < 0 || y >= ctx->height) return;
    
    // Ensure x1 <= x2
    if (x1 > x2) {
        float tf = x1; x1 = x2; x2 = tf;
        tf = z1; z1 = z2; z2 = tf;
    }
    
    int ix1 = (int)x1;
    int ix2 = (int)x2;
    
    // Clip to screen
    if (ix2 < 0 || ix1 >= ctx->width) return;
    
    // Interpolation setup
    float dx = x2 - x1;
    float dz = (dx > 0.001f) ? (z2 - z1) / dx : 0.0f;
    
    // Adjust z for clipping
    float z = z1;
    if (ix1 < 0) {
        z += dz * (0.0f - x1);
        ix1 = 0;
    }
    if (ix2 >= ctx->width) {
        ix2 = ctx->width - 1;
    }
    
    for (int x = ix1; x <= ix2; x++) {
        int idx = y * ctx->width + x;
        
        // Z-buffer test (smaller z = closer)
        if (z < ctx->zbuffer[idx]) {
            ctx->zbuffer[idx] = z;
            
#if DISPLAY_COLOR_MODE == 1
            color_t shaded = color_scale(base_color, brightness);
            ctx->colorbuffer[idx] = color_to_rgb565(shaded);
#else
    #if SHADING_MODE == 1
            bool pixel = dither_pixel(x, y, brightness);
    #else
            bool pixel = brightness > 0.5f;
    #endif
            ssd1306_set_pixel(x, y, pixel);
#endif
        }
        z += dz;
    }
}

// Draw a filled triangle with flat shading using barycentric approach
static void draw_triangle_flat(render_ctx_t *ctx, 
                                vec3_t p0, vec3_t p1, vec3_t p2, 
                                float brightness, color_t base_color) {
    // Sort vertices by Y coordinate (p0.y <= p1.y <= p2.y)
    if (p0.y > p1.y) { vec3_t t = p0; p0 = p1; p1 = t; }
    if (p0.y > p2.y) { vec3_t t = p0; p0 = p2; p2 = t; }
    if (p1.y > p2.y) { vec3_t t = p1; p1 = p2; p2 = t; }
    
    // Early rejection
    int iy0 = (int)ceilf(p0.y);
    int iy2 = (int)floorf(p2.y);
    
    if (iy2 < 0 || iy0 >= ctx->height) return;
    if (iy0 > iy2) return; // Degenerate
    
    // Clamp to screen
    if (iy0 < 0) iy0 = 0;
    if (iy2 >= ctx->height) iy2 = ctx->height - 1;
    
    // Calculate edge slopes
    float dy_total = p2.y - p0.y;
    float dy_upper = p1.y - p0.y;
    float dy_lower = p2.y - p1.y;
    
    // Avoid division by zero
    float inv_dy_total = (dy_total > 0.001f) ? 1.0f / dy_total : 0.0f;
    float inv_dy_upper = (dy_upper > 0.001f) ? 1.0f / dy_upper : 0.0f;
    float inv_dy_lower = (dy_lower > 0.001f) ? 1.0f / dy_lower : 0.0f;
    
    // Rasterize scanlines
    for (int y = iy0; y <= iy2; y++) {
        float fy = (float)y + 0.5f; // Sample at pixel center
        
        // Long edge (p0 -> p2) - always active
        float t_long = (fy - p0.y) * inv_dy_total;
        float x_long = p0.x + (p2.x - p0.x) * t_long;
        float z_long = p0.z + (p2.z - p0.z) * t_long;
        
        // Short edge - depends on which half we're in
        float x_short, z_short;
        
        if (fy < p1.y) {
            // Upper half: p0 -> p1
            float t_short = (fy - p0.y) * inv_dy_upper;
            x_short = p0.x + (p1.x - p0.x) * t_short;
            z_short = p0.z + (p1.z - p0.z) * t_short;
        } else {
            // Lower half: p1 -> p2
            float t_short = (fy - p1.y) * inv_dy_lower;
            x_short = p1.x + (p2.x - p1.x) * t_short;
            z_short = p1.z + (p2.z - p1.z) * t_short;
        }
        
        draw_scanline(ctx, y, x_long, x_short, z_long, z_short, brightness, base_color);
    }
}

void render3d_draw_mesh(render_ctx_t *ctx, mesh_t *mesh) {
    if (!mesh || mesh->face_count == 0) return;
    
    // Build model matrix: M = T * R * S (applied to vertex in reverse: scale, rotate, translate)
    mat4_t scale_m = mat4_scale(mesh->scale.x, mesh->scale.y, mesh->scale.z);
    mat4_t rot_z = mat4_rotate_z(mesh->rotation.z);
    mat4_t rot_x = mat4_rotate_x(mesh->rotation.x);
    mat4_t rot_y = mat4_rotate_y(mesh->rotation.y);
    mat4_t trans_m = mat4_translate(mesh->position.x, mesh->position.y, mesh->position.z);
    
    // Model = Trans * RotY * RotX * RotZ * Scale
    mat4_t model = mat4_multiply(trans_m, mat4_multiply(rot_y, mat4_multiply(rot_x, mat4_multiply(rot_z, scale_m))));
    
    // Combined MVP matrix
    mat4_t mv = mat4_multiply(ctx->view_matrix, model);
    mat4_t mvp = mat4_multiply(ctx->proj_matrix, mv);
    
    // Normal matrix (for lighting)
    mat4_t normal_matrix = model; // Simplified - should be inverse transpose
    
    // Render each face
    for (int i = 0; i < mesh->face_count; i++) {
        face_t *face = &mesh->faces[i];
        
        // Get world-space vertices
        vec3_t v0 = mat4_transform_point(model, mesh->vertices[face->v[0]]);
        vec3_t v1 = mat4_transform_point(model, mesh->vertices[face->v[1]]);
        vec3_t v2 = mat4_transform_point(model, mesh->vertices[face->v[2]]);
        
        // Calculate face normal
        vec3_t normal = calculate_face_normal(v0, v1, v2);
        
        // Back-face culling
        vec3_t view_dir = vec3_normalize(vec3_sub(ctx->camera.position, v0));
        if (vec3_dot(normal, view_dir) < 0) continue;
        
        // Calculate lighting
        float diffuse = -vec3_dot(normal, ctx->light.direction);
        if (diffuse < 0) diffuse = 0;
        float brightness = ctx->light.ambient + diffuse * ctx->light.intensity;
        if (brightness > 1.0f) brightness = 1.0f;
        
        // Project to screen
        vec3_t p0 = project_point(ctx, mesh->vertices[face->v[0]], mvp);
        vec3_t p1 = project_point(ctx, mesh->vertices[face->v[1]], mvp);
        vec3_t p2 = project_point(ctx, mesh->vertices[face->v[2]], mvp);
        
        // Clip against near plane (simple check)
        if (p0.z < 0 || p1.z < 0 || p2.z < 0) continue;
        
        // Draw triangle
        draw_triangle_flat(ctx, p0, p1, p2, brightness, face->color);
    }
}

void render3d_draw_mesh_wireframe(render_ctx_t *ctx, mesh_t *mesh) {
    if (!mesh || mesh->face_count == 0) return;
    
    // Build model matrix
    mat4_t scale_m = mat4_scale(mesh->scale.x, mesh->scale.y, mesh->scale.z);
    mat4_t rot_z = mat4_rotate_z(mesh->rotation.z);
    mat4_t rot_x = mat4_rotate_x(mesh->rotation.x);
    mat4_t rot_y = mat4_rotate_y(mesh->rotation.y);
    mat4_t trans_m = mat4_translate(mesh->position.x, mesh->position.y, mesh->position.z);
    mat4_t model = mat4_multiply(trans_m, mat4_multiply(rot_y, mat4_multiply(rot_x, mat4_multiply(rot_z, scale_m))));
    
    mat4_t mv = mat4_multiply(ctx->view_matrix, model);
    mat4_t mvp = mat4_multiply(ctx->proj_matrix, mv);
    
    // Render each face as wireframe
    for (int i = 0; i < mesh->face_count; i++) {
        face_t *face = &mesh->faces[i];
        
        // Project vertices
        vec3_t p0 = project_point(ctx, mesh->vertices[face->v[0]], mvp);
        vec3_t p1 = project_point(ctx, mesh->vertices[face->v[1]], mvp);
        vec3_t p2 = project_point(ctx, mesh->vertices[face->v[2]], mvp);
        
        // Skip if behind camera
        if (p0.z < 0 || p1.z < 0 || p2.z < 0) continue;
        
        // Draw edges
        render3d_draw_line((int)p0.x, (int)p0.y, (int)p1.x, (int)p1.y, true);
        render3d_draw_line((int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, true);
        render3d_draw_line((int)p2.x, (int)p2.y, (int)p0.x, (int)p0.y, true);
    }
}

void render3d_present(render_ctx_t *ctx) {
#if DISPLAY_COLOR_MODE == 1
    // For color displays, copy buffer to display here
    // (depends on your color display driver)
#else
    ssd1306_update();
#endif
}

// ============================================================================
// MESH OPERATIONS
// ============================================================================

mesh_t* mesh_create(uint16_t max_verts, uint16_t max_faces) {
    mesh_t *mesh = (mesh_t*)malloc(sizeof(mesh_t));
    if (!mesh) return NULL;
    
    memset(mesh, 0, sizeof(mesh_t));
    
    mesh->vertices = (vec3_t*)malloc(max_verts * sizeof(vec3_t));
    mesh->normals = (vec3_t*)malloc(max_verts * sizeof(vec3_t));
    mesh->faces = (face_t*)malloc(max_faces * sizeof(face_t));
    
    if (!mesh->vertices || !mesh->normals || !mesh->faces) {
        mesh_free(mesh);
        return NULL;
    }
    
    mesh->scale = vec3_create(1, 1, 1);
    
    return mesh;
}

void mesh_free(mesh_t *mesh) {
    if (!mesh) return;
    if (mesh->vertices) free(mesh->vertices);
    if (mesh->normals) free(mesh->normals);
    if (mesh->faces) free(mesh->faces);
    free(mesh);
}

void mesh_calculate_normals(mesh_t *mesh) {
    // Zero out normals
    for (int i = 0; i < mesh->vertex_count; i++) {
        mesh->normals[i] = vec3_create(0, 0, 0);
    }
    
    // Accumulate face normals at each vertex
    for (int i = 0; i < mesh->face_count; i++) {
        face_t *f = &mesh->faces[i];
        vec3_t normal = calculate_face_normal(
            mesh->vertices[f->v[0]],
            mesh->vertices[f->v[1]],
            mesh->vertices[f->v[2]]
        );
        mesh->normals[f->v[0]] = vec3_add(mesh->normals[f->v[0]], normal);
        mesh->normals[f->v[1]] = vec3_add(mesh->normals[f->v[1]], normal);
        mesh->normals[f->v[2]] = vec3_add(mesh->normals[f->v[2]], normal);
    }
    
    // Normalize
    for (int i = 0; i < mesh->vertex_count; i++) {
        mesh->normals[i] = vec3_normalize(mesh->normals[i]);
    }
    mesh->normal_count = mesh->vertex_count;
}

void mesh_set_position(mesh_t *mesh, float x, float y, float z) {
    mesh->position = vec3_create(x, y, z);
}

void mesh_set_rotation(mesh_t *mesh, float rx, float ry, float rz) {
    mesh->rotation = vec3_create(rx, ry, rz);
}

void mesh_set_scale(mesh_t *mesh, float sx, float sy, float sz) {
    mesh->scale = vec3_create(sx, sy, sz);
}

// ============================================================================
// BUILT-IN PRIMITIVES
// ============================================================================

mesh_t* mesh_create_cube(float size) {
    mesh_t *mesh = mesh_create(8, 12);
    if (!mesh) return NULL;
    
    float h = size / 2;
    
    // 8 vertices
    mesh->vertices[0] = vec3_create(-h, -h, -h);
    mesh->vertices[1] = vec3_create( h, -h, -h);
    mesh->vertices[2] = vec3_create( h,  h, -h);
    mesh->vertices[3] = vec3_create(-h,  h, -h);
    mesh->vertices[4] = vec3_create(-h, -h,  h);
    mesh->vertices[5] = vec3_create( h, -h,  h);
    mesh->vertices[6] = vec3_create( h,  h,  h);
    mesh->vertices[7] = vec3_create(-h,  h,  h);
    mesh->vertex_count = 8;
    
    // 12 triangles (2 per face)
    int faces[][3] = {
        {0,1,2}, {0,2,3},  // Front
        {5,4,7}, {5,7,6},  // Back
        {4,0,3}, {4,3,7},  // Left
        {1,5,6}, {1,6,2},  // Right
        {3,2,6}, {3,6,7},  // Top
        {4,5,1}, {4,1,0}   // Bottom
    };
    
    color_t colors[] = {
        {255, 200, 200}, {255, 200, 200},  // Front - light red
        {200, 200, 255}, {200, 200, 255},  // Back - light blue
        {200, 255, 200}, {200, 255, 200},  // Left - light green
        {255, 255, 200}, {255, 255, 200},  // Right - light yellow
        {255, 200, 255}, {255, 200, 255},  // Top - light magenta
        {200, 255, 255}, {200, 255, 255}   // Bottom - light cyan
    };
    
    for (int i = 0; i < 12; i++) {
        mesh->faces[i].v[0] = faces[i][0];
        mesh->faces[i].v[1] = faces[i][1];
        mesh->faces[i].v[2] = faces[i][2];
        mesh->faces[i].color = colors[i];
    }
    mesh->face_count = 12;
    
    mesh_calculate_normals(mesh);
    return mesh;
}

mesh_t* mesh_create_sphere(float radius, int segments) {
    if (segments < 4) segments = 4;
    if (segments > 16) segments = 16;
    
    int rings = segments;
    int slices = segments * 2;
    int vert_count = (rings - 1) * slices + 2;
    int face_count = (rings - 2) * slices * 2 + slices * 2;
    
    mesh_t *mesh = mesh_create(vert_count, face_count);
    if (!mesh) return NULL;
    
    // Top vertex
    mesh->vertices[0] = vec3_create(0, radius, 0);
    int vi = 1;
    
    // Middle rings
    for (int r = 1; r < rings; r++) {
        float phi = M_PI * r / rings;
        float y = radius * cosf(phi);
        float ring_r = radius * sinf(phi);
        
        for (int s = 0; s < slices; s++) {
            float theta = 2 * M_PI * s / slices;
            mesh->vertices[vi++] = vec3_create(
                ring_r * cosf(theta),
                y,
                ring_r * sinf(theta)
            );
        }
    }
    
    // Bottom vertex
    mesh->vertices[vi] = vec3_create(0, -radius, 0);
    mesh->vertex_count = vi + 1;
    
    // Faces
    int fi = 0;
    color_t sphere_color = {255, 220, 180}; // Skin tone
    
    // Top cap
    for (int s = 0; s < slices; s++) {
        mesh->faces[fi].v[0] = 0;
        mesh->faces[fi].v[1] = 1 + s;
        mesh->faces[fi].v[2] = 1 + (s + 1) % slices;
        mesh->faces[fi].color = sphere_color;
        fi++;
    }
    
    // Middle
    for (int r = 0; r < rings - 2; r++) {
        int ring_start = 1 + r * slices;
        int next_ring = 1 + (r + 1) * slices;
        
        for (int s = 0; s < slices; s++) {
            int s_next = (s + 1) % slices;
            
            mesh->faces[fi].v[0] = ring_start + s;
            mesh->faces[fi].v[1] = next_ring + s;
            mesh->faces[fi].v[2] = next_ring + s_next;
            mesh->faces[fi].color = sphere_color;
            fi++;
            
            mesh->faces[fi].v[0] = ring_start + s;
            mesh->faces[fi].v[1] = next_ring + s_next;
            mesh->faces[fi].v[2] = ring_start + s_next;
            mesh->faces[fi].color = sphere_color;
            fi++;
        }
    }
    
    // Bottom cap
    int last_ring = 1 + (rings - 2) * slices;
    int bottom = mesh->vertex_count - 1;
    for (int s = 0; s < slices; s++) {
        mesh->faces[fi].v[0] = bottom;
        mesh->faces[fi].v[1] = last_ring + (s + 1) % slices;
        mesh->faces[fi].v[2] = last_ring + s;
        mesh->faces[fi].color = sphere_color;
        fi++;
    }
    
    mesh->face_count = fi;
    mesh_calculate_normals(mesh);
    return mesh;
}

// Create a stylized anime face (low-poly)
mesh_t* mesh_create_face(void) {
    mesh_t *mesh = mesh_create(32, 40);
    if (!mesh) return NULL;
    
    color_t skin = {255, 220, 190};
    color_t eye_white = {255, 255, 255};
    color_t eye_dark = {40, 40, 40};
    color_t mouth = {200, 100, 100};
    
    // Face outline (simplified oval)
    float face_w = 1.0f, face_h = 1.2f, face_d = 0.5f;
    
    // Vertices for face shape
    mesh->vertices[0] = vec3_create(0, face_h * 0.5f, face_d * 0.5f);       // Top
    mesh->vertices[1] = vec3_create(-face_w * 0.4f, face_h * 0.3f, face_d); // Upper left
    mesh->vertices[2] = vec3_create(face_w * 0.4f, face_h * 0.3f, face_d);  // Upper right
    mesh->vertices[3] = vec3_create(-face_w * 0.5f, 0, face_d);             // Mid left
    mesh->vertices[4] = vec3_create(face_w * 0.5f, 0, face_d);              // Mid right
    mesh->vertices[5] = vec3_create(-face_w * 0.3f, -face_h * 0.4f, face_d * 0.8f); // Lower left
    mesh->vertices[6] = vec3_create(face_w * 0.3f, -face_h * 0.4f, face_d * 0.8f);  // Lower right
    mesh->vertices[7] = vec3_create(0, -face_h * 0.5f, face_d * 0.6f);      // Chin
    mesh->vertices[8] = vec3_create(0, 0, face_d * 1.1f);                   // Nose tip
    
    // Left eye vertices
    mesh->vertices[9] = vec3_create(-0.35f, 0.15f, face_d + 0.05f);   // Eye center
    mesh->vertices[10] = vec3_create(-0.5f, 0.15f, face_d + 0.02f);   // Left
    mesh->vertices[11] = vec3_create(-0.2f, 0.15f, face_d + 0.02f);   // Right
    mesh->vertices[12] = vec3_create(-0.35f, 0.25f, face_d + 0.02f);  // Top
    mesh->vertices[13] = vec3_create(-0.35f, 0.05f, face_d + 0.02f);  // Bottom
    
    // Right eye vertices
    mesh->vertices[14] = vec3_create(0.35f, 0.15f, face_d + 0.05f);   // Eye center
    mesh->vertices[15] = vec3_create(0.2f, 0.15f, face_d + 0.02f);    // Left
    mesh->vertices[16] = vec3_create(0.5f, 0.15f, face_d + 0.02f);    // Right
    mesh->vertices[17] = vec3_create(0.35f, 0.25f, face_d + 0.02f);   // Top
    mesh->vertices[18] = vec3_create(0.35f, 0.05f, face_d + 0.02f);   // Bottom
    
    // Mouth vertices
    mesh->vertices[19] = vec3_create(-0.15f, -0.25f, face_d + 0.02f); // Left
    mesh->vertices[20] = vec3_create(0.15f, -0.25f, face_d + 0.02f);  // Right
    mesh->vertices[21] = vec3_create(0, -0.2f, face_d + 0.03f);       // Top
    mesh->vertices[22] = vec3_create(0, -0.3f, face_d + 0.02f);       // Bottom
    
    mesh->vertex_count = 23;
    
    // Face triangles
    int fi = 0;
    
    // Main face surface
    int face_tris[][3] = {
        {0, 1, 2},    // Forehead
        {1, 3, 4}, {1, 4, 2},  // Upper face
        {3, 5, 6}, {3, 6, 4},  // Mid face
        {5, 7, 6},    // Lower face
        {1, 8, 3}, {3, 8, 5},  // Left cheek
        {2, 4, 8}, {4, 6, 8},  // Right cheek
    };
    
    for (int i = 0; i < 10; i++) {
        mesh->faces[fi].v[0] = face_tris[i][0];
        mesh->faces[fi].v[1] = face_tris[i][1];
        mesh->faces[fi].v[2] = face_tris[i][2];
        mesh->faces[fi].color = skin;
        fi++;
    }
    
    // Left eye (white part)
    mesh->faces[fi].v[0] = 10; mesh->faces[fi].v[1] = 12; mesh->faces[fi].v[2] = 9;
    mesh->faces[fi].color = eye_white; fi++;
    mesh->faces[fi].v[0] = 9; mesh->faces[fi].v[1] = 12; mesh->faces[fi].v[2] = 11;
    mesh->faces[fi].color = eye_white; fi++;
    mesh->faces[fi].v[0] = 10; mesh->faces[fi].v[1] = 9; mesh->faces[fi].v[2] = 13;
    mesh->faces[fi].color = eye_white; fi++;
    mesh->faces[fi].v[0] = 9; mesh->faces[fi].v[1] = 11; mesh->faces[fi].v[2] = 13;
    mesh->faces[fi].color = eye_white; fi++;
    
    // Right eye (white part)
    mesh->faces[fi].v[0] = 15; mesh->faces[fi].v[1] = 17; mesh->faces[fi].v[2] = 14;
    mesh->faces[fi].color = eye_white; fi++;
    mesh->faces[fi].v[0] = 14; mesh->faces[fi].v[1] = 17; mesh->faces[fi].v[2] = 16;
    mesh->faces[fi].color = eye_white; fi++;
    mesh->faces[fi].v[0] = 15; mesh->faces[fi].v[1] = 14; mesh->faces[fi].v[2] = 18;
    mesh->faces[fi].color = eye_white; fi++;
    mesh->faces[fi].v[0] = 14; mesh->faces[fi].v[1] = 16; mesh->faces[fi].v[2] = 18;
    mesh->faces[fi].color = eye_white; fi++;
    
    // Mouth
    mesh->faces[fi].v[0] = 19; mesh->faces[fi].v[1] = 21; mesh->faces[fi].v[2] = 20;
    mesh->faces[fi].color = mouth; fi++;
    mesh->faces[fi].v[0] = 19; mesh->faces[fi].v[1] = 20; mesh->faces[fi].v[2] = 22;
    mesh->faces[fi].color = mouth; fi++;
    
    mesh->face_count = fi;
    mesh_calculate_normals(mesh);
    
    return mesh;
}

// Create a cute birthday cake with candle and flame
mesh_t* mesh_create_cake(float size) {
    // 8-sided cylinder for cake base + frosting top + candle + flame
    // Vertices: 8 bottom + 8 top + 8 frosting top + 1 center + 8 candle + 2 flame = 35
    // Faces: 16 sides + 16 frosting sides + 8 frosting top + 8 candle sides + 2 candle top + 4 flame = 54
    mesh_t *mesh = mesh_create(36, 56);
    if (!mesh) return NULL;
    
    float r = size * 0.5f;           // Cake radius
    float h = size * 0.35f;          // Cake height
    float frost_h = size * 0.08f;    // Frosting height
    float candle_r = size * 0.08f;   // Candle radius
    float candle_h = size * 0.35f;   // Candle height
    
    color_t cake_color = {255, 180, 140};     // Sponge cake color
    color_t frosting = {255, 200, 210};        // Pink frosting
    color_t candle_color = {255, 255, 200};    // Pale yellow candle
    color_t flame_color = {255, 200, 80};      // Orange flame
    
    int vi = 0;
    int segments = 8;
    
    // Bottom ring of cake
    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        mesh->vertices[vi++] = vec3_create(r * cosf(angle), -h/2, r * sinf(angle));
    }
    
    // Top ring of cake (before frosting)
    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        mesh->vertices[vi++] = vec3_create(r * cosf(angle), h/2, r * sinf(angle));
    }
    
    // Frosting top ring (slightly smaller, slightly higher)
    float frost_r = r * 0.92f;
    for (int i = 0; i < segments; i++) {
        float angle = 2.0f * M_PI * i / segments;
        mesh->vertices[vi++] = vec3_create(frost_r * cosf(angle), h/2 + frost_h, frost_r * sinf(angle));
    }
    
    // Frosting center top
    mesh->vertices[vi++] = vec3_create(0, h/2 + frost_h, 0);  // v24
    
    // Candle vertices (4 corners of a square prism)
    float candle_base = h/2 + frost_h;
    mesh->vertices[vi++] = vec3_create(-candle_r, candle_base, -candle_r);              // v25
    mesh->vertices[vi++] = vec3_create( candle_r, candle_base, -candle_r);              // v26
    mesh->vertices[vi++] = vec3_create( candle_r, candle_base,  candle_r);              // v27
    mesh->vertices[vi++] = vec3_create(-candle_r, candle_base,  candle_r);              // v28
    mesh->vertices[vi++] = vec3_create(-candle_r, candle_base + candle_h, -candle_r);   // v29
    mesh->vertices[vi++] = vec3_create( candle_r, candle_base + candle_h, -candle_r);   // v30
    mesh->vertices[vi++] = vec3_create( candle_r, candle_base + candle_h,  candle_r);   // v31
    mesh->vertices[vi++] = vec3_create(-candle_r, candle_base + candle_h,  candle_r);   // v32
    
    // Flame vertices (diamond shape)
    float flame_base = candle_base + candle_h;
    float flame_h = size * 0.2f;
    mesh->vertices[vi++] = vec3_create(0, flame_base, 0);                   // v33 bottom
    mesh->vertices[vi++] = vec3_create(0, flame_base + flame_h, 0);         // v34 top
    
    mesh->vertex_count = vi;
    
    int fi = 0;
    
    // Cake sides (16 triangles for 8 quads)
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        // Lower triangle
        mesh->faces[fi].v[0] = i;
        mesh->faces[fi].v[1] = next;
        mesh->faces[fi].v[2] = segments + i;
        mesh->faces[fi].color = cake_color;
        fi++;
        // Upper triangle
        mesh->faces[fi].v[0] = next;
        mesh->faces[fi].v[1] = segments + next;
        mesh->faces[fi].v[2] = segments + i;
        mesh->faces[fi].color = cake_color;
        fi++;
    }
    
    // Frosting sides (connects cake top to frosting top)
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        mesh->faces[fi].v[0] = segments + i;
        mesh->faces[fi].v[1] = segments + next;
        mesh->faces[fi].v[2] = 2*segments + i;
        mesh->faces[fi].color = frosting;
        fi++;
        mesh->faces[fi].v[0] = segments + next;
        mesh->faces[fi].v[1] = 2*segments + next;
        mesh->faces[fi].v[2] = 2*segments + i;
        mesh->faces[fi].color = frosting;
        fi++;
    }
    
    // Frosting top (fan from center)
    int frost_center = 3 * segments; // v24
    for (int i = 0; i < segments; i++) {
        int next = (i + 1) % segments;
        mesh->faces[fi].v[0] = frost_center;
        mesh->faces[fi].v[1] = 2*segments + i;
        mesh->faces[fi].v[2] = 2*segments + next;
        mesh->faces[fi].color = frosting;
        fi++;
    }
    
    // Candle sides (4 quads = 8 triangles)
    int cv = frost_center + 1; // v25
    // Front
    mesh->faces[fi].v[0] = cv+0; mesh->faces[fi].v[1] = cv+1; mesh->faces[fi].v[2] = cv+4;
    mesh->faces[fi].color = candle_color; fi++;
    mesh->faces[fi].v[0] = cv+1; mesh->faces[fi].v[1] = cv+5; mesh->faces[fi].v[2] = cv+4;
    mesh->faces[fi].color = candle_color; fi++;
    // Right
    mesh->faces[fi].v[0] = cv+1; mesh->faces[fi].v[1] = cv+2; mesh->faces[fi].v[2] = cv+5;
    mesh->faces[fi].color = candle_color; fi++;
    mesh->faces[fi].v[0] = cv+2; mesh->faces[fi].v[1] = cv+6; mesh->faces[fi].v[2] = cv+5;
    mesh->faces[fi].color = candle_color; fi++;
    // Back
    mesh->faces[fi].v[0] = cv+2; mesh->faces[fi].v[1] = cv+3; mesh->faces[fi].v[2] = cv+6;
    mesh->faces[fi].color = candle_color; fi++;
    mesh->faces[fi].v[0] = cv+3; mesh->faces[fi].v[1] = cv+7; mesh->faces[fi].v[2] = cv+6;
    mesh->faces[fi].color = candle_color; fi++;
    // Left
    mesh->faces[fi].v[0] = cv+3; mesh->faces[fi].v[1] = cv+0; mesh->faces[fi].v[2] = cv+7;
    mesh->faces[fi].color = candle_color; fi++;
    mesh->faces[fi].v[0] = cv+0; mesh->faces[fi].v[1] = cv+4; mesh->faces[fi].v[2] = cv+7;
    mesh->faces[fi].color = candle_color; fi++;
    
    // Candle top
    mesh->faces[fi].v[0] = cv+4; mesh->faces[fi].v[1] = cv+5; mesh->faces[fi].v[2] = cv+6;
    mesh->faces[fi].color = candle_color; fi++;
    mesh->faces[fi].v[0] = cv+4; mesh->faces[fi].v[1] = cv+6; mesh->faces[fi].v[2] = cv+7;
    mesh->faces[fi].color = candle_color; fi++;
    
    // Flame (4 triangles forming a diamond from corners of candle top to flame tip)
    int fv = cv + 8; // v33 flame base, v34 flame tip
    mesh->faces[fi].v[0] = cv+4; mesh->faces[fi].v[1] = cv+5; mesh->faces[fi].v[2] = fv+1;
    mesh->faces[fi].color = flame_color; fi++;
    mesh->faces[fi].v[0] = cv+5; mesh->faces[fi].v[1] = cv+6; mesh->faces[fi].v[2] = fv+1;
    mesh->faces[fi].color = flame_color; fi++;
    mesh->faces[fi].v[0] = cv+6; mesh->faces[fi].v[1] = cv+7; mesh->faces[fi].v[2] = fv+1;
    mesh->faces[fi].color = flame_color; fi++;
    mesh->faces[fi].v[0] = cv+7; mesh->faces[fi].v[1] = cv+4; mesh->faces[fi].v[2] = fv+1;
    mesh->faces[fi].color = flame_color; fi++;
    
    mesh->face_count = fi;
    mesh_calculate_normals(mesh);
    
    return mesh;
}

