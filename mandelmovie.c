/**********************************************
*  Filename: mandelmovie.c
*  Description: creates mandel movie 
*  Author: Ava Latoria
*  Date: 11/12/26
**********************************************/

#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define DEFAULT_FRAMES 50
#define DEFAULT_WIDTH 1000
#define DEFAULT_HEIGHT 1000
#define DEFAULT_MAX_ITERATIONS 1000
#define OUTPUT_PATH_SIZE 4096

static void show_help(const char *program);
static int parse_positive_int(const char *text, const char *option_name);
static pid_t start_frame(int frame, int frame_count, int width, int height,
                         int max_iterations, const char *prefix);

int main(int argc, char *argv[])
{
    int process_count = 0;
    int frame_count = DEFAULT_FRAMES;
    int width = DEFAULT_WIDTH;
    int height = DEFAULT_HEIGHT;
    int max_iterations = DEFAULT_MAX_ITERATIONS;
    const char *prefix = "mandel";
    int option;

    while ((option = getopt(argc, argv, "p:f:W:H:m:o:h")) != -1) {
        switch (option) {
        case 'p':
            process_count = parse_positive_int(optarg, "-p");
            break;
        case 'f':
            frame_count = parse_positive_int(optarg, "-f");
            break;
        case 'W':
            width = parse_positive_int(optarg, "-W");
            break;
        case 'H':
            height = parse_positive_int(optarg, "-H");
            break;
        case 'm':
            max_iterations = parse_positive_int(optarg, "-m");
            break;
        case 'o':
            if (optarg[0] == '\0') {
                fprintf(stderr, "mandelmovie: -o requires a nonempty prefix\n");
                return EXIT_FAILURE;
            }
            prefix = optarg;
            break;
        case 'h':
            show_help(argv[0]);
            return EXIT_SUCCESS;
        default:
            show_help(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (process_count == 0) {
        fprintf(stderr, "mandelmovie: the -p option is required\n\n");
        show_help(argv[0]);
        return EXIT_FAILURE;
    }
    if (optind != argc) {
        fprintf(stderr, "mandelmovie: unexpected argument: %s\n", argv[optind]);
        return EXIT_FAILURE;
    }
    if (process_count > frame_count) {
        fprintf(stderr,
                "mandelmovie: process count (%d) cannot exceed frame count (%d)\n",
                process_count, frame_count);
        return EXIT_FAILURE;
    }

    printf("Generating %d frames with at most %d child processes...\n",
           frame_count, process_count);
    fflush(stdout);

    int next_frame = 0;
    int active_children = 0;
    int failed_frames = 0;
    int fork_failed = 0;

    while (next_frame < frame_count || active_children > 0) {
        while (!fork_failed && next_frame < frame_count &&
               active_children < process_count) {
            pid_t child = start_frame(next_frame, frame_count, width, height,
                                      max_iterations, prefix);
            if (child < 0) {
                fprintf(stderr, "mandelmovie: fork failed: %s\n",
                        strerror(errno));
                fork_failed = 1;
                break;
            }
            ++next_frame;
            ++active_children;
        }

        if (active_children == 0) {
            break;
        }

        int status;
        pid_t finished;
        do {
            finished = wait(&status);
        } while (finished < 0 && errno == EINTR);

        if (finished < 0) {
            fprintf(stderr, "mandelmovie: wait failed: %s\n", strerror(errno));
            return EXIT_FAILURE;
        }

        --active_children;
        if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
            if (WIFEXITED(status)) {
                fprintf(stderr, "mandelmovie: child %ld exited with status %d\n",
                        (long)finished, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                fprintf(stderr, "mandelmovie: child %ld was killed by signal %d\n",
                        (long)finished, WTERMSIG(status));
            }
            ++failed_frames;
        }
    }

    if (fork_failed || failed_frames != 0) {
        fprintf(stderr,
                "mandelmovie: generation failed (%d child failure%s, %d of %d frames started)\n",
                failed_frames, failed_frames == 1 ? "" : "s", next_frame,
                frame_count);
        return EXIT_FAILURE;
    }

    printf("Finished all %d frames.\n", frame_count);
    return EXIT_SUCCESS;
}

// Convert a command-line value while rejecting partial, zero, and huge values.
static int parse_positive_int(const char *text, const char *option_name)
{
    char *end;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || value <= 0 ||
        value > INT_MAX) {
        fprintf(stderr, "mandelmovie: %s requires a positive integer\n",
                option_name);
        exit(EXIT_FAILURE);
    }
    return (int)value;
}

// Fork one child and have it render its assigned frame with mandel.
static pid_t start_frame(int frame, int frame_count, int width, int height,
                         int max_iterations, const char *prefix)
{
    pid_t child = fork();
    if (child != 0) {
        return child;
    }

    // Exponential scaling gives a smooth zoom from the full set to a detail.
    const double initial_scale = 4.0;
    const double final_scale = 0.0005;
    const double progress = frame_count == 1
                                ? 1.0
                                : (double)frame / (double)(frame_count - 1);
    const double scale = initial_scale * pow(final_scale / initial_scale,
                                              progress);

    char scale_text[32];
    char width_text[32];
    char height_text[32];
    char iterations_text[32];
    char output_file[OUTPUT_PATH_SIZE];

    snprintf(scale_text, sizeof(scale_text), "%.12g", scale);
    snprintf(width_text, sizeof(width_text), "%d", width);
    snprintf(height_text, sizeof(height_text), "%d", height);
    snprintf(iterations_text, sizeof(iterations_text), "%d", max_iterations);
    if (snprintf(output_file, sizeof(output_file), "%s%d.jpg", prefix, frame) >=
        (int)sizeof(output_file)) {
        fprintf(stderr, "mandelmovie: output filename is too long\n");
        _exit(EXIT_FAILURE);
    }

    execl("./mandel", "mandel",
          "-x", "0.286932",
          "-y", "0.014287",
          "-s", scale_text,
          "-W", width_text,
          "-H", height_text,
          "-m", iterations_text,
          "-o", output_file,
          (char *)NULL);

    fprintf(stderr, "mandelmovie: could not run ./mandel: %s\n",
            strerror(errno));
    _exit(127);
}

static void show_help(const char *program)
{
    printf("Usage: %s -p <processes> [options]\n", program);
    printf("\nOptions:\n");
    printf("  -p <count>   Maximum simultaneous children (required)\n");
    printf("  -f <count>   Number of frames (default: %d)\n", DEFAULT_FRAMES);
    printf("  -W <pixels>  Frame width (default: %d)\n", DEFAULT_WIDTH);
    printf("  -H <pixels>  Frame height (default: %d)\n", DEFAULT_HEIGHT);
    printf("  -m <count>   Maximum iterations per pixel (default: %d)\n",
           DEFAULT_MAX_ITERATIONS);
    printf("  -o <prefix>  Output filename prefix (default: mandel)\n");
    printf("  -h           Show this help text\n");
}
