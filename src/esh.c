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
int jobNum;


/* adds a new job to the list*/
static int job_add_new(struct esh_pipeline *newJob) {
	newJob->jid = jobNum;
	
	list_push_front(job_list, &(newJob->elem));
	return newJob->jid;
}


/**
 * removes terminated and interrupted processes from pipeline
 * 
 */
static void 
child_status_change(pid_t child, int status, struct esh_pipeline *pipeline)
{
	//SIGSTOP CASE
	if (WIFSTOPPED(status)){
		
		pipeline->status = BACKGROUND;
		printf("\n");	
		//add to jobs list and print formatted output
		
		print_job(get_job_from_jid(job_add_new(pipeline)));
		/*
		printf("Process Stopped\n"); // ADD JOBS STOPPED PROCESS OUTPUT
		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~^^^
		*/
		
		
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
		
		list_remove(remove);
		
		if(list_empty(&pipeline->commands) && pipeline->status == BACKGROUND){
			//remove from jobs list 
			//MAY NEED TO UPDATE JOB NUMBERING
			list_remove(&pipeline->elem);
			
		}
		
		//struct esh_command *removed = list_entry(remove, struct esh_command, elem);
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
        child_status_change(child, status, (get_cmd_from_pid(child))->pipeline);
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
static struct esh_pipeline * esh_launch_background(struct esh_pipeline *pipe){
	struct list_elem *e;
	bool has_set_pipe_pgrp = false;
	
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
				perror("(background) process failed execution");
			}
			exit(EXIT_FAILURE);
		}
		////////////////////////////////////////////////////////////
		else if (c_pid < 0) {
			perror("(background) forking error");
		}
		else {
			//in parent
			if(has_set_pipe_pgrp == false){
				//initialize project group for entire pipeline
				pipe->pgrp = c_pid;
				has_set_pipe_pgrp = true;
			}
			
			if(setpgid(c_pid, pipe->pgrp) < 0){
				perror("(background) Parent Setting child process group error");
			}
			currCommand->pid = c_pid;
		}
	}
	
	return pipe;
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
	
	while(!list_empty(&rline->pipes)) 
	{
		e = list_pop_front(&rline->pipes);
		
		struct esh_pipeline *currPipe = 
			list_entry(e, struct esh_pipeline, elem);
		
		struct list_elem *c = list_begin(&currPipe->commands);
		struct esh_command *cmd = list_entry(c, struct esh_command, elem);
			
		if(is_builtin(cmd->arg[0]))
		{
			run_builtin(currPipe);
		}
		else if(!currPipe->bg_job) {
			//foreground jobs
			currPipe->status = FOREGROUND;
			esh_launch_foreground(currPipe);
			
			
			//give terminal back to shell
			give_terminal_to(esh_pgrp, eshState);
		}
		else {
			//BG job
			currPipe->status = BACKGROUND;
			struct esh_pipeline *toAdd = esh_launch_background(currPipe);
			job_add_new(toAdd);
		}
	}
	
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
	jobNum = 1;
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
	
    /* start SIGCHLD handler for background processes */
	esh_signal_sethandler(SIGCHLD, &sigchld_handler);
	
	/* initialize jobs list variables */
	job_list = malloc(sizeof(struct list));
	list_init(job_list);
	
	esh_signal_block(SIGCHLD);
	
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
        
        //esh_command_line_print(cline);
        esh_signal_unblock(SIGCHLD);
		esh_signal_block(SIGCHLD);
	
        esh_execute(cline);
        //esh_command_line_print(cline);
        //esh_command_line_free(cline);
        
    }
    return 0;
}

