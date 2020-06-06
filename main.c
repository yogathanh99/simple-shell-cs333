/**********************************************************************************************************************************************************
 * Simple UNIX Shell
 * @author: 1751019-1751024-1751094 
 **/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>   // waitpid()
#include <string.h>     
#include <fcntl.h>      // open(), creat(), close()
using namespace std;

#define MAX_LENGTH 80 // The maximum length of the commands
#define BUF_LENGTH MAX_LENGTH/2 + 1
#define REDIR_LENGTH 2 
#define PIPE_LENGTH 3

void parse_command(char command[], char* args[], int* wait){
	for (int i=0; i<BUF_LENGTH; i++) args[i]=NULL;

	if (command[strlen(command)-1]=='&'){
		*wait=1;
		command[strlen(command)-1]='\0';
	}
	else *wait=0;

	const char *space=" ";
	int i=0;
	char *token=strtok(command, space);
	while(token){
		args[i++]=token;
		token=strtok(NULL,space);
	}

	args[i]=NULL;
}

void parent(pid_t child_pid, int wait){
	int status=0;
   	printf("Parent spawned a child <%d>\n",child_pid);
   	switch (wait) {
      	// Parent and child are running concurrent
    	case 0: {
    		waitpid(child_pid, &status, 0);
        	break;
      	}
      	// Parent waits a child process
      	default: {
        	waitpid(child_pid, &status, WUNTRACED);
         	if (WIFEXITED(status)) {   
            	printf("Child exited with status = %d\n", status);
         	}
         	break;
      	}
	}
}

void child(char* args[], char* dir_val[]) {
	int value_in, value_out;
	if (dir_val[0]){
		if (strcmp(dir_val[0],">")==0){
			value_out=creat(dir_val[1],S_IRWXU);
			if (value_out==-1){
				perror("Output failed!!");
				exit(EXIT_FAILURE);
			}

			dup2(value_out, STDOUT_FILENO);

			if (close(value_out)==-1){
				perror("Clossing output failed!!");
				exit(EXIT_FAILURE);
			}
		}

		else if (strcmp(dir_val[0], "<")==0){
			value_in=open(dir_val[1], O_RDONLY);
			if (value_in==-1){
				perror("Input failed!!");
				exit(EXIT_FAILURE);
			}

			dup2(value_in, STDIN_FILENO);

			if (close(value_in)==-1){
				perror("Clossing input failed!!");
				exit(EXIT_FAILURE);
			}
		}
	}	
   	// Execute user command in child process
   	if (execvp(args[0], args) == -1) {
     	perror("Fail to execute this command");
     	exit(EXIT_FAILURE);
  	}
}

void parse_dir(char* args[], char* dir_val[]){
	unsigned i=0;
	dir_val[0]=NULL;
	dir_val[1]=NULL;

	while (args[i]){
		if (strcmp(args[i],"<")== 0 || strcmp(args[i],">")==0){
			if (args[i+1]){
				dir_val[0]=strdup(args[i]);
				dir_val[1]=strdup(args[i+1]);
				args[i]=NULL;
				args[i+1]=NULL;
			}
		}
		i++;
	}
}

bool parse_pipe(char* args[], char *child1_args[], char *child2_args[]){
	unsigned i=0, split_i=0;
	bool  c_pipe=false;

	while (args[i]){
		if (strcmp(args[i],"|")==0)	{
			split_i=i;
			c_pipe=true;
		}
		i++;
	}

	if (!c_pipe) return false;
	

	for (i=0; i<split_i; i++){
		child1_args[i]=strdup(args[i]);
	}
	child1_args[i++]=NULL;

	// Copy arguments after split pipe position to child02_argv[]
   	while (args[i]) {
      child2_args[i - split_i - 1] = strdup(args[i]);
      i++;
   }
   child2_args[i - split_i - 1] = NULL;
   
   return true;	
}

void run_pipe(char* child1_args[], char* child2_args[]){
	int piped[2];

	if (pipe(piped)==-1){
		perror("Pipe() failed");
		exit(EXIT_FAILURE);
	}

	// Fork 1st command
	if (fork()==0){
      	dup2(piped[1], STDOUT_FILENO);       
      	close(piped[0]);      

      	execvp(child1_args[0], child1_args);   
      	perror("Fail to execute first command");
      	exit(EXIT_FAILURE);
}

	// Fork 2nd command
	if (fork()==0){
		dup2(piped[0], STDIN_FILENO);
		close(piped[1]);

		execvp(child2_args[0], child2_args);
		perror("Failed to run 2st command");
		exit(EXIT_FAILURE);
	}

	close(piped[0]);
	close(piped[1]);

	wait(0);
	wait(0);
}


int main(void) {
	char command[MAX_LENGTH];
	int wait;
	int should_run = 1;

	char *args[BUF_LENGTH]; // Maximum 40 argments
	char *redir_args[REDIR_LENGTH];
   	char *child1_args[PIPE_LENGTH], *child2_args[PIPE_LENGTH];


	while (should_run) {
		printf("zero>>");
		fflush(stdout);
		while (fgets(command, MAX_LENGTH, stdin)==NULL){
			perror("You did not write anything!");
			fflush(stdin);
		}

		command[strcspn(command,"\n")]='\0';

		// Youn can enter quit or \q to quit shell
		if (strcmp(command,"quit")==0 || strcmp(command,"\\q")==0 || strcmp(command, "exit")==0){
			should_run=0;
			continue;
		}
		
		//Parse command and arguments.
		parse_command(command, args, &wait);
		parse_dir(args, redir_args);

		if (parse_pipe(args, child1_args, child2_args)){
			run_pipe(child1_args, child2_args);
			continue;
		}


		// Fork child process
      	pid_t pid = fork();

      	// Fork return twice on success: 0 - child process, > 0 - parent process
      	switch (pid) {
        	case -1:
            	perror("fork() failed!");
            	exit(EXIT_FAILURE);
      
         	case 0:     // In child process
            	child(args, redir_args);
            	exit(EXIT_SUCCESS);
      
         	default:    // In parent process
            	parent(pid, wait);
      	}
		
		//If command contains output redirection argument
		//	fork a child process invoking fork() system call and perform the followings in the child process:
		//		open the redirected file in write only mode invoking open() system call
		//		copy the opened file descriptor to standard output file descriptor (STDOUT_FILENO) invoking dup2() system call
		//		close the opened file descriptor invoking close() system call
		//		change the process image with the new process image according to the UNIX command using execvp() system call
		//	If command does not conatain & (ampersand) at the end
		//		invoke wait() system call in parent process.
		//
		//		
		//If command contains input redirection argument
		//	fork a child process invoking fork() system call and perform the followings in the child process:
		//		open the redirected file in read  only mode invoking open() system call
		//		copy the opened file descriptor to standard input file descriptor (STDIN_FILENO) invoking dup2() system call
		//		close the opened file descriptor invoking close() system call
		//		change the process image with the new process image according to the UNIX command using execvp() system call
		//	If command does not conatain & (ampersand) at the end
		//		invoke wait() system call in parent process.
		//
		//	
		
		//If command contains pipe argument
		//	fork a child process invoking fork() system call and perform the followings in the child process:
		//		create a pipe invoking pipe() system call
		//		fork another child process invoking fork() system call and perform the followings in this child process:
		//			close the write end descriptor of the pipe invoking close() system call
		//			copy the read end  descriptor of the pipe to standard input file descriptor (STDIN_FILENO) invoking dup2() system call
		//			change the process image of the this child with the new image according to the second UNIX command after the pipe symbol (|) using execvp() system call
		//		close the read end descriptor of the pipe invoking close() system call
		//		copy the write end descriptor of the pipe to standard output file descriptor (STDOUT_FILENO) invoking dup2() system call
		//		change the process image with the new process image according to the first UNIX command before the pipe symbol (|) using execvp() system call
		//	If command does not conatain & (ampersand) at the end
		//		invoke wait() system call in parent process.
		//
		//
		//If command does not contain any of the above
		//	fork a child process using fork() system call and perform the followings in the child process.
		//		change the process image with the new process image according to the UNIX command using execvp() system call
		//	If command does not conatain & (ampersand) at the end
		//		invoke wait() system call in parent process.
	}

	return 0;
}