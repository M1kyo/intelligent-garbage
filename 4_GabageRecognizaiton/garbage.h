//garbage.h
 #ifndef __GARBAGE__H
 #define __GARBAGE__H

 void garbage_init(void);
 void garbage_final(void);
 char *garbage_category(char *category);
 
 //增加拍照指令和照片路径宏定义
#define WGET_CMD "wget http://127.0.0.1:8080/?action=snapshot -O /tmp/garbage.jpg"
#define GARBAGE_FILE "/tmp/garbage.jpg"

#endif