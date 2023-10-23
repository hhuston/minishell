#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <pwd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>

#define BLUE 	"\x1b[34;1m"
#define DEFAULT "\x1b[0m"

volatile sig_atomic_t interrupted;

void sig_handler(int sig) {
	interrupted = 1;
	printf("\n");
}

int main() {
	// Installing Signal Handler
	struct sigaction action;
	action.sa_handler = sig_handler;
	sigaction(SIGINT, &action, NULL);

	char cwd[256];
	char cont = 1;
	while (cont) {
		interrupted = 0;
		errno = 0;

		// Attempt to get the current working directory
		if (getcwd(cwd, 256) == NULL) {
			fprintf(stderr, "Error: Cannot get current working directory. %s.\n", strerror(errno));
			continue;
		}
		
		fprintf(stdout, "%s[%s]%s> ", BLUE, cwd, DEFAULT);
		
		// Read User's Input
		size_t size = 2048;
		if ((input = (int*)malloc(size)) == NULL) {
			fprintf(stderr, "Error: malloc() failed. %s.\n", strerror(errno));
			continue;
		}
		fgets(input, size, stdin);
		
		if (size  == -1) {
			fflush(stdin);
			fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));
			continue;
		}

		if (strcmp(input, "\n") == 0 || interrupted != 0)
			continue;

		// Store the locations of spaces in the input then make an argument list from the input
		int* arg_locs;
		if ((arg_locs = (int*)malloc(size)) == NULL) {
			fprintf(stderr, "Error: malloc() failed. %s.\n", strerror(errno));
			continue;
		}
				
		int argc = 0;

		size_t i = 1;

		arg_locs[0] = 0;
		while (input[i] != '\n') {
			if (input[i] == ' ') {
				arg_locs[++argc] = i+1;
				input[i] = 0;
			}
			i++;
		}
		arg_locs[++argc] = i;
		input[i] = 0;
		argc++;
		char* argv[argc];
		argc--;
		for (i = 0; i < argc; i++)
			argv[i] = &input[arg_locs[i]];
		argv[argc] = NULL;
		free(arg_locs);

		// Execute user's command
		if (strcmp(argv[0], "exit") == 0)
			cont = 0;
		else if (strcmp(argv[0], "cd") == 0) {
			if (argc == 1 || (argc == 2 && strcmp(argv[1], "~") == 0)) {
				uid_t uid = getuid();
				struct passwd* pw;
				if ((pw = getpwuid(uid)) == NULL)
					fprintf(stderr, "Error: Cannot get passwd entry. %s.\n", strerror(errno));
				else
					chdir(pw->pw_dir);
			}
			else if (argc == 2) {
				char* dir = argv[1];
				chdir(dir);
				if (errno != 0)
					fprintf(stderr, "Error: Cannot change directory to \'%s\'. %s.\n", argv[1], strerror(errno));
			}
			else
				fprintf(stderr, "Error: Too many arguments to cd.\n");
		}
		else {
			// Change the user's command to be the path to /bin/<cmd>
			size_t arg0_size = strlen(argv[0]);
			char* path = (char*)malloc(5 + arg0_size + 1);

			if (arg0_size > 2 && argv[0][0] == '.' && argv[0][1] == '/')
				memcpy(path, argv[0], arg0_size);
			else {
				char* bin = "/bin/";
				for (i = 0; i < 5; i++)
					path[i] = bin[i];
				for (i = 0; i < arg0_size; i++)
					path[i + 5] = argv[0][i];
				path[5 + arg0_size] = 0;
			}		
			pid_t pid;
			if ((pid = fork()) < 0) {
				 fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));
				 continue;
			}
			else if (pid == 0) {
				if (execv(path, argv) == -1) {
					fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));
					exit(EXIT_FAILURE);
				}
				exit(EXIT_SUCCESS);
			}
			else 
				wait(NULL);
		}
		wait(NULL);
	}
	exit(EXIT_SUCCESS);
}
