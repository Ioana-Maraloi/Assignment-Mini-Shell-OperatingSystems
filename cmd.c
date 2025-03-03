// SPDX-License-Identifier: BSD-3-Clause

#include "cmd.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include "utils.h"
#define READ 0
#define WRITE 1

static bool shell_cd(word_t *dir)
{
	if (!dir)
		return 0;
	if (dir->next_word)
		return 0;
	char cwd[100];

	getcwd(cwd, 100);
	int result = chdir(dir->string);

	if (result == -1)
		return false;
	return true;
}

static int shell_exit(void)
{
	return SHELL_EXIT;
}

static int parse_simple(simple_command_t *s, int level, command_t *father)
{
	if (s->verb == NULL)
		return 0;
	if (s->verb->next_part) {
		char *aux = malloc(100);
		word_t *iter = s->verb->next_part;

		if (strcmp(iter->string, "=") == 0)
			iter = iter->next_part;
		while (iter) {
			if (iter->expand) {
				char *get = getenv(iter->string);

				strcat(aux, get);
			} else {
				strcat(aux, iter->string);
			}
			iter = iter->next_part;
		}
	int returned = setenv(s->verb->string, aux, 1);

	free(aux);
	return returned;
	}
	int stdin = dup(STDIN_FILENO);
	int stdout = dup(STDOUT_FILENO);
	int stderr = dup(STDERR_FILENO);
	int out_file;

	if (strncmp(s->verb->string, "cd", 2) == 0) {
		if (s->out) {
			if (s->io_flags == IO_REGULAR)
				out_file = open(s->out->string, O_WRONLY | O_CREAT | O_TRUNC, 0777);
			if (s->io_flags == IO_OUT_APPEND)
				out_file = open(s->out->string, O_WRONLY | O_CREAT | O_APPEND, 0777);
			if (out_file == -1) {
				perror("failed to open out file");
				return 1;
			}
		}
		if (shell_cd(s->params) == true)
			return 0;
		perror("cd fail");
		return 1;
	}
	if (s->out) {
		// *filename pt a face numele fisierului, daca
		// contine environment variables cumva
		char *filename = malloc(100);

		if (s->out->next_part) {
			// adaug
			strcat(filename, s->out->string);
			word_t *aux = s->out->next_part;

			while (aux) {
				// daca e env var
				if (aux->expand) {
					// aflu ce inseamna si adaug la numele fisierului (daca inseamna cv)
					char *get = getenv(aux->string);

					if (get != NULL)
						strcat(filename, get);
				} else {
					// daca nu e env var, adaug direct
					strcat(filename, aux->string);
				}
				aux = aux->next_part;
			}
		} else {
			strcat(filename, s->out->string);
		}
		// deschid fisierul si redirectionez outputul
		if (s->io_flags == IO_REGULAR)
			out_file = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0777);
		if (s->io_flags == IO_OUT_APPEND)
			out_file = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0777);
		if (dup2(out_file, STDOUT_FILENO) == -1) {
			close(out_file);
			exit(-1);
		}
		// redirectionez si pt err
		if (s->err) {
			if (strcmp(filename, s->err->string) == 0) {
				if (dup2(out_file, STDERR_FILENO) == -1) {
					close(out_file);
					exit(-1);
				}
			} else {
				close(out_file);
			}
		} else {
			if (out_file == -1) {
				perror("failed to open out file");
				return 1;
			}
			dup2(out_file, STDOUT_FILENO);
			close(out_file);
		}
		free(filename);
	}
	if (s->in) {
		int in_file;

		in_file = open(s->in->string, O_RDONLY);
		if (dup2(in_file, STDIN_FILENO) == -1)
			close(in_file);
	}
	// daca am err si nu l am deschis inca (err!= out)
	if (s->err) {
		int err_file;

		if (!s->out || strcmp(s->out->string, s->err->string) != 0) {
		// printf("nush daca intru aici");
			if (s->io_flags == IO_REGULAR)
				err_file = open(s->err->string, O_WRONLY | O_CREAT | O_TRUNC, 0777);
			if (s->io_flags == IO_ERR_APPEND)
				err_file = open(s->err->string, O_WRONLY | O_CREAT | O_APPEND, 0777);
			if (dup2(err_file, STDERR_FILENO) == -1)
				close(err_file);
		}
	}
	int pid = fork();

	if (pid == 0) {
		word_t *aux = s->params;
		int cnt = 0;
		// numar cate argumente am ca sa aloc vectorul
		while (aux != NULL) {
			cnt++;
			aux = aux->next_word;
		}
		if (s->out)
			cnt++;
		char **args = malloc((cnt + 2) * sizeof(char *));
		// pun comanda pe prima pozitie in vector
		args[0] = (char *)s->verb->string;
		aux = s->params;
		int i = 1;

		while (aux) {
			char *add = malloc(100);
			// daca argumentul e env var
			if (aux->expand) {
				char *get = getenv(aux->string);
				// caut ce inseamna si concatenez
				if (get != NULL)
					strcat(add, get);
				word_t *iter = aux->next_part;
				// daca e alcatuit din mai multe cuvinte, le parcurg si pe alea
				// si verific daca sunt env var si concatenez ce inseamna
				if (iter) {
					char *getget = getenv(iter->string);

					if (getget != NULL)
						strcat(add, getget);
					else
						strcat(add, iter->string);
					iter = iter->next_part;
				}
				args[i] = add;
			} else {
				// pun in vector daca nu e env var
				args[i] = (char *)aux->string;
			}
			i++;
			aux = aux->next_word;
			free(add);
		}
		args[i] = NULL;
		int status_code = execvp(args[0], args);

		if (status_code == -1) {
			printf("Execution failed for '%s'\n", args[0]);
			exit(-1);
		}
		for (int j = 0; j < cnt + 2; j++)
			free(args[j]);
		free(args);
	}
	int status;

	waitpid(pid, &status, 0);
	dup2(stdin, STDIN_FILENO);
	dup2(stdout, STDOUT_FILENO);
	dup2(stderr, STDERR_FILENO);
	close(stdin);
	close(stdout);
	close(stderr);
	return WEXITSTATUS(status);
}


static bool run_in_parallel(command_t *cmd1, command_t *cmd2, int level,
								command_t *father)
{
	pid_t pid1 = fork();

	if (pid1 == 0) {
		int ret = parse_command(cmd1, level + 1, cmd1);

		exit(ret);
	}
	pid_t pid2 = fork();

	if (pid2 == 0) {
		int ret = parse_command(cmd2, level + 1, cmd2);

		exit(ret);
	}
	waitpid(pid1, NULL, 0);
	waitpid(pid2, NULL, 0);
	return true;
}


int number_of_commands(command_t *cmd)
{
	if (cmd == NULL)
		return 0;
	if (cmd->op == OP_NONE)
		return 1;
	return number_of_commands(cmd->cmd1) + number_of_commands(cmd->cmd2);
}
void add_command(simple_command_t **command_vector, command_t *node, int *i, int cap)
{
	if (node->op == OP_NONE) {
		command_vector[*i] = node->scmd;
		(*i)++;
	} else {
		add_command(command_vector, node->cmd1, i, cap);
		add_command(command_vector, node->cmd2, i, cap);
	}
}
simple_command_t **create_command_vector(command_t *father)
{
	// creez un vector in care bag comenzile in preordine
	// ca sa pot sa le execut si sa fac pipe urile in ordinea in
	// care au fost scrise
	simple_command_t **command_vector = malloc(number_of_commands(father) * sizeof(simple_command_t *));
	int i = 0;

	add_command(command_vector, father, &i, number_of_commands(father));
	return command_vector;
}
static bool run_on_pipe(command_t *cmd1, command_t *cmd2, int level,
							command_t *father)
{
	simple_command_t **command_vector = create_command_vector(father);
	int nr_commands = number_of_commands(father);
	int fd[100][2];
	int status;
	pid_t pid[100];
	int i = 0;

	for (i = 0; i < nr_commands - 1; i++) {
		if (pipe(fd[i]) == -1)
			return false;
	}
	for (i = 0; i < nr_commands; i++) {
		pid[i] = fork();
		if (pid[i] < 0)
			return false;
		if (pid[i] == 0) {
			// partea de out; daca nu e ultima comanda
			// atunci pun in pipe
			if (i != nr_commands - 1) {
				dup2(fd[i][1], STDOUT_FILENO);
				close(fd[i][0]);
				close(fd[i][1]);
			} else {
				// daca e ultima comanda
				// verific daca are out si err
				int out_file;

				if (command_vector[i]->out) {
					if (command_vector[i]->io_flags == IO_REGULAR)
							out_file = open(command_vector[i]->out->string,
												O_WRONLY | O_CREAT | O_TRUNC, 0777);
					if (command_vector[i]->io_flags == IO_OUT_APPEND)
							out_file = open(command_vector[i]->out->string,
												O_WRONLY | O_CREAT | O_APPEND, 0777);
					dup2(out_file, STDOUT_FILENO);
					if (command_vector[i]->err) {
						if (strcmp(command_vector[i]->out->string,
								command_vector[i]->err->string) == 0) {
							dup2(out_file, STDERR_FILENO);
						}
						close(out_file);
					}
				}
			}
			// partea de inputtt
			// daca nu e prima comanda, setez inputul
			if (i > 0) {
				dup2(fd[i-1][0], STDIN_FILENO);
				close(fd[i-1][0]);
				close(fd[i-1][1]);
				}
			// efectiv functia
			word_t *aux = command_vector[i]->params;
			int cnt = 0;

			while (aux != NULL) {
				cnt++;
				aux = aux->next_word;
			}
			char **args = malloc((cnt + 2) * sizeof(char *));

			args[0] = (char *)command_vector[i]->verb->string;
			aux = command_vector[i]->params;
			int j = 1;

			while (aux) {
				args[j] = (char *)aux->string;
				j++;
				aux = aux->next_word;
			}
			args[j] = NULL;
			int status_code = execvp(args[0], args);

			for (j = 0; j < cnt + 2; j++)
				free(args[j]);
			free(args);
			exit(status_code);
		} else {
			if (i < nr_commands - 1)
				close(fd[i][1]);
			if (i > 0)
				close(fd[i - 1][0]);
		}
	}
	for (i = 0; i < nr_commands; i++)
		waitpid(pid[i], &status, 0);
	return WEXITSTATUS(status);
}

int parse_command(command_t *c, int level, command_t *father)
{
	if (c->op == OP_NONE) {
		if (strcmp(c->scmd->verb->string, "exit") == 0 || strcmp(c->scmd->verb->string, "quit") == 0)
			return shell_exit();
		return parse_simple(c->scmd, level + 1, c);
	}
	switch (c->op) {
	case OP_SEQUENTIAL:
		parse_command(c->cmd1, level + 1, c);
		parse_command(c->cmd2, level + 1, c);
		break;
	case OP_PARALLEL:
		run_in_parallel(c->cmd1, c->cmd2, level + 1, c);
		break;
	case OP_CONDITIONAL_NZERO:
		if (parse_command(c->cmd1, level + 1, c) != 0)
			return parse_command(c->cmd2, level + 1, c);
		break;
	case OP_CONDITIONAL_ZERO:
		if (parse_command(c->cmd1, level + 1, c) == 0)
			return parse_command(c->cmd2, level + 1, c);
		break;
	case OP_PIPE:
		return run_on_pipe(c->cmd1, c->cmd2, level + 1, c);
	default:
		return SHELL_EXIT;
	}
	return 0;
}
