/*
 * esh - the 'pluggable' shell.
 *
 * Developed by Godmar Back for CS 3214 Fall 2009
 * Virginia Tech.
 */

#include "esh.h"

/* saves startup state to return to*/
pid_t esh_pgrp;
struct termios* eshState;

/**
 * removes terminated and interrupted processes from pipeline
 * 
 */
static void child_status_change(pid_t child, int status, struct esh_pipeline *pipeline)
{
	
	//SIGSTOP CASE
	if (WIFSTOPPED(status)){
		pipeline->status = BACKGROUND;
		printf("\n");	
		printf("Process Stopped\n"); // ADD JOBS STOPPED PROCESS OUTPUT
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^^^
		//add to jobs list 
	}
	//SIGCHLD and SIGINT case
	else {
		if (WIFSIGNALED(status)){
			printf("\n");
		}
		struct list_elem *e;
		struct list_elem *remove;
		
		e = list_begin(&(pipeline->commands)); // starting position
		while (e != list_end (&(pipeline->commands))){
			// obtains struct using pointer arithmetic
			struct esh_command *curr = list_entry (e, struct esh_command, elem);
			if(curr->pid == child) {
				remove = &curr->elem;
				break;
			}
			e = list_next(e); // increment condition
		}
		
		// remove terminated child command
		//struct esh_command *removed = list_entry(remove, struct esh_command, elem);
		list_remove(remove);
		//free(removed);
	}
}


/**
 * SIGCHLD handler. FOR BACKGROUND PROCESSES
 * Call waitpid() to learn about any child processes that
 * have exited or changed status (been stopped, needed the
 * terminal, etc.)
 * Just record the information by updating the job list
 * data structures.  Since the call may be spurious (e.g.
 * an already pending SIGCHLD is delivered even though
 * a foreground process was already reaped), ignore when
 * waitpid returns -1.
 * Use a loop with WNOHANG since only a single SIGCHLD
 * signal may be delivered for multiple children that have
 * exited.
 */
static void sigchld_handler(int sig, siginfo_t *info, void *_ctxt)
{
    pid_t child;
    int status;

    assert(sig == SIGCHLD);

    while ((child = waitpid(-1, &status, WUNTRACED|WNOHANG)) > 0) {
		esh_signal_block(SIGCHLD);
        child_status_change(child, status, get_cmd_from_pid(child)->pipeline);
        esh_signal_unblock(SIGCHLD);
    }
}

/** Wait for all processes in this pipeline to complete, or for
 * the pipeline's process group to no longer be the foreground
 * process group.
 * You should call this function from a) where you wait for
 * jobs started without the &; and b) where you implement the
 * 'fg' command.
 *
 * Implement child_status_change such that it records the
 * information obtained from waitpid() for pid 'child.'
 * If a child has exited or terminated (but not stopped!)
 * it should be removed from the list of commands of its
 * pipeline data structure so that an empty list is obtained
 * if all processes that are part of a pipeline have
 * terminated.  If you use a different approach to keep
 * track of commands, adjust the code accordingly.
 */
static void wait_for_job(struct esh_pipeline *pipeline)
{
    assert(esh_signal_is_blocked(SIGCHLD));

    while (pipeline->status == FOREGROUND && !list_empty(&pipeline->commands)) {
        int status;

        pid_t child = waitpid(-1, &status, WUNTRACED);
        if (child != -1) {
            child_status_change(child, status, pipeline);
		}
    }
    if(pipeline->status == FOREGROUND){
		//not stopped, free pipeline
		free(pipeline);
	}
}


/**
 * Assign ownership of the terminal to process group
 * pgrp, restoring its terminal state if provided.
 *
 * Before printing a new prompt, the shell should
 * invoke this function with its own process group
 * id (obtained on startup via getpgrp()) and a
 * sane terminal state (obtained on startup via
 * esh_sys_tty_init()).
 */
static void give_terminal_to(pid_t pgrp, struct termios *pg_tty_state)
{
    esh_signal_block(SIGTTOU);
    int rc = tcsetpgrp(esh_sys_tty_getfd(), pgrp);
    if (rc == -1)
        esh_sys_fatal_error("tcsetpgrp: ");

    if (pg_tty_state)
        esh_sys_tty_restore(pg_tty_state);
    esh_signal_unblock(SIGTTOU);
}

/**
 * function to handle background execution
 */
static int esh_launch_background(struct esh_pipeline *pipe){
	struct list_elem *e;
	bool has_set_pipe_pgrp = false;
	//set status
	
	//create a terminal object for pipeline if it has not already been made
	if(!pipe->pgrpset) {
		esh_sys_tty_save((& pipe->saved_tty_state));
	}
	
	
	pid_t c_pid;
	for (e = list_begin(&pipe->commands); e != list_end(&pipe->commands); e = list_next(e)) 
	{
		
		struct esh_command *currCommand = list_entry (e, struct esh_command, elem);
		
		if((c_pid = fork()) == 0) {
			//in child
			
			//ID setup
			currCommand->pid = getpid();
			if(has_set_pipe_pgrp == false){
				//initialize project group for entire pipeline
				pipe->pgrp = currCommand->pid;
			}
			
			//set individual process groups
			if(setpgid(0, pipe->pgrp) < 0){
				perror("(background) child setting process group Error");
			}
			
			//execute here
			esh_signal_unblock(SIGCHLD);
			if (execvp(currCommand->argv[0], currCommand->argv) == -1) {
				perror("background process failed execution");
			}
			exit(EXIT_FAILURE);
		}
		////////////////////////////////////////////////////////////
		else if (c_pid < 0) {
			perror("forking error");
		}
		else {
			//in parent
			if(has_set_pipe_pgrp == false){
				//initialize project group for entire pipeline
				pipe->pgrp = c_pid;
				has_set_pipe_pgrp = true;
			}
			
			if(setpgid(c_pid, pipe->pgrp) < 0){
				perror("Parent Setting child process group error");
			}
			
			currCommand->pid = c_pid;
		}
	}
	//ADD TO JOBS LIST HERE~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	return 0;
}

/**
 * function to handle foreground execution
 */
static int esh_launch_foreground(struct esh_pipeline *pipe){
	struct list_elem *e;
	
	//create a terminal object for pipeline;
	esh_sys_tty_save((& pipe->saved_tty_state));
	bool has_set_pipe_pgrp = false;
	
	pid_t c_pid;
	for (e = list_begin(&pipe->commands); e != list_end(&pipe->commands); e = list_next(e)) 
	{
		
		struct esh_command *currCommand = list_entry (e, struct esh_command, elem);
		
		if((c_pid = fork()) == 0) {
			//in child
			
			//ID setup
			currCommand->pid = getpid();
			if(has_set_pipe_pgrp == false){
				//initialize project group for entire pipeline
				pipe->pgrp = currCommand->pid;
				give_terminal_to(pipe->pgrp, (& pipe->saved_tty_state));
			}
			
			//set individual process groups
			if(setpgid(0, pipe->pgrp) < 0){
				perror("child setting process group Error");
			}
			
			//execute here
			esh_signal_unblock(SIGCHLD);
			if (execvp(currCommand->argv[0], currCommand->argv) == -1) {
				perror("process failed execution");
			}
			exit(EXIT_FAILURE);
		}
		////////////////////////////////////////////////////////////
		else if (c_pid < 0) {
			perror("forking error");
		}
		else {
			//in parent
			if(has_set_pipe_pgrp == false){
				//initialize project group for entire pipeline
				pipe->pgrp = c_pid;
				has_set_pipe_pgrp = true;
			}
			
			if(setpgid(c_pid, pipe->pgrp) < 0){
				perror("Parent Setting child process group error");
			}
			
			currCommand->pid = c_pid;
			
			//printf("%d : pipe group id\n%d esh group id:\n", c_pid, getpgrp());
			
		}
	}
	give_terminal_to(c_pid, (& pipe->saved_tty_state));
	//wait for all forked jobs and update child status
	wait_for_job(pipe);
	
	
	return 0;
}

/**
 * this function is used to determine if new process is foreground,
 * background, or built in and the calls the corresponding function
 * 
 */
static int esh_execute(struct esh_command_line *rline){
	struct list_elem *e;
	esh_signal_block(SIGCHLD);
	//### IMPLEMENT JOB ID HANDLING HERE
	for (e = list_begin(&rline->pipes); e != list_end(&rline->pipes); e = list_next(e)) 
	{
		struct esh_pipeline *currPipe = list_entry (e, struct esh_pipeline, elem);
		if(!currPipe->bg_job) {
			//foreground jobs
			currPipe->status = FOREGROUND;
			esh_launch_foreground(currPipe);
			
			
		}
		else {
			//BG job
			currPipe->status = BACKGROUND;
			esh_launch_background(currPipe);
		}
	}
	esh_signal_unblock(SIGCHLD);
	return 0;
}


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
static void
usage(char *progname)
{
    printf("Usage: %s -h\n"
        " -h            print this help\n"
        " -p  plugindir directory from which to load plug-ins\n",
        progname);

    exit(EXIT_SUCCESS);
}

/* Build a prompt by assembling fragments from loaded plugins that 
 * implement 'make_prompt.'
 *
 * This function demonstrates how to iterate over all loaded plugins.
 */
static char *
build_prompt_from_plugins(void)
{
    char *prompt = NULL;

    for (struct list_elem * e = list_begin(&esh_plugin_list);
         e != list_end(&esh_plugin_list); e = list_next(e)) {
        struct esh_plugin *plugin = list_entry(e, struct esh_plugin, elem);

        if (plugin->make_prompt == NULL)
            continue;

        /* append prompt fragment created by plug-in */
        char * p = plugin->make_prompt();
        if (prompt == NULL) {
            prompt = p;
        } else {
            prompt = realloc(prompt, strlen(prompt) + strlen(p) + 1);
            strcat(prompt, p);
            free(p);
        }
    }

    /* default prompt */
    if (prompt == NULL)
        prompt = strdup("esh> ");

    return prompt;
}

/* The shell object plugins use.
 * Some methods are set to defaults.
 */
struct esh_shell shell =
{
    .build_prompt = build_prompt_from_plugins,
    .readline = readline,       /* GNU readline(3) */ 
    .parse_command_line = esh_parse_command_line /* Default parser */
};


//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
/**~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * ||||||||||||||||||||||||||||||MAIN|||||||||||||||||||||||||||||||||
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/
//@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
int main(int ac, char *av[]) {
    int opt;
    list_init(&esh_plugin_list);

    /* Process command-line arguments. See getopt(3) */
    while ((opt = getopt(ac, av, "hp:")) > 0) {
        switch (opt) {
        case 'h':
            usage(av[0]);
            break;

        case 'p':
            esh_plugin_load_from_directory(optarg);
            break;
        }
    }

    esh_plugin_initialize(&shell);
    
    /* saves startup state to return to*/
	esh_pgrp = getpgrp();
	eshState = esh_sys_tty_init();
    
	esh_signal_sethandler(SIGCHLD, &sigchld_handler);
	
    /* Read/eval loop. */
    for (;;) {
        /* Do not output a prompt unless shell's stdin is a terminal */
        char * prompt = isatty(0) ? shell.build_prompt() : NULL;
        char * cmdline = shell.readline(prompt);
        free (prompt);

        if (cmdline == NULL)  /* User typed EOF */
            break;

        struct esh_command_line * cline = shell.parse_command_line(cmdline);
        free (cmdline);
        if (cline == NULL)                  /* Error in command line */
            continue;

        if (list_empty(&cline->pipes)) {    /* User hit enter */
            esh_command_line_free(cline);
            continue;
        }
        
        esh_execute(cline);
        give_terminal_to(esh_pgrp, eshState);
        //esh_command_line_print(cline);
        //esh_command_line_free(cline);
        
        
    }
    return 0;
}

