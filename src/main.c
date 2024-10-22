#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

// ANSI color codes
#define RESET "\033[0m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define RED "\033[31m"
#define BLUE "\033[34m"

const char *IA_URLS[] = {
    "https://archive.org",
    "https://blog.archive.org",
    "https://help.archive.org",
    "https://web.archive.org",
    "https://mastodon.archive.org",
    "https://openlibrary.org",
    "https://archive-it.org",
};

double calculate_response_time(struct timeval start, struct timeval end) {
    double seconds = (end.tv_sec - start.tv_sec);
    double microseconds = (end.tv_usec - start.tv_usec) / 1000000.0;
    return seconds + microseconds;
}

void print_timestamp() {
    time_t now;
    struct tm *local;
    char timestamp[80];

    time(&now);
    local = localtime(&now);

    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", local);
    printf("\nTimestamp: %s\n", timestamp);
}

void check_internet_archive() {
    CURL *curl;
    CURLcode res;
    long response_code;
    double total_time;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    print_timestamp();

    for (int i = 0; i < sizeof(IA_URLS) / sizeof(IA_URLS[0]); i++) {
        curl = curl_easy_init();
        if (curl) {
            struct timeval start, end;
            gettimeofday(&start, NULL);

            curl_easy_setopt(curl, CURLOPT_URL, IA_URLS[i]);
            curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // Only fetch the header skip the body
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // Timeout 10 seconds

            // Skip SSL verification as IA for some reason doesn't like to connect with SSL right now
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

            res = curl_easy_perform(curl);
            gettimeofday(&end, NULL);

            if (res == CURLE_OK) {
                curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
                curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total_time);

                double response_time = calculate_response_time(start, end);

                printf("\nChecking: %s\n", IA_URLS[i]);
                printf("Response Time: %.3f seconds\n", response_time);

                if (response_code == 200) {
                    // Success case - Healthy
                    printf(GREEN "Status: Healthy (200 OK)" RESET "\n");
                } else if (response_code == 301 || response_code == 302) {
                    // Redirection - Still functioning but not a true success
                    printf(YELLOW "Status: Redirect (301/302)\n");
                    printf("Explanation: The page has moved, but IA is probably functioning correctly.\n" RESET);
                } else if (response_code >= 400 && response_code < 500) {
                    // Client errors (4xx) - Site is considered down
                    printf(RED "Status: Down (Client Error %ld)\n", response_code);
                    printf("Explanation: There was a client-side error, possibly IA is unavailable.\n" RESET);
                } else if (response_code >= 500) {
                    // Server errors (5xx) - Site is considered down
                    printf(RED "Status: Down (Server Error %ld)\n", response_code);
                    printf("Explanation: There is a server-side error. IA may be serving the 'Temporarily Offline'/'Service Availability' screen.\n" RESET);
                } else {
                    // Handle any other codes as "down"
                    printf(RED "Status: Down (Unhandled Status Code: %ld)\n", response_code);
                    printf("Explanation: The status code is unknown or unhandled.\n" RESET);
                }

            } else if (res == CURLE_OPERATION_TIMEDOUT) {
                // Timeout - treat as site being down
                printf(BLUE "\nChecking: %s\n", IA_URLS[i]);
                printf("Status: Down (Timed Out)\n" RESET);
                printf("Explanation: The request to IA timed out. IA is likely down or overloaded.\n");
            } else {
                // Handle any other errors from curl
                printf(RED "\nError checking %s: %s\n" RESET, IA_URLS[i], curl_easy_strerror(res));
            }

            curl_easy_cleanup(curl);
        }
    }
    curl_global_cleanup();
}

void countdown_timer(int seconds) {
    printf("\nNext check in:\n");
    for (int i = seconds; i > 0; i--) {
        printf("\r%02d:%02d remaining", i / 60, i % 60);
        fflush(stdout);
        sleep(1);
    }
    printf("\n");
}

int main() {
    int check_interval = 1800;  // 30 minutes

    while (1) {
        check_internet_archive();
        countdown_timer(check_interval);  // Display the countdown before next check
    }
    return 0;
}
