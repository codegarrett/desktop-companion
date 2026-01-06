/*
 * Lightweight 3D Rendering Engine for ESP32
 * Supports monochrome (dithered) and color displays
 */

#ifndef RENDER3D_H
#define RENDER3D_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// BUILD CONFIGURATION
// ============================================================================

// Display type: 0 = Monochrome, 1 = Color (RGB565)
#ifndef DISPLAY_COLOR_MODE
#define DISPLAY_COLOR_MODE  0
#endif

// Shading mode: 0 = Flat, 1 = Dithered (mono) / Smooth (color)
#ifndef SHADING_MODE
#define SHADING_MODE  1
#endif

// Screen dimensions
#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH  128
#endif

#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT  64
#endif

// Maximum vertices/faces for a model (adjust based on RAM)
#define MAX_VERTICES  128
#define MAX_FACES     256

// ============================================================================
// 3D MATH TYPES
// ============================================================================

// 3D Vector (using floats for simplicity, could use fixed-point for speed)
typedef struct {
    float x, y, z;
} vec3_t;

// 2D Vector (screen coordinates)
typedef struct {
    float x, y;
} vec2_t;

// 4x4 Matrix for transformations
typedef struct {
    float m[4][4];
} mat4_t;

// RGB Color (0-255 per channel)
typedef struct {
    uint8_t r, g, b;
} color_t;

// Triangle face (indices into vertex array)
typedef struct {
    uint16_t v[3];      // Vertex indices
    uint16_t n[3];      // Normal indices (optional, can be same as v)
    color_t color;      // Face color (for flat shading)
} face_t;

// 3D Mesh
typedef struct {
    vec3_t *vertices;       // Vertex positions
    vec3_t *normals;        // Vertex/face normals
    face_t *faces;          // Triangle faces
    uint16_t vertex_count;
    uint16_t normal_count;
    uint16_t face_count;
    vec3_t position;        // World position
    vec3_t rotation;        // Euler rotation (degrees)
    vec3_t scale;           // Scale factors
} mesh_t;

// Camera
typedef struct {
    vec3_t position;
    vec3_t target;          // Look-at point
    vec3_t up;              // Up vector
    float fov;              // Field of view (degrees)
    float near_plane;
    float far_plane;
} camera_t;

// Light source
typedef struct {
    vec3_t direction;       // Directional light
    float intensity;        // 0.0 to 1.0
    float ambient;          // Ambient light level
} light_t;

// Render context
typedef struct {
    camera_t camera;
    light_t light;
    mat4_t view_matrix;
    mat4_t proj_matrix;
    uint8_t *framebuffer;   // For monochrome: 1-bit packed
    uint16_t *colorbuffer;  // For color: RGB565
    float *zbuffer;         // Depth buffer
    int width, height;
} render_ctx_t;

// ============================================================================
// VECTOR OPERATIONS
// ============================================================================

vec3_t vec3_create(float x, float y, float z);
vec3_t vec3_add(vec3_t a, vec3_t b);
vec3_t vec3_sub(vec3_t a, vec3_t b);
vec3_t vec3_mul(vec3_t v, float s);
vec3_t vec3_cross(vec3_t a, vec3_t b);
float vec3_dot(vec3_t a, vec3_t b);
float vec3_length(vec3_t v);
vec3_t vec3_normalize(vec3_t v);

// ============================================================================
// MATRIX OPERATIONS
// ============================================================================

mat4_t mat4_identity(void);
mat4_t mat4_multiply(mat4_t a, mat4_t b);
mat4_t mat4_translate(float x, float y, float z);
mat4_t mat4_rotate_x(float angle_deg);
mat4_t mat4_rotate_y(float angle_deg);
mat4_t mat4_rotate_z(float angle_deg);
mat4_t mat4_scale(float x, float y, float z);
mat4_t mat4_perspective(float fov_deg, float aspect, float near, float far);
mat4_t mat4_look_at(vec3_t eye, vec3_t target, vec3_t up);
vec3_t mat4_transform_point(mat4_t m, vec3_t p);
vec3_t mat4_transform_direction(mat4_t m, vec3_t d);

// Simple line drawing (Bresenham)
void render3d_draw_line(int x0, int y0, int x1, int y1, bool on);

// ============================================================================
// RENDERING API
// ============================================================================

/**
 * Initialize the 3D renderer
 * @param ctx Render context to initialize
 * @param width Screen width
 * @param height Screen height
 * @return true on success
 */
bool render3d_init(render_ctx_t *ctx, int width, int height);

/**
 * Free renderer resources
 */
void render3d_free(render_ctx_t *ctx);

/**
 * Clear the framebuffer and zbuffer
 */
void render3d_clear(render_ctx_t *ctx);

/**
 * Set up camera matrices
 */
void render3d_set_camera(render_ctx_t *ctx, camera_t *camera);

/**
 * Set light source
 */
void render3d_set_light(render_ctx_t *ctx, light_t *light);

/**
 * Render a mesh (filled with shading)
 */
void render3d_draw_mesh(render_ctx_t *ctx, mesh_t *mesh);

/**
 * Render a mesh as wireframe (for debugging)
 */
void render3d_draw_mesh_wireframe(render_ctx_t *ctx, mesh_t *mesh);

/**
 * Copy framebuffer to display (call your display's update function)
 */
void render3d_present(render_ctx_t *ctx);

// ============================================================================
// MESH OPERATIONS
// ============================================================================

/**
 * Create an empty mesh
 */
mesh_t* mesh_create(uint16_t max_verts, uint16_t max_faces);

/**
 * Free mesh memory
 */
void mesh_free(mesh_t *mesh);

/**
 * Calculate face normals for flat shading
 */
void mesh_calculate_normals(mesh_t *mesh);

/**
 * Set mesh transform
 */
void mesh_set_position(mesh_t *mesh, float x, float y, float z);
void mesh_set_rotation(mesh_t *mesh, float rx, float ry, float rz);
void mesh_set_scale(mesh_t *mesh, float sx, float sy, float sz);

// ============================================================================
// BUILT-IN PRIMITIVES
// ============================================================================

/**
 * Create a cube mesh
 */
mesh_t* mesh_create_cube(float size);

/**
 * Create a sphere mesh (low-poly)
 */
mesh_t* mesh_create_sphere(float radius, int segments);

/**
 * Create a simple face mesh (stylized anime face)
 */
mesh_t* mesh_create_face(void);

/**
 * Create a birthday cake mesh (for birthday eyes!)
 */
mesh_t* mesh_create_cake(float size);

// ============================================================================
// DITHERING PATTERNS (for monochrome displays)
// ============================================================================

// Bayer 4x4 dithering matrix
extern const uint8_t DITHER_BAYER4[4][4];

// Get dithered pixel value for brightness
bool dither_pixel(int x, int y, float brightness);

// ============================================================================
// COLOR UTILITIES
// ============================================================================

color_t color_create(uint8_t r, uint8_t g, uint8_t b);
color_t color_scale(color_t c, float factor);
uint16_t color_to_rgb565(color_t c);
uint8_t color_to_gray(color_t c);

#endif // RENDER3D_H

