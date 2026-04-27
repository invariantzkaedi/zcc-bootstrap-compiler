#ifndef MODELS_H
#define MODELS_H

struct Vec3 { double x, y, z; };
typedef struct Vec3 Vec3;
struct Triangle { Vec3 v0, v1, v2; Vec3 color; double reflectivity; double shininess; double emission; };
typedef struct Triangle Triangle;

extern Vec3 aabb_min;
extern Vec3 aabb_max;
extern int MESH_SIZE;
extern Triangle mesh_triangles[7476];

#endif
