/*************************************************************************
    > File Name: open.c
    > Author: ma6174
    > Mail: ma6174@163.com 
    > Created Time: 2018年08月04日 星期六 06时57分22秒
 ************************************************************************/

#include<stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

int igh_ec_test_performance_fun(void)
{
        int user_time = 0, gfar_time = 0, gen_time = 0;
        int result_user_gen = 0, result_user_gfar = 0, result_gen_gfar = 0,
            sum_user_gen = 0, sum_user_gfar = 0, sum_gen_gfar = 0,
            count = 0;
        FILE * gfar_file = NULL, * user_file = NULL, * gen_file = NULL;
        char gfar_file_str[8], user_file_str[8], gen_file_str[8];

        user_file = fopen ("./igh_user_time.log" , "r");
        gfar_file = fopen ("./igh_gfar_time.log" , "r");
        gen_file = fopen ("./igh_gen_time.log" , "r");


        if (gfar_file == NULL) {
                printf("open gfar_time file error!\n");
                return -1;
        }
        if (user_file == NULL) {
                printf("open user_time file error!\n");
                return -1;
        }
        if (gen_file == NULL) {
                printf("open gen_file file error!\n");
                return -1;
        }

        while (fgets(user_file_str, sizeof(gfar_file_str), user_file) != NULL &&
                fgets(gfar_file_str, sizeof(gfar_file_str), gfar_file) != NULL &&
                fgets(gen_file_str, sizeof(gen_file_str), gen_file) != NULL) {

                sscanf(user_file_str, "%d", &user_time);
                sscanf(gfar_file_str, "%d", &gfar_time);
                sscanf(gen_file_str, "%d", &gen_time);

                result_user_gen = gen_time - user_time;
                result_user_gfar = gfar_time - user_time;
                result_gen_gfar = gfar_time - gen_time;

                sum_user_gen += result_user_gen;
                sum_user_gfar += result_user_gfar;
                sum_gen_gfar += result_gen_gfar;
                if (result_user_gfar > 0 && result_user_gfar < 50)
                        count++;
        }
        printf("Total test times:%d.Performance average:\n", count);
        printf("user_gen    : %d\n", sum_user_gen / count);
        printf("gen_gfar    : %d\n", sum_gen_gfar / count);
        printf("user_gfar   : %d\n", sum_user_gfar / count);

        fclose (gfar_file);
        fclose (user_file);
        return 0;
}


int main()
{
#if 1
    igh_ec_test_performance_fun();
#endif
#if 0

    int fd, size;
    int i = 0;
    char s[] = "Linux Programmer!\n", buffer[80];
    char str[8];
    fd = open("./temp.txt", O_RDWR | O_CREAT | O_TRUNC );
    for(i = 0; i < 10; i++){
        //memset(str, 0, sizeof(str));
        //sprintf(str, "%d\n", i);
        //write(fd, str, strlen(str));
        memset(str, 0, sizeof(str));
        sprintf(str, "%d\n", i);
        printf("---------------------:%d\n", strlen(str));
    }
    close(fd);
#endif

#if 0

    struct file *fp;
    mm_segment_t fs;
    loff_t pos;
    char buf[8];
    printk("hello enter\n");
    fp = filp_open("/root/time.log", O_RDWR | O_CREAT, 0644);
    if (IS_ERR(fp)) {
        printk("create file error\n");
        return -1;
    }
    fs = get_fs();
    set_fs(KERNEL_DS);
    pos = 0;
    for(i = 0; i < 10; i++){
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%d\n", i);
        vfs_write(fp, buf, sizeof(buf), &pos);
        pos+=strlen(buf);
    }
    filp_close(fp, NULL);
    set_fs(fs);
#endif
#if 0
    char buf[8];
    int len = 0;
    int a = 1;
    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%d\n", a);
    len = strlen(buf);
    printf("----------:%d\n", len);
    return 0;
#endif
}


