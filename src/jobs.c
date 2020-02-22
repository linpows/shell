/*
 * job list functions
 */

#include "jobs.h"

/* gets job from jid */
struct esh_pipeline * get_job_from_jid(int jid, struct list job_list)
{
    struct list_elem * e;

    for (e  = list_begin(&job_list); e != list_end(&job_list); e = list_next(e))
    {
        struct esh_pipeline *pipeE= list_entry(e, struct esh_pipeline, elem);

        if (pipeE->jid == jid)
        {
            return pipeE;
        }
    }
    return NULL;
}

/* built-in jobs command */
void builtin_jobs(struct list job_list)
{
    int i = 1;
    char *status_strings[] = {"Foreground", "Running", "Stopped", "Needs Terminal"};
    if (!list_empty(&job_list))
    {
        struct list_elem * e;

        for (e = list_begin(&job_list); e != list_end(&pipe->commands); e = list_next(e))
        {
			struct esh_pipeline * job = list_entry(e, struct esh_pipeline, elem);
            printf("[%d]     %s",job->jid, status_strings[job->status]);
            print_job(job);
        }
    }
}

/* 
 * Gets job status by pid, stored in pipeline->status
 * removes job from list if terminated
 */
void job_status(pid_t pid, int status, struct list job_list)
{
	if (pid > 0) 
	{
		struct list_elem *e;
        for (e = list_begin(&job_list); e != list_end(&job_list); e = list_next(e)) 
        {
			struct esh_pipeline *pipeline = list_entry(e, struct esh_pipeline, elem);

			if (pipeline->pgrp == pid)
			{
				// process stopped
				if (WIFSTOPPED(status)) 
                {
                    pipeline->status = STOPPED;
                    printf("[%d]     Stopped     ", pipeline->jid);
                    print_job(pipeline);
                }
				// process killed
                if (WTERMSIG(status) == 9) 
                {
                    list_remove(e);
                }
                // process exited
                else if (WIFEXITED(status))
                {
                    list_remove(e);
                }
				// unhandled signal
                else if (WIFSIGNALED(status))
                {
                    list_remove(e);
                }
            }
        }
    }
    else if (pid < 0)
    {
        esh_sys_fatal_error("Error waiting for process");
    }
}

/* prints a job's commands */
void print_job(struct esh_pipeline *pipe)
{
    struct list_elem *e;
    printf("(");
    for (e = list_begin(&pipe->commands); e != list_end(&pipe->commands); e = list_next(e))
    {
        struct esh_command *cmd = list_entry(e, struct esh_command, elem);

        char **argv = cmd->argv;
        while (*argv) {
            printf("%s ", *argv);
            cmds++;
        }
        //if there is more than one command
        if (1 < list_size(&pipe->commands))
        {
            printf("| ");
        }
    }

    printf(")\n");
}

/* gets the job by the pgid */
struct esh_pipeline * get_job_from_pgid(pid_t pgrp, struct list job_list) 
{
    struct list_elem *e;
    for (e = list_begin(&job_list); e != list_end(&job_list); e = list_next(e))
    {
        struct esh_pipeline *job = list_entry(e, struct esh_pipeline, elem);
        if (job->pgrp == pgrp)
        {
            return job;
        }
    }

    return NULL;
}
