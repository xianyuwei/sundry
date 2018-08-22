#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>


int AAA(){
    struct timeval t_val;
    gettimeofday(&t_val, NULL);
    printf("start, now, sec=%ld m_sec=%ld \n", t_val.tv_sec, t_val.tv_usec);

    long sec = t_val.tv_sec;
    time_t t_sec = (time_t)sec;
    printf("date:%s", ctime(&t_sec));

    struct timeval t_val_end;
    gettimeofday(&t_val_end, NULL);

    struct timeval t_result;
    timersub(&t_val_end, &t_val, &t_result);
    double consume = t_result.tv_sec + (1.0 * t_result.tv_usec)/1000000;
    printf("end.elapsed time= %fs \n", consume);
}

/****************************************************************************/

static int sig_alarms = 0;
static unsigned int user_alarms = 0;
void signal_handler(int signum) {
    switch (signum) {
        case SIGALRM:
            sig_alarms++;
            break;
    }
}
struct timeval t_val;
struct timeval t_val_end;

struct timeval t_result;

int xxxx(int argc, char **argv)
{
    struct sigaction sa;
    struct itimerval tv;

    
    struct timeval t_val;
    struct timeval t_val_end;

    struct timeval t_result;


    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGALRM, &sa, 0)) {
        printf("Failed to install signal handler!\n");
        return -1;
    }

    printf("Starting timer...\n");
    tv.it_value.tv_sec = 0;
    tv.it_value.tv_usec = 100000;
    tv.it_interval.tv_sec = 0;
    tv.it_interval.tv_usec = 50000;
    
    if (setitimer(ITIMER_REAL, &tv, NULL)) {
        fprintf(stderr, "Failed to start timer: %s\n", strerror(errno));
        return 1;
    }

    printf("Started.\n");
    while (1) {
        /*gettimeofday(&t_val, NULL);
        pause();
        gettimeofday(&t_val_end, NULL);
        timersub(&t_val_end, &t_val, &t_result);
        double consume = t_result.tv_sec + (1.0 * t_result.tv_usec)/1000000;
        printf("end.elapsed time= %ld \n", t_result.tv_usec - 500000);*/
        
        pause();
        printf("-----------:%d\n", user_alarms);
        //while (sig_alarms != user_alarms) {
            testFunc();
            user_alarms++;
        //}
    }

    return 0;
}

static int flag = 0;

int testFunc(){
    struct timeval t_val_end;
    gettimeofday(&t_val_end, NULL);
    timersub(&t_val_end, &t_val, &t_result);
    double consume = t_result.tv_sec + (1.0 * t_result.tv_usec)/1000000;
    printf("end.elapsed time= %ld \n", t_result.tv_usec - 50000);
    //usleep(40);
    gettimeofday(&t_val, NULL);
    //usleep(200);
}
int main(){

    while(1){
        gettimeofday(&t_val_end, NULL);
        timersub(&t_val_end, &t_val, &t_result);
        double consume = t_result.tv_sec + (1.0 * t_result.tv_usec)/1000000;
        printf("end.elapsed time= %ld \n", t_result.tv_usec);
        usleep(100);
        gettimeofday(&t_val, NULL);
        //usleep(100);
    }


}

/*****************************************************************************/
// Application parameters
#define FREQUENCY 1000
#define CLOCK_TO_USE CLOCK_REALTIME
#define MEASURE_TIMING

/****************************************************************************/

#define NSEC_PER_SEC (1000000000L)
#define PERIOD_NS 50000//(NSEC_PER_SEC / FREQUENCY)

#define DIFF_NS(A, B) (((B).tv_sec - (A).tv_sec) * NSEC_PER_SEC + \
	(B).tv_nsec - (A).tv_nsec)

#define TIMESPEC2NS(T) ((uint64_t) (T).tv_sec * NSEC_PER_SEC + (T).tv_nsec)

/*****************************************************************************/
static unsigned int counter = 0;
static unsigned int blink = 0;
const struct timespec cycletime = {0, PERIOD_NS};

struct timespec timespec_add(struct timespec time1, struct timespec time2)
{
	struct timespec result;

	if ((time1.tv_nsec + time2.tv_nsec) >= NSEC_PER_SEC) {
		result.tv_sec = time1.tv_sec + time2.tv_sec + 1;
		result.tv_nsec = time1.tv_nsec + time2.tv_nsec - NSEC_PER_SEC;
	} else {
		result.tv_sec = time1.tv_sec + time2.tv_sec;
		result.tv_nsec = time1.tv_nsec + time2.tv_nsec;
	}

	return result;
}

struct timespec start, end;
void cyclic_task(){
    struct timespec wakeupTime, time;
#ifdef MEASURE_TIMING
    struct timespec startTime, endTime, lastStartTime = {};
    unsigned int period_ns = 0, exec_ns = 0, latency_ns = 0;
#endif

    // get current time
    clock_gettime(CLOCK_TO_USE, &wakeupTime);
    int i = 0;

	while(1) {
        //printf("----------:%d\n", i++);
        gettimeofday(&t_val, NULL);
        timersub(&t_val_end, &t_val, &t_result);
        double consume = t_result.tv_sec + (1.0 * t_result.tv_usec)/1000000;
        printf("end.elapsed time= %ld \n", t_result.tv_usec);
        
        clock_gettime(CLOCK_TO_USE, &end);
        latency_ns = DIFF_NS(start, end);
        printf("latency    %10u\n", latency_ns);
        
		wakeupTime = timespec_add(wakeupTime, cycletime);
        clock_nanosleep(CLOCK_TO_USE, TIMER_ABSTIME, &wakeupTime, NULL);

#if 1
        clock_gettime(CLOCK_TO_USE, &startTime);
        latency_ns = DIFF_NS(wakeupTime, startTime);
        period_ns = DIFF_NS(lastStartTime, startTime);
        exec_ns = DIFF_NS(lastStartTime, endTime);
        lastStartTime = startTime;
        
        printf("period     %10u\n", period_ns);
        printf("exec       %10u\n", exec_ns);
        printf("latency    %10u\n", latency_ns);
        puts("-----------------------------------------------------");
#endif


        //usleep(300000);
        clock_gettime(CLOCK_TO_USE, &endTime);
        clock_gettime(CLOCK_TO_USE, &start);
        //usleep(300);
        //usleep(1);
        gettimeofday(&t_val_end, NULL);
	}
}

int eee(){
    cyclic_task();
}

