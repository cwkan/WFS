#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/time.h>
#include "WFS.h"
struct file_directory * root;
struct super_block * super_blk;
FILE *fp;
//该函数为读取并复制file_directory结构的内容，因为文件系统所有对文件的操作都需要先从文件所在目录
//读取file_directory信息,然后才能找到文件在磁盘的位置，后者的数据复制给前者
void read_cpy_file_dir(struct file_directory *a,struct file_directory *b) {
    memcpy(a,b,sizeof(struct file_directory));
}
//根据文件的块号，从磁盘（5M大文件）中读取数据
//步骤：① 打开文件；② 将FILE指针移动到文件的相应位置；③ 读出数据块
int read_cpy_data_block(long blk_no,struct data_block *data_blk) {
	FILE *fp=NULL;
	fp=fopen(disk_path,"r+");
	if (fp == NULL){
		printf("错误：read_cpy_data_block：打开文件失败\n\n");return -1;
	}
	//文件打开成功以后，就用blk_no * FS_BLOCK_SIZE作为偏移量
	fseek(fp, blk_no*FS_BLOCK_SIZE, SEEK_SET);
	fread(data_blk,FS_BLOCK_SIZE,1,fp);
	if(ferror(fp)){//看读取有没有出错
		printf("错误：read_cpy_data_block：读取文件失败\n\n");return -1;
	}
	return 0;//读取成功则关闭文件，然后返回0
}

//根据文件的块号，将data_block结构写入到相应的磁盘块里面
//步骤：① 打开文件；② 将FILE指针移动到文件的相应位置；③写入数据块
int write_data_block(long blk_no,struct data_block *data_blk) {
	FILE *fp=NULL;
	fp=fopen(disk_path,"r+");
	if (fp == NULL)	{
		printf("错误：write_data_block：打开文件失败\n\n");return -1;
	}
	fseek(fp, blk_no*FS_BLOCK_SIZE, SEEK_SET);
	fwrite(data_blk,sizeof(struct data_block),1,fp);
	if(ferror(fp))	{//看读取有没有出错
		printf("错误：write_data_block：读取文件失败\n\n");return -1;
	}
    fflush(fp);
    return 0;//写入成功则关闭文件，然后返回0
}

//路径分割，获得path所指对象的名字、后缀名，返回父目录文件的起始块位置，path是要创建的文件（目录）的目录
//步骤：① 先检查是不是二级目录（通过找第二个'/'就可以了），如果是，那么在'/'处截断字符串；
//		  如果不是，则跳过这一步
//	   ② 然后要获取这个父目录的file_directory
//	   ③ 再从path中获取文件名和后缀名（获取的时候还要判断文件名和后缀名的合法性）
	   //其中par_size是父目录文件的大小
int divide_path(char *name,char * ext, const char *path,struct file_directory * file_buf , int flag) {
	printf("divide_path：函数开始\n\n");
	char *tmp_path,*m,*n;
	tmp_path=strdup(path);//用来记录最原始的路径
    n=NULL;
	m=tmp_path;
    char * sign = strrchr(tmp_path,'/');
    if(sign==tmp_path){
        read_cpy_file_dir(file_buf,root);
    }else{
            
        *sign='\0';
        if(get_fd_to_attr(tmp_path,file_buf)==-1){
           printf("cannot find the directory\n");
           return -ENOENT;
        }
        *sign='/';
    }
    m=sign+1;
    if(flag==1)	{
		printf("divide_path:这是文件，有后缀名\n\n");
		n=strchr(m,'.');
		if(n!=NULL)		{
			*n='\0';//截断tmp_path
			n++;//此时n指针指向后缀名的第一位
		}
	}
	//要创建对象，还要检查：文件名（目录名），后缀名的长度是否超长
	if (flag == 1) { //如果创建的是文件
		if (strlen(m) > MAX_FILENAME + 1) 		{
		free(tmp_path);
			return -ENAMETOOLONG;
		} 
		else if (strlen(m) > MAX_FILENAME) 		{
			if (*(m + MAX_FILENAME) != '~') 			{
			free(tmp_path);
				return -ENAMETOOLONG;
			}
        }else if (n!= NULL) //如果有后缀名
		{
			if (strlen(n) > MAX_EXTENSION + 1) 			{
				free(tmp_path);	return -ENAMETOOLONG;
			} 
			else if (strlen(n) > MAX_EXTENSION) 			{
				if (*(n + MAX_EXTENSION) != '~') 
				{
					free(tmp_path);	return -ENAMETOOLONG;
				}
			}
		}
    }else if (flag == 2) //如果创建的是目录
	{
        n=NULL;
		if (strlen(m) > MAX_DIR_IN_BLOCK) {
			return -ENAMETOOLONG;
		}
	}

	if (m != NULL && name !=NULL && *m!=0 && strlen(m)!=0){
         strcpy(name, m);
         name[strlen(m)]='\0';	
    }
	if (n != NULL&&ext!=NULL && *n !=0 && strlen(n)!=0 ){
         strcpy(ext, n);
         ext[strlen(n)]='\0';	
    }
	printf("已经获取到父目录的file_directory（attr），检查一下：\n\n");
	printf("attr:fname=%s，fsize=%lu，firstBlock=%u，flag=%d\n\n",file_buf->fname,\
	file_buf->fsize,file_buf->sectors[0],file_buf->flag);

	printf("divide_path：检查过要创建对象的文件（目录）名，并没有问题\n\n");
	printf("divide_path：分割后的父目录名：%s\n文件名：%s\n\n",tmp_path,m);
	printf("divide_path：函数结束返回\n\n");
	free(tmp_path);
	
	return 0;
}

//遍历父目录下的all文件块内的所有文件和目录，如果已存在同名文件或目录，返回-EEXIST
//注意file_dir指针是调用函数的地方传过来的，直接++就可以了
int exist_check(struct file_directory *parent_file_dir,const char *fname,const char * fext,uint32_t* offset, uint32_t *pos,int flag,int way,struct file_directory *  file_dir_buf)
 //way 0 for checking duplication of name; 1 for checking while there has the file or directory;2 for find target_file_dir;3 for finding
 // having the file_directory which has the same name as fname
{
	printf("exist_check：现在开始检查该data_blk是否存在重名的对象\n\n");
    uint32_t sectors[MAX_DATA_IN_BLOCK/(sizeof(uint32_t))+12]={0};
    uint32_t num;
    *offset=*pos=0;
    if(parent_file_dir->sectors[12]==0){
        num=12;
    }else{
        num=MAX_DATA_IN_BLOCK/(sizeof(uint32_t))+12;        
        struct data_block block;
        memset(&block,0,sizeof(struct data_block));
        read_cpy_data_block(parent_file_dir->sectors[12],&block);
        memcpy((char *)(&sectors[12]),block.data,MAX_DATA_IN_BLOCK);
    }
    int i;
    for(i=0;i<12;i++){
        sectors[i]=parent_file_dir->sectors[i];
    }
    struct file_directory file_dir;
    struct data_block block;
    memset(&block, 0, sizeof(struct data_block));
    uint32_t way_offset=513;
    for(i=0;i<num;i++){
       if(sectors[i]==0){
            continue;
       }
       *offset=0;
       while(*offset+sizeof(struct file_directory) < (MAX_DATA_IN_BLOCK)){
            memset(&block, 0, sizeof(struct data_block));
            read_cpy_data_block(sectors[i],&block);
            memset(&file_dir, 0, sizeof(struct file_directory));
            struct file_directory* file_block=(struct file_directory*)(block.data+*offset);
            memcpy((char*)(&file_dir),(char*)file_block, sizeof(struct file_directory));
         
            if(way==0){
            	if (file_dir.flag == 0 && way_offset ==513){
                    *pos =i;
                    way_offset=*offset;
                }
	          	//如果文件名、后缀名（无后缀名）皆匹配
           		else if (flag == 1 && file_dir.flag == 1 )
        		{
    		    	if(strcmp(fname, file_dir.fname) == 0){
			          	if((*fname == '\0' && strlen(file_dir.fext) == 0) || (fext<=10 &&*fext != '\0' && strcmp(fext, file_dir.fext) == 0))
			        	{                        
			        		printf("错误：exist_check：存在重名的文件对象，函数结束返回-EEXIST\n\n");return -EEXIST;
		        		}
	        		}
        		}			
	        	//如果目录名匹配
	        	else if (flag == 2 && file_dir.flag == 2 && strcmp(fname, file_dir.fname) == 0)		{
        			printf("错误：exist_check：存在重名的目录对象，函数结束返回-EEXIST\n\n");return -EEXIST;
                }
            }else if(way == 1){
            	if (flag == 0) {
                     *pos =i;
                }
	            //如果文件名、后缀名（无后缀名）皆匹配
	    	    else if (flag == 1 && file_dir.flag == 1 ){
                    printf("错误：exist_check：存在文件对象，函数结束返回-EEXIST\n\n");return -EEXIST;
    	        }else if (flag == 2 && file_dir.flag == 2 ){
    		      	printf("错误：exist_check：存在目录对象，函数结束返回-EEXIST\n\n");return -EEXIST;
    	        }
            }else if(way==2){
                *pos =i;
	            if (((flag == 1 && file_dir.flag == 1)|| (flag == 2 && file_dir.flag == 2)))
	        	{
                    if(file_dir.fext==NULL || fext<=10){
                        if(strcmp(fname,file_dir.fname)==0){
                            memcpy((char*)file_dir_buf, (char*)(&file_dir), sizeof(struct file_directory));
                            return 0;
                        }
                    }else if(strcmp(fext,file_dir.fext)==0 && strcmp(fname,file_dir.fname)==0){
                        printf("%d %d\n",strcmp(fext,file_dir.fext),strcmp(fname,file_dir.fname));
                        memcpy((char*)file_dir_buf, (char*)(&file_dir), sizeof(struct file_directory));
                        return 0;
                    }
        		}
            }else if(way==3){
                if(fext<=10 && *(file_dir.fext)!='\0'){
                    if(strcmp(fname, file_dir.fname) == 0 && strcmp(fext,file_dir.fext)==0){
                       memcpy((char*)file_dir_buf, (char*)(&file_dir), sizeof(struct file_directory));   
                       return 0;
                    }
                }else {
                    if(strcmp(fname,file_dir.fname)==0){
                       memcpy((char*)file_dir_buf, (char*)(&file_dir), sizeof(struct file_directory));
                       return 0;
                    }
                }
            }
            *offset += sizeof(struct file_directory);
       	}  
    }
	printf("exist_check：函数结束返回\n\n");
    if(way==2)
        return -EEXIST;
    else if(way==0){
        *offset=way_offset;
    }
	return 0;
}

//在bitmap中标记第start_blk块是否被使用（flag=0,1)
int set_blk_use(long start_blk,int flag) {
	printf("set_blk_use：函数开始");
	if(start_blk==-1) {printf("错误：set_blk_use：你和我开玩笑？start_blk为-1，函数结束返回\n\n");return -1;}
	
	int start=start_blk/8;//因为每个byte有8bits，要找到start_blk在bitmap的第几个byte中，要除以8
	int left=(start_blk%8);//计算在bitmap中的这个byte的第几位表示该块
	unsigned char f = 0x00;
	unsigned char mask = 0x01;
    mask<<=left;//构造相应位置1的掩码
	

	fseek(fp, FS_BLOCK_SIZE + start, SEEK_SET);//super_block占了FS_BLOCK_SIZE个bytes
	unsigned char *tmp = malloc(sizeof(unsigned char));
	fread(tmp, sizeof(unsigned char), 1, fp);//把相应的byte读出来
	f = *tmp;

	//将该位置1，其他位不动
	if (flag) f |= mask;
	//将该位置0，其他位不动
	else f &= ~mask;

	*tmp = f;
	fseek(fp, FS_BLOCK_SIZE + start, SEEK_SET);
	fwrite(tmp, sizeof(unsigned char), 1, fp);
	printf("set_blk_use：bitmap状态设置成功,函数结束返回\n\n");
	free(tmp);
	return 0;
}

//判断该path中是否含有目录和文件，如果为空则返回1，不为空则返回0
int path_is_emp(const char* path) {
	printf("path_is_emp：函数开始\n\n");
	struct file_directory attr;
    memset(&attr,0,sizeof(struct file_directory));
	//读取属性到attr里
	if (get_fd_to_attr(path, &attr) == -1) 	{
		printf("错误：path_is_emp：get_fd_to_attr失败，path所指对象的file_directory不存在，函数结束返回\n\n");
		return 0;
	}
	//传入路径为文件
	if (attr.flag == 1) 	{
		printf("错误：path_is_emp：传入的path是文件的path，不能进行判断，函数结束返回\n\n");
	    return 0;
	}
    uint32_t pos,offset;
    if(exist_check(&attr,NULL,NULL,&offset,&pos, 2,1,NULL)){
        return 0;
    }
    printf("path_is_emp：判断完毕，函数结束返回\n\n");
	return 1;
}

//将file_directory的数据赋值给path相应文件或目录的file_directory	(!!!!!)
int setattr(const char* path, struct file_directory* attr, int flag)  {
	printf("setattr：函数开始\n\n");
	int res;
    struct file_directory file_buf;
    memset(&file_buf,0,sizeof(struct file_directory));
	//char *name=malloc(15*sizeof(char));
    char path_dup[30]={0};
    strcpy(path_dup,path);
    if(strcmp(path_dup,"/")==0){
        if(flag==1){
             printf("root file no exist");
             return -1;
        }
        struct data_block block;
        read_cpy_data_block(super_blk->first_blk,&block);
        memcpy(block.data,(char*)attr,sizeof(struct file_directory));
        memcpy(root,attr,sizeof(struct file_directory));
        write_data_block(super_blk->first_blk,&block);
        return 0;
    }
    char * sign = strrchr(path_dup,'/');
    struct file_directory parent_file;
    memset(&parent_file,0,sizeof(struct file_directory));
    uint32_t sector_index,offset;
    if(sign == path_dup){//the target file exists in root directory
        memcpy(&parent_file,root,sizeof(struct file_directory));        
    }else{
        *sign='\0';
        memset(&parent_file,0,sizeof(struct file_directory));
        res = get_fd_to_attr(path_dup,&parent_file);//find parent file_directory
        *sign='/';
    }
    if(flag==2){
       if(res= exist_check(&parent_file,sign+1,NULL,&offset,&sector_index,flag,2,&file_buf)){
        printf("the file_directory no exist");
        return -1;
       }
    }else if(flag==1){
       char * fext = strrchr(path_dup,'.');
       *fext='\0';
       if(res= exist_check(&parent_file,sign+1,fext+1,&offset,&sector_index,flag,2,&file_buf)){
           printf("the file_directory no exist");
           return -1;
       }
       *fext='.';
    }
    struct data_block buf;
    memset(&buf,0,sizeof(struct data_block));
    uint32_t first_block;
    if(sector_index<=11){
         first_block=parent_file.sectors[sector_index];
    }else{
         struct data_block sector_block;
         memset(&sector_block,0,sizeof(struct data_block));
         read_cpy_data_block(parent_file.sectors[12],&sector_block);
         uint32_t * sector = (uint32_t*)(sector_block.data);
         first_block = sector[sector_index-12];
    }
    if(read_cpy_data_block(first_block,&buf)){
         printf("copy block failed");
         return -1; 
    }
    memcpy((buf.data)+ offset,(char*)attr,sizeof(struct file_directory));
    buf.size+=sizeof(struct file_directory);
    write_data_block(first_block,&buf);
    printf("setattr：赋值成功，函数结束返回\n\n");
 	//找遍整个目录都没找到该文件就直接返回-1
	return 0;
}

//从next_blk起清空data_blk后续块
void ClearBlocks(struct file_directory * file_dir) {
	printf("ClearBlocks：函数开始\n\n");
    int i;
    for(i=0;i<=11;i++){
        if(file_dir->sectors[i]==0)
             continue;
        set_blk_use(file_dir->sectors[i], 0);//在bitmap中设置相应位没有被使用
    }
    if(file_dir->sectors[12]!=0){
        struct data_block block;
        if(read_cpy_data_block(file_dir->sectors[12],&block)){
            printf("read block failed");
            return;
        }
        uint32_t * sector=(uint32_t*)block.data;
        int j=0;
        while(j<(MAX_DATA_IN_BLOCK/sizeof(uint32_t))){
            if(*sector ==0){
                sector++;
                j++;
                continue;
            }
	        set_blk_use(*sector, 0);//在bitmap中设置相应位没有被使用
            sector++;
            j++;
        }
        set_blk_use(file_dir->sectors[12],0);
    }
	printf("ClearBlocks：函数结束返回\n\n");
}

 //找到num个连续空闲块，返回空闲块区的起始块号start_blk，返回找到的连续空闲块个数（否则返回找到的最大值）
 //这里我采用的是首次适应法（就是找到第一片大小大于num的连续区域，并将这片区域的起始块号返回）
int get_empty_blk(int num, uint32_t * start_blk) {
	printf("get_empty_blk：函数开始\n\n");
	//从头开始找，跳过super_block、bitmap和根目录块，现在偏移量为1282（从第 0 block 偏移1282 blocks，指向编号为1282的块（其实是第1283块））
	*start_blk = 1 + BITMAP_BLOCK + 1;
	int tmp = 0;
	//打开文件
	int start, left;
	unsigned char mask, f;//8bits
	unsigned char * flag;
	unsigned char * head;
	//max和max_start是用来记录可以找到的最大连续块的位置
	int max = 0;
	long max_start = -1;

	//要找到一片连续的区域，我们先检查bitmap
	//要确保start_blk的合法性(一共10240块，则块编号最多去到10239)
	//每一次找不到连续的一片区域存放数据，就会不断循环，直到找到为止
	printf("get_empty_blk：现在开始寻找一片连续的区域\n\n");
	while(*start_blk < TOTAL_BLOCK_NUM )	{
		start = *start_blk / 8;//start_blk每个循环结束都会更新到新的磁盘块空闲的位置
		left = (*start_blk % 8);//1byte里面的第几bit是空的
		mask = 1; mask <<= left;

		fseek(fp, FS_BLOCK_SIZE + start, SEEK_SET);//跳过super block，跳到bitmap中start_blk指定位置（以byte为单位）

		flag = malloc(num);
		fread(flag,num , 1, fp);
        head=flag;
		f = *flag;

		//下面开始检查这一片连续存储空间是否满足num块大小的要求
		for (tmp = 0; tmp < num; tmp++) 		{
			//mask为1的位，f中为1，该位被占用，说明这一片连续空间不够大，跳出
			if ((f & mask) == mask)	break;
			//maskhighest位为1，说明这个byte已检查到highest位，8个bit已读完
			if ((mask & 0x80) == 0x80) 			{//读下8个bit
				f = *flag++;
				mask = 0x01; //指向8个bit的lowest位
			} 
			//位为1,left移1位，检查下一位是否可用
			else mask <<= 1;
		}
		//跳出上面的循环有两种可能，一种是tmp==num，说明已经找到了连续空间
		//另外一种是tmp<num说明这片连续空间不够大，要更新start_blk的位置
		//tmp为找到的可用连续块数目
		if (tmp > max) 		{
			//记录这个连续块的起始位
			max_start = *start_blk;
			max = tmp;
		}
		//如果后来找到的连续块数目少于之前找到的，不做替换
		//找到了num个连续的空白块
		if (tmp == num) break;

		//只找到了tmp个可用block，小于num，要重新找，更新起始块号
		*start_blk = (tmp + 1) + *start_blk;
		tmp = 0;
		//找不到空闲块
        free(head);
	}
	*start_blk = max_start;

	int j = max_start;
	int i;
	//将这片连续空间在bitmap中标记为1
	for (i = 0; i < max; i++) 	{
		if (set_blk_use(j++, 1) == -1) 		{
			printf("错误：get_empty_blk：set_blk_use失败，函数结束返回\n\n");
			free(head); return i;//return how much blocks have been allocated
		}
	}
	printf("get_empty_blk：申请空间成功，函数结束返回\n\n");
	free(head);
	return max;
}

/***************************************************************************************************************************/
//三个功能函数:getattr,create_file_dir,remove_file_dir

//根据文件的路径，到相应的目录寻找该文件的file_directory，并赋值给attr
int get_fd_to_attr(const char * path,struct file_directory *attr) {
    // char * p=strchr(path,'.');
    // if(p!=NULL)
    //     *p='\0';
    if(strcmp(path,"/")==0){
        read_cpy_file_dir(attr,root);
        return 0;
    }
    // if(p!=NULL)
    //     *p='.';
    char* path_dup = strdup(path);
    char * sign=strrchr(path_dup,'/');
    char * fext = strrchr(path_dup,'.');
    struct file_directory  file_dir_buf;
    memset(&file_dir_buf , 0, sizeof(struct file_directory));
    uint32_t offset,pos;
    if(path_dup==sign){      
        if(fext!=NULL && strlen(fext)!=0){
             *fext='\0';
             if(exist_check(root,sign+1,fext+1,&offset,&pos,0,3,&file_dir_buf)){
                free(path_dup);
                printf("find file_dir failed\n");
                return -1;
             }
             *fext='.'; 
        }else{
             if(exist_check(root,sign+1,NULL,&offset,&pos,0,3,&file_dir_buf)){
                free(path_dup);
                printf("find file_dir failed\n");
                return -1;
             }
        }
        read_cpy_file_dir(attr,&file_dir_buf);
        free(path_dup);
        return 0;
    }else{
        sign=strchr(path_dup+1,'/');
        *sign='\0';
        fext=strchr(path_dup,'.');
        if(fext!=NULL && strlen(fext)!=0){
             *fext=0;
             if(exist_check(root,path_dup+1,fext+1,&offset,&pos,0,3,&file_dir_buf)){
                free(path_dup);
                printf("find file_dir failed\n");
                return -1;
             }
             *fext='.'; 
        }else{
             if(exist_check(root,path_dup+1,NULL,&offset,&pos,0,3,&file_dir_buf)){
                free(path_dup);
                printf("find file_dir failed\n");
                return -1;
             }
        }
        *sign='/';
        char * way=sign;
        sign=strrchr(path_dup,'/');
        while(way!=sign){
            if(file_dir_buf.flag==1){
                printf("%s should be a directory",file_dir_buf.fname);
                free(path_dup);
                return 0;
            }
            way=way+1;
            char* point = strchr(way,'/');
            *point='\0';
            fext=strchr(way,'.');
            char * lastP=fext;
            if(fext!=NULL){
                *fext=0;
                fext=fext+1;
            }
            struct file_directory next_file_dir;
            memset(&next_file_dir,0,sizeof(struct file_directory));
            if(exist_check(&file_dir_buf,way,fext,&offset,&pos,0,3,&next_file_dir)){
                 printf("find file_dir in path failed\n");
                 return -1;
            }
            if(fext!=NULL && strlen(fext)!=0){
                *lastP='.';
            }
            read_cpy_file_dir(&file_dir_buf,&next_file_dir);
            *point='/';
            way=point;
        }
        struct file_directory  target_file_dir;
        memset(&target_file_dir,0,sizeof(struct file_directory));
        char * fext=strrchr(path_dup,'.');
        if (fext!=NULL)
        {
             *fext='\0';
        } 
        if(exist_check(&file_dir_buf,sign+1,fext+1,&offset,&pos,0,3,&target_file_dir)){
             free(path_dup);
             printf("find file_dir in path failed\n");
             return -1;
        }
        if (fext!=NULL)
        {
             *fext='.';
        } 
        read_cpy_file_dir(attr,&target_file_dir);
        free(path_dup);      
        return 0;
    }
	return -1;
}

//创建path所指的文件或目录的file_directory，并为该文件（目录）申请空闲块，创建成功返回0，创建失败返回-1
//mkdir和mknod这两种操作都要用到
int create_file_dir(const char* path, int flag) {
	printf("调用了create_file_dir，创建的类型是：%d，创建的路径是：%s\n\n",flag,path);
    struct file_directory file_buf;
    memset(&file_buf,0,sizeof(struct file_directory));
    char name[MAX_PATH_NAME_LEN]={0},ext[MAX_PATH_NAME_LEN]={0};
    if(divide_path(name,ext,path,&file_buf,flag)){
        printf("divide_path error\n");
        return -1;
    }
    if(file_buf.flag==1){
        printf("parent_file_dir is file\n");
        return -1;
    }
    uint32_t offset,pos;
    if(exist_check(&file_buf, name,ext, &offset,&pos,flag,0,NULL)){
        printf("exist_check error\n");
        return -1;
    }
    struct file_directory target_file_dir;
    memset(&target_file_dir,0,sizeof(struct file_directory));
    strcpy(target_file_dir.fname,name);   
    strcpy(target_file_dir.fext,ext);   
    // get time
    time(&(target_file_dir.atime));
    time(&(target_file_dir.mtime));
    target_file_dir.flag= flag;
    target_file_dir.uid=geteuid();
    target_file_dir.authority=3;
    target_file_dir.fsize=sizeof(struct file_directory);
    strcpy(target_file_dir.owner,root->owner);

    struct data_block new_block;
    memset(&new_block,0,sizeof(struct data_block));            

    uint32_t num=0;
    int i=0;
    if(offset >=0 && offset<=(MAX_DATA_IN_BLOCK-sizeof(struct file_directory))){
        struct data_block block; 
        memset(&block,0,sizeof(struct data_block));
        read_cpy_data_block(file_buf.sectors[pos],&block);
        memcpy((block.data) + offset,(char*)(&target_file_dir),sizeof(struct file_directory));
        block.size+=sizeof(struct file_directory);
        write_data_block(file_buf.sectors[pos],&block);
    }else{
        uint32_t sectors[MAX_DATA_IN_BLOCK/(sizeof(uint32_t))+12]={0};
        if(file_buf.sectors[12]==0){
           num=12;
        }else{
            num=MAX_DATA_IN_BLOCK/(sizeof(uint32_t))+12;   
            struct data_block block;
            memset(&block,0,sizeof(struct data_block));
            read_cpy_data_block(file_buf.sectors[12],&block);
            memcpy(((char*)(&sectors[12])),block.data,MAX_DATA_IN_BLOCK);
        }
        for(i=0;i<12;i++){
              sectors[i]=file_buf.sectors[i];
        }
        for(i=0;i<num;i++){
            if(sectors[i]==0){
                if(i<12){
                    if(get_empty_blk(1,&(file_buf.sectors[i]))!=1){
                        printf("request one block failed\n");
                        return -1;
                    }
                    new_block.size=sizeof(struct file_directory);
                    memcpy(new_block.data,(char*)(&target_file_dir),sizeof(struct file_directory));
                    // new_block.size=sizeof(struct file_directory);
                    write_data_block(file_buf.sectors[i],&new_block);
                    break;
                }else{
                    if(get_empty_blk(1,&(sectors[i]))!=1){
                        printf("request one block failed\n");
                        return -1;
                    }
                    struct data_block block;
                    memset(&block,0,sizeof(struct data_block));
                    uint32_t* index=(uint32_t*)block.data;
                    index[i-12]=sectors[i];
                    block.size+=sizeof(uint32_t);
                    new_block.size=sizeof(struct file_directory);
                    memcpy(new_block.data,(char*)(&target_file_dir),sizeof(struct file_directory));
                    write_data_block(sectors[i],&new_block);
                    write_data_block(file_buf.sectors[12],&block);
                    break;
               }
            }
        }
    }
    memset(&new_block,0,sizeof(struct data_block));            
    if(num==12 && i==12 && file_buf.sectors[12]==0){
        if(get_empty_blk(1,&(file_buf.sectors[12]))!=1 ||  get_empty_blk(1,(uint32_t*)(new_block.data))!=1 ||  (write_data_block(file_buf.sectors[12],&new_block))){
             printf("request one block failed\n");
             return -1;
        }
        uint32_t sec=((uint32_t*)new_block.data)[0];
        new_block.size+=sizeof(uint32_t);
        write_data_block(file_buf.sectors[12],&new_block);
        struct data_block data_blk;
        memset(&data_blk,0,sizeof(struct data_block));
        data_blk.size+=sizeof(struct file_directory);
        memcpy(data_blk.data,(char*)(&target_file_dir),sizeof(struct file_directory)); 
        write_data_block(sec,&data_blk);

    }
    file_buf.fsize+=sizeof(struct file_directory);
    char * sign=strrchr(path,'/');
    if(sign!=path){
        *sign=0;
        setattr(path,&file_buf,2);
        *sign='/';
    }else{    
        setattr("/",&file_buf,2);
    }

    return 0;
}

//删除path所指的文件或目录的file_directory和文件的数据块，成功返回0，失败返回-1
int remove_file_dir(const char *path, int flag) {
	printf("remove_file_dir：函数开始\n\n");
    if(strcmp(path,"/")==0){
        printf("cannot delete root directory!!!\n");
        return -1;
    }
	struct file_directory parent_file_dir;
    memset(&parent_file_dir,0,sizeof(struct file_directory));
    char name[MAX_PATH_NAME_LEN]={0},ext[MAX_PATH_NAME_LEN]={0};
	//读取文件属性
	if (divide_path(name,ext,path, &parent_file_dir,flag) == -1) 	{
        printf("错误：remove_file_dir：get_fd_to_attr失败，函数结束返回\n\n");	return -ENOENT;
	}
	struct file_directory target_file_dir;
    memset(&target_file_dir,0,sizeof(struct file_directory));
    uint32_t offset;
    uint32_t pos;
    if(exist_check(&parent_file_dir,name,ext,&offset,&pos,flag,2,&target_file_dir)){
        printf("find target_file_dir failed\n");
        return -1;
    }
	//flag与指定的不一致，则返回相应错误信息
	if (flag == 1 && target_file_dir.flag == 2)	{
        printf("错误：remove_file_dir：要删除的对象flag不一致，删除失败，函数结束返回\n\n");return -EISDIR;
	} 
	else if (flag == 2 && target_file_dir.flag == 1) 
	{
        printf("错误：remove_file_dir：要删除的对象flag不一致，删除失败，函数结束返回\n\n");return -ENOTDIR;
	}
	//清空该文件从起始块开始的后续块
	if (flag == 1){
	    ClearBlocks(&target_file_dir);
    } 
	else if (!path_is_emp(path)) //只能删除空的目录，目录非空返回错误信息
	{ 
		printf("remove_file_dir：要删除的目录不为空，删除失败，函数结束返回\n\n");
		return -ENOTEMPTY;
	}
    uint32_t block_index;
    struct data_block sector_block;
    memset(&sector_block,0,sizeof(struct data_block));
    if(pos>11){            
        read_cpy_data_block(parent_file_dir.sectors[12],&sector_block);
        uint32_t * sector = (uint32_t*)(sector_block.data);
        block_index = sector[pos-12];
        struct data_block block;
        memset(&block,0,sizeof(struct data_block));
        read_cpy_data_block(block_index,&block);
        ((struct file_directory*)(block.data+offset))->flag=0;
        block.size-=sizeof(struct file_directory);
        if(block.size==0){
            set_blk_use(block_index,0);
            sector[pos-12]=0;
            sector_block.size-=sizeof(uint32_t);
            write_data_block(parent_file_dir.sectors[12],&sector_block);
            if(sector_block.size==0){
                set_blk_use(parent_file_dir.sectors[12],0);
                parent_file_dir.sectors[12]=0;
            }
        }
        write_data_block(block_index,&sector_block);
    }else{
        block_index=parent_file_dir.sectors[pos];
        read_cpy_data_block(block_index,&sector_block);
        ((struct file_directory*)((sector_block.data)+offset))->flag=0;
        sector_block.size-=sizeof(struct file_directory);
        if(sector_block.size==0){
            set_blk_use(block_index,0);
            parent_file_dir.sectors[pos]=0;
        }
        write_data_block(block_index,&sector_block);
    }
    parent_file_dir.fsize-=sizeof(struct file_directory);
    char * sign = strrchr(path,'/');
    if (sign==path)
    {
        if (setattr("/",&parent_file_dir, 2) == -1) 	{
            printf("remove_file_dir：setattr失败，函数结束返回\n\n");
            return -1;
	    }
    }else{
        *sign='\0';
	    if (setattr(path,&parent_file_dir, 2) == -1) 	{
		    printf("remove_file_dir：setattr失败，函数结束返回\n\n");
            return -1;
    	}
        *sign='/';
    }
    
    
	printf("remove_file_dir：删除成功，函数结束返回\n\n");
	return 0;
}

/***************************************************************************************************************************/

//文件系统初始化函数，载入文件系统的时候系统需要知道这个文件系统的大小（以块为单位）
static void* WFS_init(struct fuse_conn_info *conn, struct fuse_config * config) {
	(void) conn;
	 //img_init();
	fp = fopen(disk_path, "r+");
	if (fp == NULL) {
		fprintf(stderr, "错误：打开文件失败，文件不存在，函数结束返回\n");
		return 0;
	}
    super_blk=malloc(sizeof(struct super_block));
    memset(super_blk,0,sizeof(struct super_block));
    fread(super_blk,sizeof(struct super_block),1,fp);
    root=malloc(sizeof(struct file_directory));
    memset(root,0,sizeof(struct file_directory));
    struct data_block *block= malloc(sizeof(struct data_block));
    fseek(fp, FS_BLOCK_SIZE * (super_blk->first_blk), SEEK_SET);
    memset(block,0,sizeof(struct data_block));
    fread(block,FS_BLOCK_SIZE , 1, fp); 
    memcpy((char*)root,block->data,sizeof(struct file_directory));
    free(block);

	//用超级块中的fs_size初始化全局变量
	TOTAL_BLOCK_NUM = super_blk->fs_size;
	return 0;
}

/*struct stat {
        mode_t     st_mode;       //文件对应的模式，文件，目录等
        ino_t      st_ino;       //inode节点号
        dev_t      st_dev;        //设备号码
        dev_t      st_rdev;       //特殊设备号码
        nlink_t    st_nlink;      //文件的连接数
        uid_t      st_uid;        //文件所有者
        gid_t      st_gid;        //文件所有者对应的组
        off_t      st_size;       //普通文件，对应的文件字节数
        time_t     st_atime;      //文件最后被访问的时间
        time_t     st_mtime;      //文件内容最后被修改的时间
        time_t     st_ctime;      //文件状态改变时间
        blksize_t st_blksize;    //文件内容对应的块大小
        blkcnt_t   st_blocks;     //文件内容对应的块数量
      };*/

//该函数用于读取文件属性（通过对象的路径获取文件的属性，并赋值给stbuf）
static int WFS_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)  {
    char path_dup[30]={0};
     if (path[strlen(path)-1]=='$')
    {
        memcpy(path_dup,path,strlen(path)-1);
    }else{
        memcpy(path_dup,path,strlen(path));
    }
	int res = 0;
	struct file_directory attr;

	//非根目录
	if (get_fd_to_attr(path_dup, &attr) == -1) 	{
		printf("WFS_getattr：get_fd_to_attr时发生错误，函数结束返回\n\n");return -ENOENT;
	}

	memset(stbuf, 0, sizeof(struct stat));//将stat结构中成员的值全部置0
	if (attr.flag==2)
	{//从path判断这个文件是		一个目录	还是	一般文件
		printf("WFS_getattr：这个file_directory是一个目录\n\n");
		stbuf->st_mode = S_IFDIR | 0666;//设置成目录,S_IFDIR和0666（8进制的文件权限掩码），这里进行或运算
        stbuf->st_uid=attr.uid;
		stbuf->st_size = attr.fsize;
        stbuf->st_atime=attr.atime;
        stbuf->st_mtime=attr.mtime;
		//stbuf->st_nlink = 2;//st_nlink是连到该文件的硬连接数目,即一个文件的一个或多个文件名。说白点，所谓链接无非是把文件名和计算机文件系统使用的节点号链接起来。因此我们可以用多个文件名与同一个文件进行链接，这些文件名可以在同一目录或不同目录。
	} 
	else if (attr.flag==1) 	{
		printf("WFS_getattr：这个file_directory是一个文件\n\n");
		stbuf->st_mode = S_IFREG | 0666;//该文件是	一般文件
		stbuf->st_size = attr.fsize;
        stbuf->st_uid=attr.uid;
        stbuf->st_atime=attr.atime;
        stbuf->st_mtime=attr.mtime;
		//stbuf->st_nlink = 1;
	} 
	else {printf("WFS_getattr：这个文件（目录）不存在，函数结束返回\n\n");;res = -ENOENT;}//文件不存在
	
	printf("WFS_getattr：getattr成功，函数结束返回\n\n");
	return res;
}

//创建文件
static int WFS_mknod (const char *path, mode_t mode, dev_t dev) {
    char path_dup[30]={0};
     if (path[strlen(path)-1]=='$')
    {
        memcpy(path_dup,path,strlen(path)-1);
    }else{
        memcpy(path_dup,path,strlen(path));
    }
	return create_file_dir(path_dup, 1);
}

//删除文件
static int WFS_unlink (const char *path) {
    char path_dup[30]={0};
     if (path[strlen(path)-1]=='$')
    {
        memcpy(path_dup,path,strlen(path)-1);
    }else{
        memcpy(path_dup,path,strlen(path));
    }
	return remove_file_dir(path_dup,1);
}


//打开文件时的操作
static int WFS_open(const char *path, struct fuse_file_info *fi) {

	return 0;
}

//读取文件时的操作
//根据路径path找到文件起始位置，再偏移offset长度开始读取size大小的数据到buf中，返回文件大小
//其中，buf用来存储从path读出来的文件信息，size为文件大小，offset为读取时候的偏移量，fi为fuse的文件信息
//步骤：① 先读取该path所指文件的file_directory；② 然后根据nStartBlock读出文件内容
static int WFS_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    char path_dup[30]={0};
     if (path[strlen(path)-1]=='$')
    {
        memcpy(path_dup,path,strlen(path)-1);
    }else{
        memcpy(path_dup,path,strlen(path));
    }
	printf("WFS_read：函数开始\n\n");
	struct file_directory attr;

	//读取该path所指对象的file_directory
	if (get_fd_to_attr(path_dup, &attr) == -1) 	{
		 printf("错误：WFS_read：get_fd_to_attr失败，函数结束返回\n\n");return -ENOENT;
	}
	//如果读取到的对象是目录，那么返回错误（只有文件会用到read这个函数）
	if (attr.flag == 2) 	{
		printf("错误：WFS_read：对象为目录不是文件，读取失败，函数结束返回\n\n");return -EISDIR;
	}

    time(&( attr.atime));


	struct data_block data_blk;
    size = size > (attr.fsize - sizeof(struct file_directory)) ? (attr.fsize - sizeof(struct file_directory)) : size;
    memset(&data_blk,0,sizeof(struct data_block));
	//根据文件信息读取文件内容
    uint32_t pos=offset/MAX_DATA_IN_BLOCK;
    uint32_t offset_in_block=offset%MAX_DATA_IN_BLOCK;
    uint32_t copy_num=0; 
    if(pos<12){
        while(copy_num<size&&pos<12){
            if(attr.sectors[pos]==0){
                printf("read no allocated block\n");
                return copy_num;
            }
            read_cpy_data_block(attr.sectors[pos],&data_blk);
            uint32_t num=(size-copy_num)>(MAX_DATA_IN_BLOCK-offset_in_block)?MAX_DATA_IN_BLOCK-offset_in_block:size-copy_num;
            memcpy(buf,(data_blk.data)+offset_in_block,num);
            buf+=num;
            copy_num+=num;
            pos++;
            offset_in_block=0;
            memset(&data_blk,0,sizeof(struct data_block));
        }

    }
    if(pos>=12 && copy_num<size){
        if(attr.sectors[12]==0){
           printf("read no allocated block\n");
           return copy_num;
        }
        read_cpy_data_block(attr.sectors[12],&data_blk);
        uint32_t * sector=((uint32_t*)data_blk.data);
        uint32_t index=pos-12;
        struct data_block  cpy_block;
        memset(&cpy_block,0,sizeof(struct data_block));
       
        while(copy_num<size){
            if(sector[index]==0){
                printf("read no allocated block\n");
                return copy_num;
            }
            read_cpy_data_block(sector[index],&cpy_block);
            uint32_t num=(size-copy_num)>(MAX_DATA_IN_BLOCK-offset_in_block)?MAX_DATA_IN_BLOCK-offset_in_block:size-copy_num;
            memcpy(buf,(cpy_block.data)+offset_in_block,num);
            buf+=num;
            copy_num+=num;
            index++;
            offset_in_block=0;
            memset(&cpy_block,0,sizeof(struct data_block));
        }
    }

    setattr(path,&attr,2);
	printf("WFS_read：文件读取成功，函数结束返回\n\n");
	return size;
}

//修改文件,将buf里大小为size的内容，写入path指定的起始块后的第offset
//步骤：① 找到path所指对象的file_directory；② 根据nStartBlock和offset将内容写入相应位置；
static int WFS_write (const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    char path_dup[30]={0};
     if (path[strlen(path)-1]=='$')
    {
        memcpy(path_dup,path,strlen(path)-1);
    }else{
        memcpy(path_dup,path,strlen(path));
    }
	printf("WFS_write：函数开始\n\n");
	struct file_directory attr;
	//打开path所指的对象，将其file_directory读到attr中
	get_fd_to_attr(path_dup, &attr);
	
    time(&(attr.mtime));
	//然后检查要写入数据的位置是否越界
	if (offset > attr.fsize) 	{
		printf("WFS_write：offset越界，函数结束返回\n\n");return -EFBIG;
	}
	struct data_block data_blk;
    memset(&data_blk,0,sizeof(struct data_block));

    uint32_t pos=offset/MAX_DATA_IN_BLOCK;
    uint32_t offset_in_block=offset%MAX_DATA_IN_BLOCK;
    uint32_t write_num=0,add_bytes=0; 
    if(pos<12){
        while(write_num<size&&pos<12){
            if(attr.sectors[pos]==0){
                if(get_empty_blk(1,&(attr.sectors[pos]))!=1){
                    printf("allocate one block failed\n");
                    return write_num;
                }

            }else{
                read_cpy_data_block(attr.sectors[pos],&data_blk);
            }
            uint32_t num=(size-write_num)>(MAX_DATA_IN_BLOCK-offset_in_block)?MAX_DATA_IN_BLOCK-offset_in_block:size-write_num;
            memcpy((data_blk.data)+offset_in_block,buf,num);
            if(offset_in_block+num>data_blk.size){
                add_bytes+=(offset_in_block+num)-data_blk.size;
                data_blk.size=offset_in_block+num;
            }
            offset_in_block=0;
            write_data_block(attr.sectors[pos],&data_blk);
            buf+=num;
            write_num+=num;
            pos++;
            memset(&data_blk,0,sizeof(struct data_block));               
        }
    }
    if(pos>=12 && write_num<size){
        if(attr.sectors[12]==0){
            if(get_empty_blk(1,&(attr.sectors[12]))!=1){
                    printf("allocate one block failed\n");
                    return write_num;
            }
            memset(&data_blk,0,sizeof(struct data_block));
        }else{
            read_cpy_data_block(attr.sectors[12],&data_blk);
        }
        uint32_t * sector=((uint32_t*)data_blk.data);
        uint32_t index=pos-12;
        struct data_block write_block;
        memset(&write_block,0,sizeof(struct data_block));
       
        while(write_num<size){
            if(sector[index]==0){
                 if(get_empty_blk(1,&(sector[index]))!=1){
                      printf("allocate one block failed\n");
                      return write_num;
                  }
                 data_blk.size+=sizeof(uint32_t);
                 write_data_block(attr.sectors[12],&data_blk);
            }else{
                 read_cpy_data_block(sector[index],&write_block);
            }
            uint32_t num=(size-write_num)>(MAX_DATA_IN_BLOCK-offset_in_block)?MAX_DATA_IN_BLOCK-offset_in_block:size-write_num;
            memcpy((write_block.data)+offset_in_block,buf,num);
            if(offset_in_block+num>write_block.size){
                add_bytes+=(offset_in_block+num)-write_block.size;
                write_block.size=offset_in_block+num;
            }
            write_data_block(sector[index],&write_block);
            buf+=num;
            write_num+=num;
            index++;
            offset_in_block=0;
            memset(&write_block,0,sizeof(struct data_block));
        }
    }
    attr.fsize+=add_bytes;
    setattr(path_dup,&attr,1);


    printf("WFS_write：文件写入成功，函数结束返回\n\n");
	return size;
}

//关闭文件
static int WFS_release (const char *, struct fuse_file_info *)
{
	return 0;
}

//创建目录
static int WFS_mkdir (const char *path, mode_t mode) {
    char path_dup[30]={0};
     if (path[strlen(path)-1]=='$')
    {
        memcpy(path_dup,path,strlen(path)-1);
    }else{
        memcpy(path_dup,path,strlen(path));
    }
    printf("create file \n");
	return create_file_dir(path_dup,2);
}

//删除目录
static int WFS_rmdir (const char *path) {
    char path_dup[30]={0};
     if (path[strlen(path)-1]=='$')
    {
        memcpy(path_dup,path,strlen(path)-1);
    }else{
        memcpy(path_dup,path,strlen(path));
    }
	return remove_file_dir(path_dup,2);
}

//进入目录
static int WFS_access(const char *path, int flag) {
    char path_dup[30]={0};
     if (path[strlen(path)-1]=='$')
    {
        memcpy(path_dup,path,strlen(path)-1);
    }else{
        memcpy(path_dup,path,strlen(path));
    }
    struct file_directory* attr=malloc(sizeof(struct file_directory));
	if(get_fd_to_attr(path_dup,attr)==-1)
	{
		free(attr);return -ENOENT;
	}
	if(attr->flag==1) {free(attr);return -ENOTDIR;}
	return 0;
}

//终端中ls -l读取目录的操作会使用到这个函数，因为fuse创建出来的文件系统是在用户空间上的
//这个函数用来读取目录，并将目录里面的文件名加入到buf里面
//步骤：① 读取path所对应目录的file_directory，找到该目录文件的nStartBlock；
//	   ② 读取nStartBlock里面的所有file_directory，并用filler把 (文件+后缀)/目录 名加入到buf里面
static int WFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi)//,enum use_readdir_flags flags)
{
    char path_dup[30]={0};
     if (path[strlen(path)-1]=='$')
    {
        memcpy(path_dup,path,strlen(path)-1);
    }else{
        memcpy(path_dup,path,strlen(path));
    }
    
	struct data_block data_blk;
	struct file_directory attr;
	if (get_fd_to_attr(path_dup, &attr) == -1) {//打开path指定的文件，将文件属性读到attr中
		return -ENOENT;
	}

	//如果该路径所指对象为文件，则返回错误信息
	if (attr.flag == 1) 	{   
		return -ENOENT;
	}

    uint32_t i,num;
    uint32_t  sectors[12+(MAX_DATA_IN_BLOCK/sizeof(uint32_t))]={0};
    if(attr.sectors[12]==0){
        num=12;
        for(i=0;i<12;i++){
            sectors[i]=attr.sectors[i];
        }
    }else{
        num=12+(MAX_DATA_IN_BLOCK/sizeof(uint32_t));
        for(i=0;i<12;i++){ 
            sectors[i]=attr.sectors[i];
        }
        if (read_cpy_data_block(attr.sectors[12], &data_blk)) 	{ 
	    	return -ENOENT;
    	}
        for(i=12;i<num;i++){
            sectors[i]=((uint32_t*)(data_blk.data))[i-12];
        }
    }
	//如果path所指对象是路径，那么读出该目录的数据块内容
	
	//无论是什么目录，先用filler函数添加 . 和 ..
	filler(buf, ".", NULL, 0, 0);
	filler(buf, "..", NULL, 0, 0);
    
	//按顺序查找,并向buf添加目录内的文件和目录名
    for(i=0;i<num;i++){
        if(sectors[i]==0)
             continue;
        memset(&data_blk,0,sizeof(struct data_block));
        if (read_cpy_data_block(sectors[i], &data_blk)) 	{ 
	    	return 0;
    	}
    	char name[MAX_FILENAME + MAX_EXTENSION + 2];//2是因为文件名和扩展名都有nul字符
        uint32_t offset=0;
    	while (offset+sizeof(struct file_directory) < MAX_DATA_IN_BLOCK) {
            struct file_directory * file_dir  = ((struct file_directory*)((data_blk.data)+offset));
            if(file_dir->flag==0){
	    	    offset += sizeof(struct file_directory);
                continue;
            }
    		strcpy(name, file_dir->fname);
            if (strlen(file_dir->fext) != 0){
		    	strcat(name, ".");
		    	strcat(name, file_dir->fext);
	    	}
	    	if (file_dir->flag != 0 && name[strlen(name) - 1] != '~' && filler(buf, name, NULL, 0, 0))  //将文件名添加到buf里面
	    		break;
	    	offset += sizeof(struct file_directory);
    	}
    }
   	return 0;
}

//所有文件的操作都要放到这里，fuse会帮我们在相应的linux操作中执行这些我们写好的函数
static struct fuse_operations WFS_oper = {
    .init       = WFS_init,//初始化
	.getattr	= WFS_getattr,//获取文件属性（包括目录的）

	.mknod      = WFS_mknod,//创建文件
    .unlink     = WFS_unlink,//删除文件
	.open		= WFS_open,//无论是read还是write文件，都要用到打开文件
	.read		= WFS_read,//读取文件内容
    .write      = WFS_write,//修改文件内容
//    .release    = WFS_release,//和open相对，关闭文件

    .mkdir      = WFS_mkdir,//创建目录
    .rmdir      = WFS_rmdir,//删除目录
    .access		= WFS_access,//进入目录
	.readdir	= WFS_readdir,//读取目录
};

int main(int argc, char *argv[]) {
	umask(0);
	return fuse_main(argc, argv, &WFS_oper, NULL);

}

/* 
  通过上述的分析可以知道，使用FUSE必须要自己实现对文件或目录的操作， 系统调用也会最终调用到用户自己实现的函数。
  用户实现的函数需要在结构体fuse_operations中注册。而在main()函数中，用户只需要调用fuse_main()函数就可以了，剩下的复杂工作可以交给FUSE。
*/
