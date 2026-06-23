float sqrtf(float);
float sinf(float);
float cosf(float);
float powf(float, float);

static float tbl_a[] = {
    sqrtf(2.0f),
    sinf(0.5f),
    cosf(0.5f),
    powf(2.0f, 5.0f)
};

static float tbl_b[] = {
    sqrtf(2.0f),
    sinf(0.5f),
    cosf(0.5f),
    powf(2.0f, 5.0f)
};

int main(void) {
    unsigned int *a = (unsigned int *)tbl_a;
    unsigned int *b = (unsigned int *)tbl_b;
    return (int)(a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && a[3] == b[3]);
}
