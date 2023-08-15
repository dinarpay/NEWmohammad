#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <time.h>
#include <pthread.h>

#define TARGETS_FILE "targets.txt"
#define FAST_TARGETS_FILE "fast_targets.txt"
#define SLOW_TARGETS_FILE "slow_targets.txt"
#define TIMEOUT 5L
#define THRESHOLD 1.0
#define NUM_THREADS 1000
#define TARGETS_PER_THREAD 10

pthread_mutex_t file_mutex;

void *check_targets(void *targets) {
    char **target_list = (char **)targets;
    for (int i = 0; i < TARGETS_PER_THREAD && target_list[i]; i++) {
        CURL *curl;
        CURLcode res;
        struct timespec start, end;
        char *target = target_list[i];

        curl = curl_easy_init();
        if (!curl) {
            fprintf(stderr, "Error initializing curl.\n");
            continue;
        }

        curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, TIMEOUT);
        curl_easy_setopt(curl, CURLOPT_URL, target);
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        clock_gettime(CLOCK_MONOTONIC, &start);
        res = curl_easy_perform(curl);
        clock_gettime(CLOCK_MONOTONIC, &end);

        double response_time = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;

        FILE *file;
        if (res != CURLE_OK) {
            fprintf(stderr, "Error with target %s: %s\n", target, curl_easy_strerror(res));
        } else if (response_time < THRESHOLD) {
            pthread_mutex_lock(&file_mutex);
            file = fopen(FAST_TARGETS_FILE, "a");
            fprintf(file, "%s\n", target);
            fclose(file);
            pthread_mutex_unlock(&file_mutex);
            printf("%s is fast (%f seconds)\n", target, response_time);
        } else {
            pthread_mutex_lock(&file_mutex);
            file = fopen(SLOW_TARGETS_FILE, "a");
            fprintf(file, "%s\n", target);
            fclose(file);
            pthread_mutex_unlock(&file_mutex);
            printf("%s is slow (%f seconds)\n", target, response_time);
        }

        curl_easy_cleanup(curl);
        free(target);
    }
    free(targets);
    return NULL;
}

int main() {
    FILE *targets_file = fopen(TARGETS_FILE, "r");
    char target[256];
    pthread_t threads[NUM_THREADS];
    char **current_targets = NULL;
    int thread_count = 0;
    int target_count = 0;

    if (!targets_file) {
        fprintf(stderr, "Error opening targets file.\n");
        return 1;
    }

    curl_global_init(CURL_GLOBAL_DEFAULT);
    pthread_mutex_init(&file_mutex, NULL);

    while (fgets(target, sizeof(target), targets_file)) {
        target[strcspn(target, "\n")] = 0; // Remove newline

        if (target_count == 0) {
            current_targets = calloc(TARGETS_PER_THREAD + 1, sizeof(char *));
        }

        current_targets[target_count] = strdup(target);
        target_count++;

        if (target_count == TARGETS_PER_THREAD) {
            pthread_create(&threads[thread_count], NULL, check_targets, current_targets);
            thread_count++;
            target_count = 0;
            current_targets = NULL;

            if (thread_count == NUM_THREADS) {
                for (int i = 0; i < NUM_THREADS; i++) {
                    pthread_join(threads[i], NULL);
                }
                thread_count = 0;
            }
        }
    }

    if (target_count > 0) {
        pthread_create(&threads[thread_count], NULL, check_targets, current_targets);
        thread_count++;
    }

    // Join any remaining threads
    for (int i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    curl_global_cleanup();
    pthread_mutex_destroy(&file_mutex);

    fclose(targets_file);

    printf("Scan completed.\n");

    return 0;
}
