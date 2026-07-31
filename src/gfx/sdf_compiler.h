/*
 * ZCC SIMD SDF Shader Compiler
 * Header File: src/gfx/sdf_compiler.h
 * Target: AVX2 8-wide float32 SIMD Raymarching Distance Evaluator
 */

#ifndef ZCC_SDF_COMPILER_H
#define ZCC_SDF_COMPILER_H

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

typedef struct {
    float x, y, z;
} Vec3;

/* Primitive Distance Functions */
float sdf_sphere(Vec3 p, float r);
float sdf_box(Vec3 p, Vec3 b);
float sdf_torus(Vec3 p, float r1, float r2);
float sdf_smooth_union(float d1, float d2, float k);

/* AVX2 8-wide SIMD Evaluator */
void zcc_sdf_eval_simd_avx2(const float *px, const float *py, const float *pz, float *out_dist, int count);

/* Shader Compiler & Mesh Sculpting Driver Entry Points */
int zcc_compile_sdf_shader_simd(const char *out_shader_path);
void zcc_sculpt_sdf(const char *output_file, int resolution);
void zcc_sculpt(const char *prompt, const char *output_file);

#endif /* ZCC_SDF_COMPILER_H */

