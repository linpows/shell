/*
 * job list functions
 */

#include "jobs.h"

struct list job_list;

/* Return the list of current jobs */
struct list * get_jobs()
{
	return &job_list;
}

/* Return job corresponding to jid */
struct esh_pipeline * get_job_from_jid(int jid)
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

/* Return job corresponding to pgrp */
struct esh_pipeline * get_job_from_pgrp(pid_t pgrp) 
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

/* Return process corresponding to pid */
struct esh_command * get_cmd_from_pid(pid_t pid){
	if (!list_empty(&job_list))
    {
        struct list_elem * e = list_begin(&job_list);
        struct esh_pipeline * job;

        for (job = list_entry(e, struct esh_pipeline, elem); e != list_end(&job_list); e = list_next(e))
        {
			job = list_entry(e, struct esh_pipeline, elem);
			struct list_elem * c = list_begin(&job->commands);
			struct esh_command * cmd;
            
            for (cmd = list_entry(c, struct esh_command, elem); c != list_end(&job->commands); c = list_next(c))
            {
				cmd = list_entry(c, struct esh_command, elem);
				if (cmd->pid == pid)
				{
					return cmd;
				}
			}
        }
    }
	
	return NULL;
}

/* built-in jobs command */
void builtin_jobs()
{
    char *status_strings[] = {"Foreground", "Running", "Stopped", "Needs Terminal"};
    if (!list_empty(&job_list))
    {
        struct list_elem * e = list_begin(&job_list);
        struct esh_pipeline * job;

        for (job = list_entry(e, struct esh_pipeline, elem); e != list_end(&job_list); e = list_next(e))
        {
			job = list_entry(e, struct esh_pipeline, elem);
            printf("[%d]     %s",job->jid, status_strings[job->status]);
            print_job(job);
        }
    }
}

/* 
 * Gets job status by pid, stored in pipeline->status
 * removes job from list if terminated
 */
void job_status(pid_t pid, int status)
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
            argv++;
        }
        //if there is more than one command
        if (1 < list_size(&pipe->commands))
        {
            printf("| ");
        }
    }

    printf(")\n");
}
