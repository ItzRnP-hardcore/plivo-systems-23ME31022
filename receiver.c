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
    in_addr.sin_port = htons(47002);
    in_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&in_addr, sizeof in_addr) < 0) {
        perror("bind 47002");
        return 1;
    }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in player = {0};
    player.sin_family = AF_INET;
    player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];
    for (;;) {
        ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
        if (n <= 0) continue;
        
        if (n == 164 || n == 324) {
            sendto(out_fd, buf, 164, 0, (struct sockaddr *)&player, sizeof player);
        }
        
        if (n == 324) {
            uint32_t seq;
            memcpy(&seq, buf, 4);
            uint32_t h_seq = ntohl(seq);
            
            if (h_seq >= OFFSET) {
                uint32_t bseq = h_seq - OFFSET;
                uint32_t n_bseq = htonl(bseq);
                
                unsigned char pbuf[164];
                memcpy(pbuf, &n_bseq, 4);
                memcpy(pbuf + 4, buf + 164, 160);
                
                sendto(out_fd, pbuf, 164, 0, (struct sockaddr *)&player, sizeof player);
            }
        }
    }
    return 0;
}
