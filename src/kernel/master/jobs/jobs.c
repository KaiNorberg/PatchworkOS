#include "jobs.h"

#include "schedule/schedule.h"
#include "load_balancer/load_balancer.h"
#include "time/time.h"

void jobs_init()
{
    schedule_job_init();
    load_balancer_init();
    time_job_init();
}