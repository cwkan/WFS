#ifndef __WFS
#define __WFS
#include <stddef.h>
#include <stdint.h>
#define FS_BLOCK_SIZE 512
#define SUPER_BLOCK 1
#define BITMAP_BLOCK 3 
#define ROOT_DIR_BLOCK 1
#define MAX_DATA_IN_BLOCK 504
#define MAX_DIR_IN_BLOCK 8
#define MAX_FILENAME 8
#define MAX_EXTENSION 3
#define MAX_OWNERNAME 16
#define MAX_BLOCKNUM 13
#define MAX_PATH_NAME_LEN 50
long TOTAL_BLOCK_NUM;
//超级块中记录的，大小为 24 bytes（3个long），占用1块磁盘块
struct super_block {
    uint32_t fs_size; //size of file system, in blocks（以块为单位）
    uint32_t first_blk; //first block of root directory（根目录的起始块位置，以块为单位）
    uint32_t bitmap; //size of bitmap, in blocks（以块为单位）
};

//记录文件信息的数据结构,统一存放在目录文件里面，也就是说目录文件里面存的全部都是这个结构，占用1块磁盘块
struct file_directory {
    char fname[MAX_FILENAME + 1]; //文件名 (plus space for nul)
    char fext[MAX_EXTENSION + 1]; //扩展名 (plus space for nul)
    time_t atime; 	/* 上次访问时间 */
    time_t mtime;	/*上次修改时间 */
//  time_t ctime; 	/* 上次文件状态改变时间 */
    uid_t uid;        //
    int mode;       //
    size_t fsize;   //文件大小（file size）
    uint32_t sectors[MAX_BLOCKNUM]; //record where the blocks exist, and the thirteen block secondary index.
    int flag; //indicate type of file. 0:for unused; 1:for file; 2:for directory
    int authority;
    char owner[MAX_OWNERNAME];
};

//文件内容存放用到的数据结构，大小为 512 bytes，占用1块磁盘块
struct data_block {
    size_t size; //文件使用了这个块里面的多少Bytes
    char data[MAX_DATA_IN_BLOCK];// And all the rest of the space in the block can be used for actual data storage.
};

char *disk_path="/tmp/diskimg";

//辅助函数声明
void read_cpy_file_dir(struct file_directory *a,struct file_directory *b);
int read_cpy_data_block(long blk_no,struct data_block *data_blk);
int write_data_block(long blk_no,struct data_block *data_blk);
int divide_path(char *name,char * ext, const char *path,struct file_directory * file_buf , int flag);
int exist_check(struct file_directory *parent_file_dir,const char *fname,const char * fext,uint32_t* offset, uint32_t *pos,int flag,int way,struct file_directory *  file_dir_buf);
int set_blk_use(long start_blk,int flag);
int path_is_emp(const char* path);
int setattr(const char* path, struct file_directory* attr, int flag);
void ClearBlocks(struct file_directory * file_dir);
int get_empty_blk(int num, uint32_t* start_blk);

//功能函数声明
int get_fd_to_attr(const char * path,struct file_directory *attr);
int create_file_dir(const char* path, int flag);
int remove_file_dir(const char *path, int flag);
#endif
