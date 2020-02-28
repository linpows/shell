/*
 * job list functions
 */

#include "jobs.h"



/* Return the list of current jobs */
struct list * get_jobs()
{
	return job_list;
}

/* Return job corresponding to jid */
struct esh_pipeline * get_job_from_jid(int jid)
{
    struct list_elem * e;

    for (e  = list_begin(job_list); e != list_end(job_list); e = list_next(e))
    {
        struct esh_pipeline *pipe= list_entry(e, struct esh_pipeline, elem);

        if (pipe->jid == jid)
        {
            return pipe;
        }
    }
    return NULL;
}

/* Return job corresponding to pgrp */
struct esh_pipeline * get_job_from_pgrp(pid_t pgrp) 
{
    struct list_elem *e;
    for (e = list_begin(job_list); e != list_end(job_list); e = list_next(e))
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
	if (!list_empty(job_list))
    {
        struct list_elem * e = list_begin(job_list);
        

        for (struct esh_pipeline * job = list_entry(e, struct esh_pipeline, elem); e != list_end(job_list); e = list_next(e))
        {
			job = list_entry(e, struct esh_pipeline, elem);
			struct list_elem * c = list_begin(&job->commands);
			
            
            for (struct esh_command * cmd = list_entry(c, struct esh_command, elem); c != list_end(&job->commands); c = list_next(c))
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
void jobs_builtin()
{
    char *status_strings[] = {"Foreground", "Running", "Stopped", "Needs Terminal"};
    if (!list_empty(job_list))
    {
        struct list_elem * e = list_rbegin(job_list);
        

        for (struct esh_pipeline * job = list_entry(e, struct esh_pipeline, elem); e != list_rend(job_list); e = list_prev(e))
        {
			job = list_entry(e, struct esh_pipeline, elem);
            printf("[%d] %s (",job->jid, status_strings[job->status]);
            print_job(job);
            printf(")\n");
        }
    }
}

/* prints a job's commands */
void print_job(struct esh_pipeline *pipe)
{
    struct list_elem *e;
    int numCommands = list_size(&pipe->commands);
    for (e = list_begin(&pipe->commands); e != list_end(&pipe->commands); e = list_next(e))
    {
        struct esh_command *cmd = list_entry(e, struct esh_command, elem);

        char **argv = cmd->argv;
        printf("%s", *argv);
        argv++;
        while (*argv) {
            printf(" %s", *argv);
            argv++;
        }
        //if there is more than one command
        if (1 < numCommands--)
        {
            printf(" | ");
        }
    }
    
    if(pipe->bg_job) 
    {
		printf(" &");
	}
}
