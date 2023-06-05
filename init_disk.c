#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <unistd.h>
#include <pwd.h>
#include "WFS.h"
/*磁盘（5M文件）的初始化步骤：
  5Mdisk can store 5*1024*1024/512 = 10240 blocks.One block can store 512*8=4096 bits.If we treat some blocks as bitmap, then the bitmap need 10240/4096(round up)=3 blocks at least, so we use 3 blocks to implement the bitmap, throwing out the extension from my mind XD.
*/

int main()
{
    printf("init_disk begin\n");
    FILE *fp=NULL;
    fp = fopen("/tmp/diskimg", "r+");//打开文件
	if (fp == NULL) {
		printf("打开文件失败，文件不存在\n");return 0;
    }
    struct super_block *super_blk; 
    struct file_directory *root;

    //1. 初始化super_block     大小：1块
    super_blk = malloc(sizeof(struct super_block));//动态内存分配，申请super_blk

    memset(super_blk,0,sizeof(struct super_block));
    super_blk->fs_size=10240;
    super_blk->first_blk=4;// 0 is the first one
    super_blk->bitmap=BITMAP_BLOCK;

    fwrite(super_blk, sizeof(struct super_block), 1, fp);
    printf("initial super_block success!\n");

    //initialize map_block    大小:3块= 12288bit
    if (fseek(fp, FS_BLOCK_SIZE * 1, SEEK_SET) != 0)//首先要将指针移动到文件的第二块的起始位置512
        fprintf(stderr, "bitmap fseek failed!\n");

    //512*3=1536
    char bitmap_init[BITMAP_BLOCK*FS_BLOCK_SIZE]={0};
    bitmap_init[0] |= 0x1F; //1 for super block ,3 for bitmap block,1 for root directory 
    int last_byte = 10236/8;
    int last_bit = 10236%8;
    while(last_bit<8){
        bitmap_init[last_byte] |= 1<<last_bit++;
    }
    last_byte++;
    while(last_byte<BITMAP_BLOCK*FS_BLOCK_SIZE){
        bitmap_init[last_byte++]=0xff;
    }
    fwrite(bitmap_init,FS_BLOCK_SIZE * super_blk->bitmap, 1, fp);

    //3. initialize root directory   
    fseek(fp, FS_BLOCK_SIZE * (super_blk->first_blk), SEEK_SET);//skip 4 block
    struct data_block* block = (struct data_block*)malloc(sizeof(struct data_block));
    memset(block, 0, sizeof(struct data_block));
    block->size=sizeof(struct file_directory);
 
    root =(struct file_directory*)block->data;
    memset(root->fname,0,MAX_FILENAME+1);
    root->fname[0]='/';
    // get time
    time(&(root->atime));
    time(&(root->mtime));
    root->fsize=0;
    memset(root->sectors,0,MAX_BLOCKNUM*sizeof(uint32_t));
    root->flag=2;
    root->authority =3;// 11b->rw
    struct passwd* pwd;
    root->uid=getuid();
    pwd=getpwuid(root->uid);
    strcpy(root->owner, pwd->pw_name);
    fwrite(block,FS_BLOCK_SIZE , 1, fp); 
    printf("initial data_block success!\n");
    fflush(fp);
    fclose(fp);
    free(block);
    free(super_blk);
    printf("super_bitmap_data_blocks init success!\n");
    return 0;
}
