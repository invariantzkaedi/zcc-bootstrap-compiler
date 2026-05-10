#include <stdio.h>

struct Point {
    int x;
    int y;
};

struct Line {
    struct Point p1;
    struct Point p2;
};

struct Triangle {
    struct Line edges[3];
};

struct Polyhedron {
    struct Triangle faces[4];
    int face_count;
};

struct Matrix3D {
    int data[3][3][3];
};

// Global nested structs and arrays
static struct Polyhedron tetra = {
    {
        { { { {0, 0}, {1, 0} }, { {1, 0}, {0, 1} }, { {0, 1}, {0, 0} } } },
        { { { {0, 0}, {1, 0} }, { {1, 0}, {0, 1} }, { {0, 1}, {0, 0} } } },
        { { { {0, 0}, {1, 0} }, { {1, 0}, {0, 1} }, { {0, 1}, {0, 0} } } },
        { { { {0, 0}, {1, 0} }, { {1, 0}, {0, 1} }, { {0, 1}, {0, 0} } } }
    },
    4
};

static struct Matrix3D m = {
    {
        {
            {1, 2, 3}, {4, 5, 6}, {7, 8, 9}
        },
        {
            {11, 12, 13}, {14, 15, 16}, {17, 18, 19}
        },
        {
            {21, 22, 23}, {24, 25, 26}, {27, 28, 29}
        }
    }
};

int main() {
    // Local nested structs and arrays
    struct Matrix3D local_m = {
        {
            {
                {1, 2, 3}, {4, 5, 6}, {7, 8, 9}
            },
            {
                {11, 12, 13}, {14, 15, 16}, {17, 18, 19}
            },
            {
                {21, 22, 23}, {24, 25, 26}, {27, 28, 29}
            }
        }
    };
    
    struct Polyhedron local_tetra = {
        {
            { { { {0, 0}, {1, 0} }, { {1, 0}, {0, 1} }, { {0, 1}, {0, 0} } } },
            { { { {0, 0}, {1, 0} }, { {1, 0}, {0, 1} }, { {0, 1}, {0, 0} } } },
            { { { {0, 0}, {1, 0} }, { {1, 0}, {0, 1} }, { {0, 1}, {0, 0} } } },
            { { { {0, 0}, {1, 0} }, { {1, 0}, {0, 1} }, { {0, 1}, {0, 0} } } }
        },
        4
    };

    if (tetra.face_count != 4) return 1;
    if (tetra.faces[0].edges[1].p1.x != 1) return 1;
    if (m.data[2][1][2] != 26) return 1;
    if (m.data[0][0][0] != 1) return 1;

    if (local_tetra.face_count != 4) return 1;
    if (local_tetra.faces[0].edges[1].p1.x != 1) return 1;
    if (local_m.data[2][1][2] != 26) return 1;
    if (local_m.data[0][0][0] != 1) return 1;

    printf("DEEP RECURSIVE INITIALIZERS PASSED\n");
    return 0;
}
