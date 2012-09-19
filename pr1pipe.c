/* CMPSC 473, Project 1, starter kit
 *
 * Sample program for a pipe
 *
 * See http://www.cse.psu.edu/~dheller/cmpsc311/Lectures/Interprocess-Communication.html
 * and http://www.cse.psu.edu/~dheller/cmpsc311/Lectures/Files-Directories.html
 * for some more information, examples, and references to the CMPSC 311 textbooks.
 */

//--------------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

// This makes Solaris and Linux happy about waitpid(); not required on Mac OS X
#include <sys/wait.h>

//--------------------------------------------------------------------------------

void err_sys(char *msg);                          // print message and quit

// In a pipe, the parent is upstream, and the child is downstream, connected by
//   a pair of open file descriptors.

// This function will copy each byte to its output pipe, without any changes.
// It will only write to the first pipe.
//void P1_actions(int argc, char *argv[], int fd);
int P1_actions(FILE *infile_ptr, char *infile, int fd);

// This function will grab each byte from the input side of the first pipe,
// and search through the bytes for matches to the specified strings.
void P2_actions(FILE *outfile_ptr, char **m_lines, int m_count, int fd);
// fd = file descriptor, opened by pipe()
// treat fd as if it had come from open()
//

void P3_actions(FILE *outfile_ptr, int second_fd);

#define BUFFER_SIZE 4096
#define SMALL_LINE 512

//--------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
                                                  // make some noise
    printf(" 1: PID, PPID: %d %d\n", getpid(), getppid());

    int fd[2];                                    // pipe endpoints
    pid_t child_pid;

    FILE *infile_ptr;
    FILE *outfile_ptr;

//    int optind;
//    char *optarg;
    int ch;

    char *infile;
    char *outfile;
    int m_count = 0;
    char **m_lines;
    m_lines = malloc(sizeof(int*) * argc);
    // Getopt switch statement for command line options:

    while((ch = getopt(argc, argv, "i:o:m:")) != -1)
    {
        switch(ch)
        {
            case 'i':
                infile = strdup(optarg);
                printf("infile is %s\n", infile);
                break;
            case 'o':
                outfile = strdup(optarg);
                printf("outfile is %s\n", outfile);
                break;
            case 'm':
                m_lines[m_count] = strdup(optarg);
                printf("m line is %s\n", m_lines[m_count]);
                m_count++;
                break;
            default:
                printf("Bad command line argument given...\n");
                exit(0);
                break;
        }
    }

    // Now open the file pointed to by 'infile':
    if(infile != NULL)
    {
        infile_ptr = fopen(infile, "r");
        if(infile_ptr == NULL)
        {
            printf("Error opening infile for reading\n");
            exit(errno);
        }
    }

    // Open the output file for writing:
    if(outfile != NULL)
    {
        outfile_ptr = fopen(outfile, "w");
        if(outfile_ptr == NULL)
        {
            printf("Error opening outfile for writing\n");
            exit(errno);
        }
    }
    

    if (pipe(fd) < 0)
        { err_sys("pipe error"); }

    if ((child_pid = fork()) < 0)
    {
        err_sys("fork error");
    }
    else if (child_pid > 0)                       // parent
    {
        close(fd[0]);                             // close the unused read end, this will only be writing.
        P1_actions(infile_ptr, infile, fd[1]);        // write to fd[1]
        if (waitpid(child_pid, NULL, 0) < 0)      // wait for child
            { err_sys("waitpid error"); }
    }
    else                                      // child
    {
        close(fd[1]);                             // close the unusued write end
        P2_actions(outfile_ptr, m_lines, m_count, fd[0]);         // read from fd[0]
    }

                                                  // make some noise
    printf(" 2: PID, PPID: %d %d\n", getpid(), getppid());

    return 0;
}


//--------------------------------------------------------------------------------

// print message and quit

void err_sys(char *msg)
{
    printf("error: PID %d, %s\n", getpid(), msg);
    exit(0);
}


//--------------------------------------------------------------------------------

// write to fd

int P1_actions(FILE *infile_ptr, char *infile, int fd)
{
/* If you want to use read() and write(), this works:
 *
 * write(fd, "hello world\n", 12);
 */

    // Open up the write end of the first pipe for writing
    FILE *fp = fdopen(fd, "w");                   // use fp as if it had come from fopen()
    if (fp == NULL)
        { err_sys("fdopen(w) error"); }

// The following is so we don't need to call fflush(fp) after each fprintf().
// The default for a pipe-opened stream is full buffering, so we switch to line
//   buffering.
// But, we need to be careful not to exceed BUFFER_SIZE characters per output
//   line, including the newline and null terminator.
    static char buffer[BUFFER_SIZE];          // off the stack, always allocated
                                                  // set fp to line-buffering
    int ret = setvbuf(fp, buffer, _IOLBF, BUFFER_SIZE);
    if (ret != 0)
        { err_sys("setvbuf error (parent)"); }

    // Read the lines from the infile, put them into fp:
    char line[SMALL_LINE]; // Allocate space to hold each line

    int line_count = 0;
    while(fgets(line, SMALL_LINE, infile_ptr) != NULL)
    {
        fprintf(fp, "%s", line);
        line_count++;
    }

    int size = ftell(infile_ptr);
    printf("P1: file %s, bytes %d\n", infile, size);

    // Close the write stream, we're done with it.
    close(fd);

    return line_count;
}


//--------------------------------------------------------------------------------

// read from fd

void P2_actions(FILE *outfile_ptr, char **m_lines, int m_count, int fd)
{
    // We will need to fork then pipe again, so create some necessary variables:
    pid_t child_pid;
    int second_fd[2];

    if(pipe(second_fd) < 0)
    {
        err_sys("pipe error");
    }

    if((child_pid = fork()) < 0)
    {
        err_sys("fork error");
    }

    else if(child_pid > 0) // We're in the parent
    {
        close(second_fd[0]); // Close the unused read end
        
        FILE *fp = fdopen(fd, "r");
        if (fp == NULL)
            { err_sys("P2 fdopen(r) error"); }

        FILE *second_pipe_fp = fdopen(second_fd[1], "w");
        if(second_pipe_fp == NULL)
        {
            err_sys("P2 fdopen(w) error");
        }

        static char buffer[BUFFER_SIZE];
        int ret = setvbuf(fp, buffer, _IOLBF, BUFFER_SIZE);
        if (ret != 0)
            { err_sys("setvbuf error (P2 fp)"); }

        int ret2 = setvbuf(second_pipe_fp, buffer, _IOLBF, BUFFER_SIZE);
        if (ret2 != 0)
            { err_sys("setvbuf error (P2 second fp)"); }

        int match_lines; // Holds number of lines we have found a match on.
        int match_count; // Holds number of matches we have total.
        bool found_match;// Boolean tells if match was found or not.

        int i, j;
        char **lines;
        lines = malloc(sizeof(int*) * SMALL_LINE);

        char holder[BUFFER_SIZE];
        for(i = 0; fgets(holder, BUFFER_SIZE, fp) != NULL; i++)
        {
            lines[i] = strdup(holder);
        }

        char *pch;
        /*for(i = 0; lines[i] != NULL; i++)
        {
            printf("test '%s'\n", lines[i]);
        }*/

        for(i = 0; i < m_count; i++)
        {
            match_lines = 0;
            match_count = 0;

            //printf("Looking for %s...\n", m_lines[i]);
            for(j = 0; lines[j] != NULL; j++)
            {
                found_match = false;
                //printf("Looking at line %s...\n", lines[j]);
                pch = strstr(lines[j], m_lines[i]);
                while(pch != NULL)
                {
                    //printf("Found match! It's '%s'\n", pch);
                    // Increase the counter to tell how many matches we have for this string.
                    match_count++;
                    found_match = true;
                    //printf("changed to '%s'\n", pch+strlen(m_lines[i]));
                    pch = strstr(pch+strlen(m_lines[i]), m_lines[i]);
                }

                if(found_match) 
                {
                    match_lines++;
                }
            }
            printf("P2: string %s, lines %d, matches %d\n", m_lines[i], match_lines, match_count);
            fprintf(second_pipe_fp, "%s", lines[i]);
        }
        //if (p == NULL)                                // error or end-of-file; for this program, it's an error
            //{ err_sys("fgets error"); }

        for(i = 0; lines[i] != NULL; i++)
            free(lines[i]);

        close(second_fd[1]);
        if (waitpid(child_pid, NULL, 0) < 0)      // wait for child
            { err_sys("waitpid error"); }
    }

    else // This is the child to the most recent fork. 
    {
        close(second_fd[1]); // Close the unused write end
        P3_actions(outfile_ptr, second_fd[0]);
    }

}

void P3_actions(FILE *outfile_ptr, int second_fd)
{
    FILE *fp = fdopen(second_fd, "r");
    if(fp == NULL)
    {
        err_sys("fdopen(second r) error");
    }

    static char buffer[BUFFER_SIZE];

    int ret = setvbuf(fp, buffer, _IOLBF, BUFFER_SIZE);
    if (ret != 0)
        { err_sys("setvbuf error (second_child)"); }

    char line[BUFFER_SIZE];

    printf("Right before fgets in P3\n");
    while(fgets(line, BUFFER_SIZE, fp) != NULL)
    {
        fprintf(outfile_ptr, "%s", line);
    }

}

//--------------------------------------------------------------------------------
