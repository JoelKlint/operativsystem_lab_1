#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "list.h"

#define PERM		(0644)		/* default permission rw-r--r-- */
#define MAXBUF		(512)		/* max length of input line. */
#define MAX_ARG		(100)		/* max number of cmd line arguments. */

typedef enum { 
	AMPERSAND, 			/* & */
	NEWLINE,			/* end of line reached. */
	NORMAL,				/* file name or command option. */
	INPUT,				/* input redirection (< file) */
	OUTPUT,				/* output redirection (> file) */
	PIPE,				/* | for instance: ls *.c | wc -l */
	SEMICOLON			/* ; */
} token_type_t;

static char*	progname;		/* name of this shell program. */
static char	input_buf[MAXBUF];	/* input is placed here. */
static char	token_buf[2 * MAXBUF];	/* tokens are placed here. */
static char*	input_char;		/* next character to check. */
static char*	token;			/* a token such as /bin/ls */

static list_t*	path_dir_list;		/* list of directories in PATH. */
static int	input_fd;		/* for i/o redirection or pipe. */
static int	output_fd;		/* for i/o redirection or pipe */

/* fetch_line: read one line from user and put it in input_buf. */
int fetch_line(char* prompt)
{
	int	c;
	int	count;

	input_char = input_buf;
	token = token_buf;

	printf("%s", prompt);
	fflush(stdout);

	count = 0;

	for (;;) {

		c = getchar();

		if (c == EOF)
			return EOF;

		if (count < MAXBUF)
			input_buf[count++] = c;

		if (c == '\n' && count < MAXBUF) {
			input_buf[count] = 0;
			return count;
		}

		if (c == '\n') {
			printf("too long input line\n");
			return fetch_line(prompt);
		}

	}
}

/* end_of_token: true if character c is not part of previous token. */
static bool end_of_token(char c)
{
	switch (c) {
	case 0:
	case ' ':
	case '\t':
	case '\n':
	case ';':
	case '|':
	case '&':
	case '<':
	case '>':
		return true;

	default:
		return false;
	}
}

/* gettoken: read one token and let *outptr point to it. */
int gettoken(char** outptr)
{
	token_type_t	type;

	*outptr = token;

	while (*input_char == ' '|| *input_char == '\t')
		input_char++;

	*token++ = *input_char;

	switch (*input_char++) {
	case '\n':
		type = NEWLINE;
		break;

	case '<':
		type = INPUT;
		break;
	
	case '>':
		type = OUTPUT;
		break;
	
	case '&':
		type = AMPERSAND;
		break;
	
	case '|':
		type = PIPE; 
		break;
	
	default:
		type = NORMAL;

		while (!end_of_token(*input_char))
			*token++ = *input_char++;
	}

	*token++ = 0; /* null-terminate the string. */
	
	return type;
}

/* error: print error message using formatting string similar to printf. */
void error(char *fmt, ...)
{
	va_list		ap;

	fprintf(stderr, "%s: error: ", progname);

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	/* print system error code if errno is not zero. */
	if (errno != 0) {
		fprintf(stderr, ": ");
		perror(0);
	} else
		fputc('\n', stderr);

}

/* run_program: fork and exec a program. */
void run_program(char** argv, int argc, bool foreground, bool doing_pipe)
{
	if(strcmp(argv[0], "cd") == 0) {
		// HANDLE THIS CASE
		// if(strcmp(argv[1], "-") == 0) {
		// 	char* old_pwd = getenv("OLDPWD");
		// 	printf("%s", old_pwd);
		// 	setenv("OLDPWD", getenv("PWD"), 1);
		// 	setenv("PWD", old_pwd, 1);
		// 	chdir(old_pwd);
		// }
		// else {
			setenv("OLDPWD", getenv("PWD"), 1);
			chdir(argv[1]);
			setenv("PWD", argv[1], 1);
		// }
		return;
	}

	// int child_pid = fork();

	// We are in the child process
	int child_pid;
	if((child_pid = fork()) == 0) {
		// printf("Current command: %s\n", argv[0]);
		// printf("%s pid: %d\n", argv[0], getpid());
		// printf("Input pipe: %d\n", input_fd);
		// printf("Output pipe: %d\n", output_fd);

		if(input_fd != 0) {
			// printf("pid %d changing input to: %d\n", getpid(), input_fd);
			dup2(input_fd, 0);
		}
		if(output_fd != 0) {
			// printf("pid %d changing output to: %d\n", getpid(), output_fd);
			dup2(output_fd, 1);
		}

		// om input_fd != 0. dup2 input
		// om output_fd != 0. dup2 output


		// This only happens in case of Ampersand
		if(!foreground && !doing_pipe) {
			// printf("%s\n", "This is a background process");
			output_fd = open("/dev/null", O_WRONLY);
			dup2(output_fd, 1);
		}

		execvp(argv[0], argv);

		// while(length(path_dir_list) != 0) {
		// 	void* current_dir_path = remove_first(&path_dir_list);
		// 	char program_path[100] = "";
		// 	char *cur = program_path, * const end = program_path + sizeof program_path;
		// 	cur += snprintf(cur, end-cur, "%s", current_dir_path);
		// 	cur += snprintf(cur, end-cur, "%s", "/");
		// 	cur += snprintf(cur, end-cur, "%s", argv[0]);
		// 	if (access(program_path, F_OK|X_OK) == 0) {
		// 		 int res = execv(program_path, argv);
		// 		 if (res != 0) {
		// 		 	printf("%s", "GOT ERROR WAT");
		// 		 }
		// 	}
		// }

		// Kill child process
		printf("Could not find command\n");
		exit(1);		

	}
	else {
		// Parent
		// Man kan alltid vänta. Men kan bli bättre iom parallellt. Vänta på sista
		// printf("\n");
		if(!doing_pipe && foreground) {
			// printf("Parent(%d) waiting for pid: %d\n", getpid(), child_pid);
			int term_pid = waitpid(child_pid, NULL, 0);
			// printf("Parent(%d) finished waiting for pid: %d\n", getpid(), term_pid);
		}
	}

	/* you need to fork, search for the command in argv[0],
         * setup stdin and stdout of the child process, execv it.
         * the parent should sometimes wait and sometimes not wait for
	 * the child process (you must figure out when). if foreground
	 * is true then basically you should wait but when we are
	 * running a command in a pipe such as PROG1 | PROG2 you might
	 * not want to wait for each of PROG1 and PROG2...
	 * 
	 * hints:
	 *  snprintf is useful for constructing strings.
	 *  access is useful for checking wether a path refers to an 
	 *      executable program.
	 * 
	 * 
	 */
}

void parse_line(void)
{
	char*		argv[MAX_ARG + 1];
	int		argc;
	int		pipe_fd[2];	/* 1 for producer and 0 for consumer. */
	token_type_t	type;
	bool		foreground;
	bool		doing_pipe;

	input_fd	= 0;
	output_fd	= 0;
	argc		= 0;

	for (;;) {
			
		foreground	= true;
		doing_pipe	= false;

		type = gettoken(&argv[argc]);

		switch (type) {
		case NORMAL:
			// printf("NORMAL\n");
			argc += 1;
			break;

		case INPUT:
			// printf("INPUT\n");
			type = gettoken(&argv[argc]);
			if (type != NORMAL) {
				error("expected file name: but found %s", 
					argv[argc]);
				return;
			}

			input_fd = open(argv[argc], O_RDONLY);

			if (input_fd < 0)
				error("cannot read from %s", argv[argc]);

			break;

		case OUTPUT:
			// printf("OUTPUT\n");
			type = gettoken(&argv[argc]);
			if (type != NORMAL) {
				error("expected file name: but found %s", 
					argv[argc]);
				return;
			}

			output_fd = open(argv[argc], O_CREAT | O_WRONLY, PERM);

			if (output_fd < 0)
				error("cannot write to %s", argv[argc]);
			break;

		case PIPE:
			// printf("PIPE\n");
			doing_pipe = true;
			pipe(pipe_fd);
			output_fd = pipe_fd[1];

			/*FALLTHROUGH*/

		case AMPERSAND:
			// printf("AMPERSAND\n");
			foreground = false;

			/*FALLTHROUGH*/

		case NEWLINE:
			// printf("NEWLINE\n");
		case SEMICOLON:
			// printf("SEMICOLON\n");

			if (argc == 0)
				return;
						
			argv[argc] = NULL;

			run_program(argv, argc, foreground, doing_pipe);

			if(doing_pipe) {
				close(pipe_fd[1]);
				input_fd = pipe_fd[0];
			}
			else {
				input_fd = 0;
			}

			output_fd	= 0;
			argc		= 0;

			if (type == NEWLINE)
				return;

			break;
		}
	}
}

/* init_search_path: make a list of directories to look for programs in. */
static void init_search_path(void)
{
	char*		dir_start;
	char*		path;
	char*		s;
	list_t*		p;
	bool		proceed;

	path = getenv("PATH");

	/* path may look like "/bin:/usr/bin:/usr/local/bin" 
	 * and this function makes a list with strings 
	 * "/bin" "usr/bin" "usr/local/bin"
 	 *
	 */

	dir_start = malloc(1+strlen(path));
	if (dir_start == NULL) {
		error("out of memory.");
		exit(1);
	}

	strcpy(dir_start, path);

	path_dir_list = NULL;

	if (path == NULL || *path == 0) {
		path_dir_list = new_list("");
		return;
	}

	proceed = true;

	while (proceed) {
		s = dir_start;
		while (*s != ':' && *s != 0)
			s++;
		if (*s == ':')
			*s = 0;
		else
			proceed = false;

		insert_last(&path_dir_list, dir_start);

		dir_start = s + 1;
	}

	p = path_dir_list;

	if (p == NULL)
		return;

#if 0
	do {
		printf("%s\n", (char*)p->data);
		p = p->succ;	
	} while (p != path_dir_list);
#endif
}

/* main: main program of simple shell. */
int main(int argc, char** argv)
{
	progname = argv[0];

	init_search_path();	

	while (fetch_line("% ") != EOF)
		parse_line();

	return 0;
}
