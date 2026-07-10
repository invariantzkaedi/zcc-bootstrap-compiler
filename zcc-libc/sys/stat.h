struct stat {
    int st_dev;
    int st_ino;
    int st_mode;
    int st_nlink;
    int st_uid;
    int st_gid;
    int st_rdev;
    long st_size;
    long st_blksize;
    long st_blocks;
    long st_atime;
    long st_mtime;
    long st_ctime;
};
int stat(const char*, struct stat*);
int fstat(int, struct stat*);
