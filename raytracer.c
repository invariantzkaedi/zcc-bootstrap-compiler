#include <stdio.h>
/* #include <math.h> */
#include "models.h"

double sqrt(double x);
double floor(double x);
double cos(double x);
double sin(double x);
char *getenv(const char *name);
int atoi(const char *nptr);

#define PI 3.14159265358979

/* ------------------------------------------------------------------ vec3 */
#define make_vec3(xx, yy, zz, out) do { (out)->x = (xx); (out)->y = (yy); (out)->z = (zz); } while(0)
void v_add(Vec3 *a, Vec3 *b, Vec3 *out){ make_vec3(a->x+b->x, a->y+b->y, a->z+b->z, out); }
void v_sub(Vec3 *a, Vec3 *b, Vec3 *out){ make_vec3(a->x-b->x, a->y-b->y, a->z-b->z, out); }
void v_mul(Vec3 *a, double d, Vec3 *out){ make_vec3(a->x*d, a->y*d, a->z*d, out); }
double v_dot(Vec3 *a, Vec3 *b){ return a->x*b->x + a->y*b->y + a->z*b->z; }
double v_len(Vec3 *v){ double d=v_dot(v,v); return d<=0.0?0.0:sqrt(d); }
void v_norm(Vec3 *v, Vec3 *out){
    double l=v_len(v);
    if(l>0.0) make_vec3(v->x/l, v->y/l, v->z/l, out);
    else make_vec3(0,0,1,out);
}
void v_cross(Vec3 *a, Vec3 *b, Vec3 *out){
    make_vec3(a->y*b->z - a->z*b->y,
              a->z*b->x - a->x*b->z,
              a->x*b->y - a->y*b->x, out);
}
void v_reflect(Vec3 *I, Vec3 *N, Vec3 *out){
    double d=v_dot(N,I);
    Vec3 t; v_mul(N, 2.0*d, &t); v_sub(I, &t, out);
}

/* ------------------------------------------------------ AABB intersection */
double hit_aabb_dist(Vec3 *mn, Vec3 *mx, Vec3 *ro, Vec3 *rdinv){
    double tx0=(mn->x-ro->x)*rdinv->x, tx1=(mx->x-ro->x)*rdinv->x;
    double ty0=(mn->y-ro->y)*rdinv->y, ty1=(mx->y-ro->y)*rdinv->y;
    double tz0=(mn->z-ro->z)*rdinv->z, tz1=(mx->z-ro->z)*rdinv->z;
    double tmin, tmax, a, b;
    tmin = tx0<tx1?tx0:tx1; tmax = tx0<tx1?tx1:tx0;
    a=ty0<ty1?ty0:ty1; b=ty0<ty1?ty1:ty0;
    if(a>tmin)tmin=a; if(b<tmax)tmax=b;
    a=tz0<tz1?tz0:tz1; b=tz0<tz1?tz1:tz0;
    if(a>tmin)tmin=a; if(b<tmax)tmax=b;
    if (tmax >= tmin && tmax > 0.0) return tmin > 0.0 ? tmin : 0.0;
    return 1e18;
}

/* ------------------------------------------------------------------ BVH */
typedef struct {
    Vec3 aabb_min, aabb_max;
    int left_first, tri_count;
} BVHNode;

#define MAX_BVH_NODES 250000
BVHNode bvh_nodes[MAX_BVH_NODES];
int bvh_alloc = 0;
int mesh_bvh_roots[10];

void update_node_bounds(int node_idx, Triangle *tris, int first_tri, int tri_count) {
    BVHNode *node = &bvh_nodes[node_idx];
    int i;
    make_vec3(1e30, 1e30, 1e30, &node->aabb_min);
    make_vec3(-1e30, -1e30, -1e30, &node->aabb_max);
    for (i = 0; i < tri_count; i++) {
        Triangle *t = &tris[first_tri + i];
        node->aabb_min.x = t->v0.x < node->aabb_min.x ? t->v0.x : node->aabb_min.x;
        node->aabb_min.y = t->v0.y < node->aabb_min.y ? t->v0.y : node->aabb_min.y;
        node->aabb_min.z = t->v0.z < node->aabb_min.z ? t->v0.z : node->aabb_min.z;
        node->aabb_max.x = t->v0.x > node->aabb_max.x ? t->v0.x : node->aabb_max.x;
        node->aabb_max.y = t->v0.y > node->aabb_max.y ? t->v0.y : node->aabb_max.y;
        node->aabb_max.z = t->v0.z > node->aabb_max.z ? t->v0.z : node->aabb_max.z;

        node->aabb_min.x = t->v1.x < node->aabb_min.x ? t->v1.x : node->aabb_min.x;
        node->aabb_min.y = t->v1.y < node->aabb_min.y ? t->v1.y : node->aabb_min.y;
        node->aabb_min.z = t->v1.z < node->aabb_min.z ? t->v1.z : node->aabb_min.z;
        node->aabb_max.x = t->v1.x > node->aabb_max.x ? t->v1.x : node->aabb_max.x;
        node->aabb_max.y = t->v1.y > node->aabb_max.y ? t->v1.y : node->aabb_max.y;
        node->aabb_max.z = t->v1.z > node->aabb_max.z ? t->v1.z : node->aabb_max.z;

        node->aabb_min.x = t->v2.x < node->aabb_min.x ? t->v2.x : node->aabb_min.x;
        node->aabb_min.y = t->v2.y < node->aabb_min.y ? t->v2.y : node->aabb_min.y;
        node->aabb_min.z = t->v2.z < node->aabb_min.z ? t->v2.z : node->aabb_min.z;
        node->aabb_max.x = t->v2.x > node->aabb_max.x ? t->v2.x : node->aabb_max.x;
        node->aabb_max.y = t->v2.y > node->aabb_max.y ? t->v2.y : node->aabb_max.y;
        node->aabb_max.z = t->v2.z > node->aabb_max.z ? t->v2.z : node->aabb_max.z;
    }
}

void subdivide_bvh(int node_idx, Triangle *tris, int first_tri, int tri_count) {
    BVHNode *node = &bvh_nodes[node_idx];
    Vec3 extent;
    int axis, i, j, left_count, left_idx, right_idx;
    double split_pos, center;

    update_node_bounds(node_idx, tris, first_tri, tri_count);
    if (tri_count <= 2) {
        node->left_first = first_tri;
        node->tri_count = tri_count;
        return;
    }

    v_sub(&node->aabb_max, &node->aabb_min, &extent);
    axis = 0;
    if (extent.y > extent.x) axis = 1;
    if (extent.z > extent.x && extent.z > extent.y) axis = 2;

    split_pos = ((axis == 0 ? node->aabb_min.x : (axis == 1 ? node->aabb_min.y : node->aabb_min.z)) + 
                 (axis == 0 ? node->aabb_max.x : (axis == 1 ? node->aabb_max.y : node->aabb_max.z))) * 0.5;

    i = first_tri;
    j = first_tri + tri_count - 1;
    while (i <= j) {
        center = ((axis == 0 ? tris[i].v0.x : (axis == 1 ? tris[i].v0.y : tris[i].v0.z)) +
                  (axis == 0 ? tris[i].v1.x : (axis == 1 ? tris[i].v1.y : tris[i].v1.z)) +
                  (axis == 0 ? tris[i].v2.x : (axis == 1 ? tris[i].v2.y : tris[i].v2.z))) / 3.0;
        if (center < split_pos) {
            i++;
        } else {
            Triangle temp = tris[i];
            tris[i] = tris[j];
            tris[j] = temp;
            j--;
        }
    }

    left_count = i - first_tri;
    if (left_count == 0 || left_count == tri_count) {
        node->left_first = first_tri;
        node->tri_count = tri_count;
        return;
    }

    left_idx = bvh_alloc++;
    right_idx = bvh_alloc++;
    node->left_first = left_idx;
    node->tri_count = 0;
    subdivide_bvh(left_idx, tris, first_tri, left_count);
    subdivide_bvh(right_idx, tris, i, tri_count - left_count);
}

int build_bvh(int mesh_id) {
    Mesh *m = &scene_meshes[mesh_id];
    int root_idx = bvh_alloc++;
    subdivide_bvh(root_idx, m->tris, 0, m->count);
    return root_idx;
}

/* ---------------------------------------------- Moller-Trumbore triangle */
double hit_tri(Triangle *tri, Vec3 *ro, Vec3 *rd){
    Vec3 e1,e2,h,s,q;
    double a,f,u,vv;
    v_sub(&tri->v1,&tri->v0,&e1);
    v_sub(&tri->v2,&tri->v0,&e2);
    v_cross(rd,&e2,&h);
    a=v_dot(&e1,&h);
    if(a>-1e-8&&a<1e-8) return -1.0;
    f=1.0/a;
    v_sub(ro,&tri->v0,&s);
    u=f*v_dot(&s,&h);
    if(u<0.0||u>1.0) return -1.0;
    v_cross(&s,&e1,&q);
    vv=f*v_dot(rd,&q);
    if(vv<0.0||u+vv>1.0) return -1.0;
    return f*v_dot(&e2,&q);
}

/* ---------------------------------------------- Determininistic Math */
double zcc_pow_fixed(double base, int exp) {
    double res = 1.0;
    while (exp > 0) {
        if (exp & 1) res *= base;
        base *= base;
        exp >>= 1;
    }
    return res;
}

/* ----------------------------------------------------------------- trace */
void trace(Vec3 *ro, Vec3 *rd, int depth, Vec3 *out){
    int mi, i, best_mi, best_ti, shadow;
    double best_t, dk, df, sa, si, refl, shin;
    Vec3 rdinv, td, hit, e1,e2,n, nudge, color, rl, ld, rf, fd, amb, diff_k, diff_f, spec, direct, tmp, neg_rd, vd, half, lv, rd_r, rc, t1, t2;
    Triangle *tri;
    Mesh *m;

    best_t  = 1e18;
    best_mi = -1;
    best_ti = -1;

    make_vec3(rd->x!=0?1.0/rd->x:1e18,
              rd->y!=0?1.0/rd->y:1e18,
              rd->z!=0?1.0/rd->z:1e18, &rdinv);

    for(mi=0; mi<SCENE_MESH_COUNT; mi++){
        int stack[64], stack_ptr;
        m = &scene_meshes[mi];
        if(hit_aabb_dist(&m->aabb_min, &m->aabb_max, ro, &rdinv) >= best_t) continue;
        
        stack_ptr = 0;
        stack[stack_ptr++] = mesh_bvh_roots[mi];
        
        while(stack_ptr > 0) {
            int ni = stack[--stack_ptr];
            BVHNode *node = &bvh_nodes[ni];
            if(hit_aabb_dist(&node->aabb_min, &node->aabb_max, ro, &rdinv) >= best_t) continue;
            
            if(node->tri_count > 0) {
                for(i=0; i<node->tri_count; i++){
                    int ti = node->left_first + i;
                    double t = hit_tri(&m->tris[ti], ro, rd);
                    if(t>0.001 && t<best_t){ best_t=t; best_mi=mi; best_ti=ti; }
                }
            } else {
                double t1 = hit_aabb_dist(&bvh_nodes[node->left_first].aabb_min, &bvh_nodes[node->left_first].aabb_max, ro, &rdinv);
                double t2 = hit_aabb_dist(&bvh_nodes[node->left_first+1].aabb_min, &bvh_nodes[node->left_first+1].aabb_max, ro, &rdinv);
                if(t1 > t2) {
                    if(t1 < best_t) stack[stack_ptr++] = node->left_first;
                    if(t2 < best_t) stack[stack_ptr++] = node->left_first + 1;
                } else {
                    if(t2 < best_t) stack[stack_ptr++] = node->left_first + 1;
                    if(t1 < best_t) stack[stack_ptr++] = node->left_first;
                }
            }
        }
    }

    if(best_mi==-1){
        double a = 0.5*(rd->y+1.0);
        Vec3 lo,hi,t1,t2;
        make_vec3(0.01,0.01,0.03,&lo); /* Deeper space */
        make_vec3(0.04,0.03,0.12,&hi);
        v_mul(&lo,1.0-a,&t1); v_mul(&hi,a,&t2); v_add(&t1,&t2,out);
        return;
    }

    tri = &scene_meshes[best_mi].tris[best_ti];
    v_mul(rd, best_t, &td); v_add(ro, &td, &hit);

    v_sub(&tri->v1,&tri->v0,&e1);
    v_sub(&tri->v2,&tri->v0,&e2);
    v_cross(&e1,&e2,&n); v_norm(&n,&n);
    if(v_dot(&n,rd)>0.0) v_mul(&n,-1.0,&n);
    v_mul(&n,0.001,&nudge); v_add(&hit,&nudge,&hit);

    color = tri->color;
    refl = tri->reflectivity;
    shin = tri->shininess;

    make_vec3(4.0,6.0,8.0,&rl); v_norm(&rl,&ld);
    shadow=0;
    for(mi=0; mi<SCENE_MESH_COUNT && !shadow; mi++){
        int stack[64], stack_ptr;
        Vec3 ldinv;
        m = &scene_meshes[mi];
        make_vec3(ld.x!=0?1.0/ld.x:1e18, ld.y!=0?1.0/ld.y:1e18, ld.z!=0?1.0/ld.z:1e18, &ldinv);
        if(hit_aabb_dist(&m->aabb_min,&m->aabb_max,&hit,&ldinv) > 1e17) continue;

        stack_ptr = 0;
        stack[stack_ptr++] = mesh_bvh_roots[mi];
        while(stack_ptr > 0 && !shadow) {
            int ni = stack[--stack_ptr];
            BVHNode *node = &bvh_nodes[ni];
            if(hit_aabb_dist(&node->aabb_min, &node->aabb_max, &hit, &ldinv) > 1e17) continue;
            
            if(node->tri_count > 0) {
                for(i=0; i<node->tri_count && !shadow; i++){
                    if(hit_tri(&m->tris[node->left_first + i], &hit, &ld)>0.001) shadow=1;
                }
            } else {
                stack[stack_ptr++] = node->left_first + 1;
                stack[stack_ptr++] = node->left_first;
            }
        }
    }

    make_vec3(-3.0,2.0,-1.0,&rf); v_norm(&rf,&fd);
    v_mul(&color,0.30,&amb);
    dk = v_dot(&n,&ld); if(dk<0.0)dk=0.0; if(shadow)dk=0.0;
    v_mul(&color,dk*0.80,&diff_k);
    df = v_dot(&n,&fd); if(df<0.0)df=0.0;
    v_mul(&color,df*0.45,&diff_f);

    make_vec3(0,0,0,&spec);
    if(!shadow && shin>0.0 && dk>0.0){
        v_mul(rd,-1.0,&neg_rd); v_norm(&neg_rd,&vd);
        v_add(&ld,&vd,&lv); v_norm(&lv,&half);
        sa=v_dot(&n,&half);
        if(sa>0.0){ si=zcc_pow_fixed(sa,(int)shin); v_mul(&color,si*0.5,&spec); }
    }

    v_add(&amb,&diff_k,&tmp); v_add(&tmp,&diff_f,&direct); v_add(&direct,&spec,&direct);
    if(refl>0.0 && depth<2){
        v_reflect(rd,&n,&rd_r); v_norm(&rd_r,&rd_r);
        trace(&hit,&rd_r,depth+1,&rc);
        v_mul(&direct,1.0-refl,&t1); v_mul(&rc,refl,&t2); v_add(&t1,&t2,out);
    } else {
        *out = direct;
    }
}

/* ------------------------------------------------------------------ main */
int main(){
    int width=640, height=360, i, j;
    int frame=0, total_frames=30;
    char *f_env  = getenv("FRAME");
    char *w_env  = getenv("WIDTH");
    char *h_env  = getenv("HEIGHT");
    
    for(i=0; i<SCENE_MESH_COUNT; i++){
        fprintf(stderr, "Building BVH for mesh %d...\n", i);
        mesh_bvh_roots[i] = build_bvh(i);
    }
    if(f_env) frame  = atoi(f_env);
    if(w_env) width  = atoi(w_env);
    if(h_env) height = atoi(h_env);

    printf("P3\n%d %d\n255\n",width,height);

    double angle = (double)frame * 2.0 * PI / (double)total_frames;
    double cam_dist = 8.0;
    Vec3 ro, target, forward, up_tmp, up, right, cam_y;
    make_vec3(cam_dist * sin(angle), 1.5, cam_dist * cos(angle), &ro);
    make_vec3(0, 0, -2.0, &target); /* Look at the center of the fleet */
    
    v_sub(&target, &ro, &forward); v_norm(&forward, &forward);
    make_vec3(0, 1.0, 0, &up_tmp);
    v_cross(&forward, &up_tmp, &right); v_norm(&right, &right);
    v_cross(&right, &forward, &up); v_norm(&up, &up);

    for(j=height-1; j>=0; j--){
        for(i=0; i<width; i++){
            double u=(double)i/(double)(width-1);
            double v=(double)j/(double)(height-1);
            double dx=(u*2.0-1.0)*0.6*(double)width/(double)height;
            double dy=(v*2.0-1.0)*0.6;
            
            Vec3 rd, right_comp, up_comp, forward_comp, rdn;
            v_mul(&right, dx, &right_comp);
            v_mul(&up, dy, &up_comp);
            v_mul(&forward, 1.0, &forward_comp);
            v_add(&right_comp, &up_comp, &rd);
            v_add(&rd, &forward_comp, &rd);
            v_norm(&rd, &rdn);

            Vec3 col;
            trace(&ro,&rdn,0,&col);

            int ir=(int)(255.99*col.x), ig=(int)(255.99*col.y), ib=(int)(255.99*col.z);
            if(ir>255)ir=255; if(ig>255)ig=255; if(ib>255)ib=255;
            if(ir<0)ir=0; if(ig<0)ig=0; if(ib<0)ib=0;
            printf("%d %d %d\n",ir,ig,ib);
        }
    }
    return 0;
}
