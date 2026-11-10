#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/wait.h>

typedef enum {
    MODE_LIST,
    MODE_EXEC,
    MODE_PRINT,
    MODE_LEN
} ToolMode;

/* Strip trailing whitespace and newlines */
static void rtrim(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || isspace((unsigned char)str[len - 1]))) {
        str[len - 1] = '\0';
        len--;
    }
}

/* Check if a line is a markdown H1 header: '# ' */
static int is_h1_header(const char *line, const char **title_out) {
    if (line[0] == '#' && (line[1] == ' ' || line[1] == '\t')) {
        const char *p = line + 2;
        while (*p == ' ' || *p == '\t') {
            p++;
        }
        if (title_out) {
            *title_out = p;
        }
        return 1;
    }
    return 0;
}

/* Check if line starts with ```bash */
static int is_bash_code_block_start(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (strncmp(line, "```bash", 7) == 0) {
        char next = line[7];
        return (next == '\0' || next == '\r' || next == '\n' || isspace((unsigned char)next));
    }
    return 0;
}

/* Check if line closes code block with ``` */
static int is_code_block_end(const char *line) {
    while (*line == ' ' || *line == '\t') line++;
    if (strncmp(line, "```", 3) == 0) {
        char next = line[3];
        return (next == '\0' || next == '\r' || next == '\n' || isspace((unsigned char)next));
    }
    return 0;
}

/* List mode: print headers as "%3d %s\n" */
static int list_headers(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening markdown file");
        return 1;
    }

    char *line = NULL;
    size_t cap = 0;
    int step_number = 0;

    while (getline(&line, &cap, fp) != -1) {
        const char *title = NULL;
        if (is_h1_header(line, &title)) {
            step_number++;
            char buffer[1024];
            strncpy(buffer, title, sizeof(buffer) - 1);
            buffer[sizeof(buffer) - 1] = '\0';
            rtrim(buffer);
            printf("%3d : %s\n", step_number, buffer);
        }
    }

    free(line);
    fclose(fp);
    return 0;
}

/* Len mode: print total number of headers */
static int count_headers(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening markdown file");
        return 1;
    }

    char *line = NULL;
    size_t cap = 0;
    int count = 0;

    while (getline(&line, &cap, fp) != -1) {
        if (is_h1_header(line, NULL)) {
            count++;
        }
    }

    free(line);
    fclose(fp);

    printf("%d\n", count);
    return 0;
}

/* Extract the first ```bash block from a given section */
static char *extract_bash_block(const char *filename, int target_step) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("Error opening markdown file");
        return NULL;
    }

    char *line = NULL;
    size_t cap = 0;
    int current_step = 0;
    int in_target_section = 0;
    int in_bash_block = 0;
    char *code_buffer = NULL;
    size_t code_len = 0;
    int found_block = 0;

    while (getline(&line, &cap, fp) != -1) {
        if (is_h1_header(line, NULL)) {
            current_step++;
            if (current_step == target_step) {
                in_target_section = 1;
                continue;
            } else if (in_target_section) {
                break;
            }
        }

        if (in_target_section) {
            if (!in_bash_block) {
                if (is_bash_code_block_start(line)) {
                    in_bash_block = 1;
                    continue;
                }
            } else {
                if (is_code_block_end(line)) {
                    found_block = 1;
                    break;
                }
                size_t line_len = strlen(line);
                char *new_buf = realloc(code_buffer, code_len + line_len + 1);
                if (!new_buf) {
                    fprintf(stderr, "Error: out of memory.\n");
                    free(code_buffer);
                    free(line);
                    fclose(fp);
                    return NULL;
                }
                code_buffer = new_buf;
                memcpy(code_buffer + code_len, line, line_len);
                code_len += line_len;
                code_buffer[code_len] = '\0';
            }
        }
    }

    free(line);
    fclose(fp);

    if (current_step < target_step) {
        fprintf(stderr, "Error: Step %d not found in '%s'.\n", target_step, filename);
        free(code_buffer);
        return NULL;
    }

    if (!found_block || code_len == 0) {
        fprintf(stderr, "Error: No ```bash code block found in step %d.\n", target_step);
        free(code_buffer);
        return NULL;
    }

    return code_buffer;
}

/* Print mode: dump raw code directly to stdout */
static int print_step(const char *filename, int target_step) {
    char *code = extract_bash_block(filename, target_step);
    if (!code) {
        return 1;
    }

    fputs(code, stdout);
    size_t len = strlen(code);
    if (len > 0 && code[len - 1] != '\n') {
        putchar('\n');
    }

    free(code);
    return 0;
}

/* Exec mode: extract, show, prompt, and execute */
static int exec_step(const char *filename, int target_step, int auto_yes) {
    char *code_buffer = extract_bash_block(filename, target_step);
    if (!code_buffer) {
        return 1;
    }

    size_t code_len = strlen(code_buffer);

    printf("--- Code to execute (Step %d) ---\n", target_step);
    fputs(code_buffer, stdout);
    if (code_buffer[code_len - 1] != '\n') {
        putchar('\n');
    }
    printf("---------------------------------\n");

    if (!auto_yes) {
        printf("Execute this script? [y/N]: ");
        fflush(stdout);

        char response[64];
        if (!fgets(response, sizeof(response), stdin)) {
            printf("\nAborted.\n");
            free(code_buffer);
            return 0;
        }

        char *r = response;
        while (isspace((unsigned char)*r)) r++;
        if (*r != 'y' && *r != 'Y') {
            printf("Aborted.\n");
            free(code_buffer);
            return 0;
        }
    }

    /* macOS supports /tmp as a symlink to /var/folders/..., so /tmp/bakery_XXXXXX works reliably */
    char temp_path[] = "/tmp/bakery_XXXXXX";
    int fd = mkstemp(temp_path);
    if (fd == -1) {
        perror("Error creating temporary script file");
        free(code_buffer);
        return 1;
    }

    FILE *tf = fdopen(fd, "w");
    if (!tf) {
        perror("Error opening temporary script file");
        close(fd);
        unlink(temp_path);
        free(code_buffer);
        return 1;
    }

    fputs(code_buffer, tf);
    fclose(tf);
    free(code_buffer);

    /* Runs using /bin/bash */
    char command[512];
    snprintf(command, sizeof(command), "/bin/bash \"%s\"", temp_path);
    int status = system(command);

    unlink(temp_path);

    if (status == -1) {
        perror("Failed to execute bash");
        return 1;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return 1;
}

static void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, "  %s <file.md>                  List headers as step numbers\n", prog_name);
    fprintf(stderr, "  %s len <file.md>              Print total count of headers\n", prog_name);
    fprintf(stderr, "  %s print <num> <file.md>      Print raw bash code for piping\n", prog_name);
    fprintf(stderr, "  %s exec <num> <file.md> [-y]  Execute bash block in step <num>\n", prog_name);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    ToolMode mode = MODE_LIST;
    const char *filename = NULL;
    int target_step = -1;
    int auto_yes = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-y") == 0) {
            auto_yes = 1;
        } else if (strcmp(argv[i], "len") == 0) {
            mode = MODE_LEN;
        } else if (strcmp(argv[i], "print") == 0) {
            mode = MODE_PRINT;
            if (i + 1 < argc && isdigit((unsigned char)argv[i + 1][0])) {
                target_step = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Error: 'print' requires a step number.\n");
                return 1;
            }
        } else if (strcmp(argv[i], "exec") == 0) {
            mode = MODE_EXEC;
            if (i + 1 < argc && isdigit((unsigned char)argv[i + 1][0])) {
                target_step = atoi(argv[++i]);
            } else {
                fprintf(stderr, "Error: 'exec' requires a step number.\n");
                return 1;
            }
        } else if (argv[i][0] == '-') {
            print_usage(argv[0]);
            return 1;
        } else {
            if (!filename) {
                filename = argv[i];
            }
        }
    }

    if (!filename) {
        fprintf(stderr, "Error: No markdown file specified.\n");
        print_usage(argv[0]);
        return 1;
    }

    switch (mode) {
        case MODE_LIST:
            return list_headers(filename);
        case MODE_LEN:
            return count_headers(filename);
        case MODE_PRINT:
            if (target_step <= 0) {
                fprintf(stderr, "Error: Please specify a valid step number greater than 0.\n");
                return 1;
            }
            return print_step(filename, target_step);
        case MODE_EXEC:
            if (target_step <= 0) {
                fprintf(stderr, "Error: Please specify a valid step number greater than 0.\n");
                return 1;
            }
            return exec_step(filename, target_step, auto_yes);
    }

    return 0;
}
