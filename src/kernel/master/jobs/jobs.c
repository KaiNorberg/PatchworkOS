#include "jobs.h"

#include "schedule/schedule.h"
#include "time/time.h"

void jobs_init()
{
    schedule_job_init();
    time_job_init();
}