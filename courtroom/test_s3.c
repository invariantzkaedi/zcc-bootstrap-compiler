int main() {
    struct S3 {
        short f0;
        short f1;
        int f2;
        int f3;
    };
    struct S3 s3 = {26, 42, 35, 23};
    return s3.f0;
}
