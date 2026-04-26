/* VOXEL ENGINE: Systems Graveyard — ZKAEDI PRIME Upgrade */

#ifndef VOXEL_ENGINE_H
#define VOXEL_ENGINE_H

#include <math.h>
#include <stdlib.h>
#include "models.h"

/* 64x32x64 Systems Graveyard */
#define CHUNK_SIZE_X 64
#define CHUNK_SIZE_Y 32
#define CHUNK_SIZE_Z 64

typedef struct {
    unsigned type : 12;
    unsigned metadata : 4;
} Voxel;

enum VoxelType {
    VOXEL_AIR         = 0,
    VOXEL_OBSIDIAN    = 1,   /* pitch black, high reflectivity */
    VOXEL_GUNMETAL    = 2,   /* dark gray, matte industrial base */
    VOXEL_CYAN_CONDUIT = 3,  /* emissive cyan pathways */
    VOXEL_MAGENTA_CRYSTAL = 4, /* emissive magenta deep clusters */
};

/* ------------------------------------------------------------------ noise */
static inline float noise3d(int x, int y, int z) {
    int n = x + y * 57 + z * 131;
    n = (n << 13) ^ n;
    return (1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f);
}

static float perlin_noise(float x, float y, float z) {
    int xi = (int)floor(x); int yi = (int)floor(y); int zi = (int)floor(z);
    float xf = x - (float)xi; float yf = y - (float)yi; float zf = z - (float)zi;
    float u = xf*xf*(3.0f-2.0f*xf);
    float v = yf*yf*(3.0f-2.0f*yf);
    float w = zf*zf*(3.0f-2.0f*zf);
    float c000=noise3d(xi,yi,zi);     float c100=noise3d(xi+1,yi,zi);
    float c010=noise3d(xi,yi+1,zi);   float c110=noise3d(xi+1,yi+1,zi);
    float c001=noise3d(xi,yi,zi+1);   float c101=noise3d(xi+1,yi,zi+1);
    float c011=noise3d(xi,yi+1,zi+1); float c111=noise3d(xi+1,yi+1,zi+1);
    float x00=c000*(1.0f-u)+c100*u; float x10=c010*(1.0f-u)+c110*u;
    float x01=c001*(1.0f-u)+c101*u; float x11=c011*(1.0f-u)+c111*u;
    float y0=x00*(1.0f-v)+x10*v;    float y1=x01*(1.0f-v)+x11*v;
    return y0*(1.0f-w)+y1*w;
}

/* ------------------------------------------------------------------ face emit */
static void add_face(Triangle *tris, int *count,
                     double x, double y, double z, int face_dir,
                     double r, double g, double b,
                     double refl, double shin, double emission) {
    Vec3 p0,p1,p2,p3;
    double hs=0.5, cx=x, cy=y, cz=z;
    if      (face_dir==0){make_vec3(cx-hs,cy+hs,cz-hs,&p0);make_vec3(cx+hs,cy+hs,cz-hs,&p1);make_vec3(cx+hs,cy+hs,cz+hs,&p2);make_vec3(cx-hs,cy+hs,cz+hs,&p3);}
    else if (face_dir==1){make_vec3(cx-hs,cy-hs,cz-hs,&p0);make_vec3(cx+hs,cy-hs,cz-hs,&p1);make_vec3(cx+hs,cy-hs,cz+hs,&p2);make_vec3(cx-hs,cy-hs,cz+hs,&p3);}
    else if (face_dir==2){make_vec3(cx-hs,cy-hs,cz+hs,&p0);make_vec3(cx+hs,cy-hs,cz+hs,&p1);make_vec3(cx+hs,cy+hs,cz+hs,&p2);make_vec3(cx-hs,cy+hs,cz+hs,&p3);}
    else if (face_dir==3){make_vec3(cx-hs,cy-hs,cz-hs,&p0);make_vec3(cx+hs,cy-hs,cz-hs,&p1);make_vec3(cx+hs,cy+hs,cz-hs,&p2);make_vec3(cx-hs,cy+hs,cz-hs,&p3);}
    else if (face_dir==4){make_vec3(cx+hs,cy-hs,cz-hs,&p0);make_vec3(cx+hs,cy-hs,cz+hs,&p1);make_vec3(cx+hs,cy+hs,cz+hs,&p2);make_vec3(cx+hs,cy+hs,cz-hs,&p3);}
    else                 {make_vec3(cx-hs,cy-hs,cz-hs,&p0);make_vec3(cx-hs,cy-hs,cz+hs,&p1);make_vec3(cx-hs,cy+hs,cz+hs,&p2);make_vec3(cx-hs,cy+hs,cz-hs,&p3);}

    Vec3 color; make_vec3(r,g,b,&color);
    tris[*count].v0=p0; tris[*count].v1=p1; tris[*count].v2=p2;
    tris[*count].color=color; tris[*count].reflectivity=refl;
    tris[*count].shininess=shin; tris[*count].emission=emission; (*count)++;
    tris[*count].v0=p0; tris[*count].v1=p2; tris[*count].v2=p3;
    tris[*count].color=color; tris[*count].reflectivity=refl;
    tris[*count].shininess=shin; tris[*count].emission=emission; (*count)++;
}

/* ------------------------------------------------------------------ synthesize */
static Triangle* synthesize_voxel_geometry(int *out_tri_count) {
    int total = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;
    Voxel *voxels = (Voxel *)malloc(total * sizeof(Voxel));

    /* --- Heightmap: sheer plateaus and deep trenches --- */
    for (int x=0; x<CHUNK_SIZE_X; x++) {
        for (int z=0; z<CHUNK_SIZE_Z; z++) {
            /* Coarse noise for macro topology */
            float macro = perlin_noise(x*0.05f, 0, z*0.05f) * 10.0f;
            /* Clamp sharply to create sheer cliff edges */
            float clamped = (float)((int)(macro / 3.0f)) * 3.0f;
            int h = (int)(CHUNK_SIZE_Y / 3 + clamped);
            if (h < 2) h = 2;
            if (h > CHUNK_SIZE_Y - 2) h = CHUNK_SIZE_Y - 2;

            /* Cyan conduit paths: thin horizontal veins along Z axis */
            int is_conduit_col = (perlin_noise(x*0.3f, 5.0f, z*0.3f) > 0.55f) && (x % 4 == 0);

            for (int y=0; y<CHUNK_SIZE_Y; y++) {
                int idx = (y*CHUNK_SIZE_Z*CHUNK_SIZE_X)+(z*CHUNK_SIZE_X)+x;
                unsigned type = VOXEL_AIR;
                if (y <= h) {
                    /* Base layer: obsidian at top surface */
                    if (y == h)       type = VOXEL_OBSIDIAN;
                    else if (y > h-3) type = VOXEL_GUNMETAL;
                    else              type = VOXEL_GUNMETAL;

                    /* Cyan conduit veins run horizontally near surface */
                    if (is_conduit_col && y == h && y > 2) type = VOXEL_CYAN_CONDUIT;

                    /* Magenta crystals: second-octave perlin, depth-gated at y < h*0.4 */
                    if (y < (int)(h * 0.4f)) {
                        if (perlin_noise(x*0.7f, y*0.7f, z*0.7f) > 0.60f)
                            type = VOXEL_MAGENTA_CRYSTAL;
                    }
                }
                voxels[idx].type = type;
            }
        }
    }

    /* Allocate max possible triangles */
    Triangle *tris = (Triangle *)malloc(total * 12 * sizeof(Triangle));
    int tcount = 0;

    for (int y=0; y<CHUNK_SIZE_Y; y++) {
        for (int z=0; z<CHUNK_SIZE_Z; z++) {
            for (int x=0; x<CHUNK_SIZE_X; x++) {
                int idx = (y*CHUNK_SIZE_Z*CHUNK_SIZE_X)+(z*CHUNK_SIZE_X)+x;
                if (voxels[idx].type == VOXEL_AIR) continue;
                unsigned type = voxels[idx].type;

                /* Material parameters per spec */
                double r=0,g=0,b=0,refl=0,shin=0,emission=0;
                if (type==VOXEL_OBSIDIAN)       { r=0.04; g=0.04; b=0.06; refl=0.85; shin=180.0; emission=0.0; }
                if (type==VOXEL_GUNMETAL)        { r=0.12; g=0.13; b=0.15; refl=0.05; shin=20.0;  emission=0.0; }
                if (type==VOXEL_CYAN_CONDUIT)    { r=0.0;  g=1.0;  b=1.0;  refl=0.0;  shin=0.0;   emission=2.0; }
                if (type==VOXEL_MAGENTA_CRYSTAL) { r=1.0;  g=0.0;  b=1.0;  refl=0.0;  shin=0.0;   emission=1.6; }

                /* Neighbor-check face culling */
                int top  = (y==CHUNK_SIZE_Y-1)||(voxels[((y+1)*CHUNK_SIZE_Z*CHUNK_SIZE_X)+(z*CHUNK_SIZE_X)+x].type==VOXEL_AIR);
                int bot  = (y==0)             ||(voxels[((y-1)*CHUNK_SIZE_Z*CHUNK_SIZE_X)+(z*CHUNK_SIZE_X)+x].type==VOXEL_AIR);
                int fwd  = (z==CHUNK_SIZE_Z-1)||(voxels[(y*CHUNK_SIZE_Z*CHUNK_SIZE_X)+((z+1)*CHUNK_SIZE_X)+x].type==VOXEL_AIR);
                int back = (z==0)             ||(voxels[(y*CHUNK_SIZE_Z*CHUNK_SIZE_X)+((z-1)*CHUNK_SIZE_X)+x].type==VOXEL_AIR);
                int rt   = (x==CHUNK_SIZE_X-1)||(voxels[(y*CHUNK_SIZE_Z*CHUNK_SIZE_X)+(z*CHUNK_SIZE_X)+(x+1)].type==VOXEL_AIR);
                int lt   = (x==0)             ||(voxels[(y*CHUNK_SIZE_Z*CHUNK_SIZE_X)+(z*CHUNK_SIZE_X)+(x-1)].type==VOXEL_AIR);

                /* World-space position: scale 2.5 units/voxel, centered, shifted below origin */
                double wx = (x - CHUNK_SIZE_X/2) * 2.5;
                double wy = (y - CHUNK_SIZE_Y/2) * 2.5 - 12.0;
                double wz = (z - CHUNK_SIZE_Z/2) * 2.5;

                if(top)  add_face(tris,&tcount,wx,wy,wz,0, r,g*1.1,b, refl,shin,emission);
                if(bot)  add_face(tris,&tcount,wx,wy,wz,1, r*0.4,g*0.4,b*0.4, refl,shin,emission);
                if(fwd)  add_face(tris,&tcount,wx,wy,wz,2, r*0.7,g*0.7,b*0.7, refl,shin,emission);
                if(back) add_face(tris,&tcount,wx,wy,wz,3, r*0.7,g*0.7,b*0.7, refl,shin,emission);
                if(rt)   add_face(tris,&tcount,wx,wy,wz,4, r*0.85,g*0.85,b*0.85, refl,shin,emission);
                if(lt)   add_face(tris,&tcount,wx,wy,wz,5, r*0.85,g*0.85,b*0.85, refl,shin,emission);
            }
        }
    }

    free(voxels);
    *out_tri_count = tcount;
    return tris;
}

#endif
