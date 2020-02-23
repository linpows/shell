#ifndef __JOBS_H
#define __JOBS_H

/*
 * job list functions
 */
 
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "list.h"
#include "esh.h"

/* Return the list of current jobs */
struct list * get_jobs();

/* Return job corresponding to jid */
struct esh_pipeline * get_job_from_jid(int jid);

/* Return job corresponding to pgrp */
struct esh_pipeline * get_job_from_pgid(pid_t pgrp);

/* Return process corresponding to pid */
struct esh_command * get_cmd_from_pid(pid_t pid);
 
/* built-in jobs command */
void builtin_jobs();

/* 
 * Gets job status by pid, stored in pipeline->status
 * removes job from list if terminated
 */
void job_status(pid_t pid, int status);

/* prints a job's commands */
void print_job(struct esh_pipeline *pipe);

#endif //__JOBS_H
