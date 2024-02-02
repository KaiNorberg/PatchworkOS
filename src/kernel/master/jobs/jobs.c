#include "jobs.h"

#include "schedule/schedule.h"
#include "task_balancer/task_balancer.h"
#include "time/time.h"

void jobs_init()
{
    schedule_job_init();
    task_balancer_init();
    time_job_init();
}