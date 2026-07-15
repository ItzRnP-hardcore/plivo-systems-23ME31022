#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>

#ifndef OFFSET
#define OFFSET 2
#endif

int main(void) {
    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in in_addr = {0};
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(47010);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47010");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET;
    relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];
    unsigned char history[256][160];

    long long frames = 0;
    long long total_sent = 0;

    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n <= 0) continue;
        
        frames++;
        
        uint32_t seq;
        memcpy(&seq, buf, 4);
        uint32_t h_seq = ntohl(seq);
        
        memcpy(history[h_seq % 256], buf + 4, 160);

        double max_budget = frames * 160.0 * 1.98;
        
        if (h_seq >= OFFSET && (total_sent + 324) <= max_budget) {
            unsigned char out_buf[324];
            memcpy(out_buf, buf, 164);
            memcpy(out_buf + 164, history[(h_seq - OFFSET) % 256], 160);
            sendto(out_fd, out_buf, 324, 0, (struct sockaddr *)&relay, sizeof relay);
            total_sent += 324;
        } else {
            sendto(out_fd, buf, 164, 0, (struct sockaddr *)&relay, sizeof relay);
            total_sent += 164;
        }
    }
    return 0;
}
