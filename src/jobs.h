#ifndef __JOBS_H
#define __JOBS_H

/*
 * job list functions
 */
#include <assert.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "list.h"
#include "esh.h"
#include "esh-sys-utils.h"

/* gets job from jid */
struct esh_pipeline * get_job_from_jid(int jid, struct list job_list);
 
/* built-in jobs command */
void builtin_jobs(struct list job_list);

/* 
 * Gets job status by pid, stored in pipeline->status
 * removes job from list if terminated
 */
void job_status(pid_t pid, int status, struct list job_list);

/* prints a job's commands */
void print_job(struct esh_pipeline *pipe);

/* gets the job by the pgid */
struct esh_pipeline * get_job_from_pgid(pid_t pgrp, struct list job_list);

#endif //__JOBS_H
