/*
 * builtin functions
 */

#include "builtin.h"

/* checks if command is built in */
bool is_builtin(char* cmd)
{
    if (strncmp(cmd, "jobs", 4) == 0 || strncmp(cmd, "fg", 2) == 0 || strncmp(cmd, "bg", 2) == 0 || strncmp(cmd, "kill", 4) == 0 || strncmp(cmd, "stop", 4) == 0)
    {
        return true;
    }
    return false;
}

/* runs built in commands */
void run_builtin(struct esh_pipeline* pipe)
{
    struct list_elem * e = list_begin (&pipe->commands);
    struct esh_command *cmd = list_entry(e, struct esh_command, elem);

    if (strncmp(cmd->argv[0], "jobs", 4) == 0)
    {
        jobs_builtin();
    }
    else if (strncmp(cmd->argv[0], "fg", 2) == 0)
    {
        fg_builtin(atoi(cmd->argv[1]));
    }
    else if (strncmp(cmd->argv[0], "bg", 2) == 0)
    {
        bg_builtin(atoi(cmd->argv[1]));
    }
    else if (strncmp(cmd->argv[0], "kill", 4) == 0)
    {
        kill_builtin(atoi(cmd->argv[1]));
    }
    else if (strncmp(cmd->argv[0], "stop", 4) == 0)
    {
        stop_builtin(atoi(cmd->argv[1]));
    }
}

/* built-in fg command: fg <jid> */
void fg_builtin(int jobId)
{
    // get job
    struct esh_pipeline *pipe;
    pipe = get_job_from_jid(jobId);

	if (pipe != NULL)
    {
		//move to foreground
		pipe->status = FOREGROUND;
		pipe->bg_job = false;

		print_job(pipe);
		
		pipe->status = FOREGROUND;
		pipe->bg_job = false;
		
		// give term
		give_terminal_to(pipe->pgrp, &pipe->saved_tty_state);

		//continue if stopped
		if (pipe->status == STOPPED)
		{
			kill(pipe->pgrp, SIGCONT);
		}
		
		//wait
		esh_signal_block(SIGCHLD);
		wait_for_job(pipe);
		esh_signal_unblock(SIGCHLD);    
		give_terminal_to(esh_pgrp, eshState);
	}
}

/* built-in bg command: bg <jid>*/
void bg_builtin(int jobId)
{
    struct esh_pipeline *pipe;
    pipe = get_job_from_jid(jobId);

	if (pipe != NULL)
    {
		pipe->status = BACKGROUND;
		pipe->bg_job = true;
		kill(pipe->pgrp, SIGCONT);

		//Save state
		esh_sys_tty_save(&pipe->saved_tty_state);
		
		//print_job(pipe);
	}
}

/* built-in kill command: kill <jid>*/
void kill_builtin(int jobId)
{
    struct esh_pipeline *pipe;
    pipe = get_job_from_jid(jobId);
    if (pipe != NULL)
    {
        kill(pipe->pgrp, SIGKILL); //SIGTERM?
        //printf("Killed job: [%d]\n", pipe->jid);
    }
}

/* built-in stop command: stop <jid> */
void stop_builtin (int jobId)
{
    struct esh_pipeline *pipe;
    pipe = get_job_from_jid(jobId);

	if(pipe != NULL) 
	{
		kill(pipe->pgrp, SIGSTOP);
		pipe->status = STOPPED;
		char *status_strings[] = {"Foreground", "Running", "Stopped", "Needs Terminal"};
		printf("[%d] %s (",pipe->jid, status_strings[pipe->status]);
		print_job(pipe);
		printf(")\n");
	}
}
