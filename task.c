#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<pthread.h>
#include<unistd.h>

#define POPEN_CMD "cat /tmp/ctl.txt"
static int g_ctl_flag = 0;

void popen_result(FILE *fp){
    char buf[16] = {0};
    if(!fp) return;

    memset(buf, 0, sizeof(buf));
    while(fgets(buf, sizeof(buf) - 1, fp) != 0){
        if(buf == NULL){
            g_ctl_flag = 0;
            continue;
        }
        if(strncmp(buf, "start", strlen("start")) == 0){
            g_ctl_flag = 1;
        }else{
            g_ctl_flag = 0;
        }
    }
}

void *ctl_thread(void *arg)
{
    FILE * fp = NULL;
    
    while(1){
        usleep(100);
        fp = popen(POPEN_CMD, "r");
        if(!fp){
            printf("popen error!\n");
        }
        popen_result(fp);
        pclose(fp);
    }
    return NULL;
}

void *beckhoff_thread(void *arg)
{
    int i = 0;
    while(1){
        while(g_ctl_flag){
            sleep(1);
            printf("=====================================:%d\n", i++);
        }
	}
    return NULL;
}

int main(){
    int ret = 0;
    printf("=====================================:%ld\n", sizeof(short));
    
    
    pthread_t ctl_t,beckhoff_t;
    ret = pthread_create(&ctl_t, NULL, &ctl_thread, NULL);
    if(ret != 0){
        printf("ctl_thread create error!\n");
    }
    ret = pthread_create(&beckhoff_t, NULL, &beckhoff_thread, NULL);
    if(ret != 0){
        printf("beckhoff_thread create error!\n");
    }

    pthread_join(ctl_t, NULL);
    if(ret != 0){
        printf("ctl_thread pthread_join error!\n");
    }
    pthread_join(beckhoff_t, NULL);
    if(ret != 0){
        printf("beckhoff_thread pthread_join error!\n");
    }
}

