/* SENDER — dual-coverage XOR FEC + NACK retransmission.
 *
 * Redundant packet: [4B seq i][160B payload i][160B X_i] where
 *     X_i = P(i-2) XOR P(i-K),   K = clamp((DELAY_MS-80)/20, 1, 8)
 * Frame j is covered by X_{j+2} (fast repair, partner j+2-K is old and
 * almost surely delivered) and X_{j+K} (burst-safe repair, partner j+K-2
 * lies beyond a burst of length <= K-2). Two chances per frame for the
 * same 160 bytes plain replication spends on one. If K <= 2 the two lags
 * coincide, so fall back to plain replication of P(i-K).
 *
 * Feedback lane: 4-byte NACKs answered with a standard 164B packet, sent
 * twice (it targets a known loss and crosses the same lossy lane).
 * Budget: FEC attach gated at 1.97x, retransmits at 1.99x, so the 2.0x
 * cap holds even when retransmits cluster at the end of a run. */
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>
#include <stdint.h>

#define JITTER_HEADROOM_MS 80.0
#define HIST 1024

static int derive_offset(void) {
    const char *d = getenv("DELAY_MS");
    double delay = d ? atof(d) : 120.0;
    int off = (int)((delay - JITTER_HEADROOM_MS) / 20.0);
    if (off < 1) off = 1;
    if (off > 8) off = 8;
    return off;
}

int main(void) {
    uint32_t K = (uint32_t)derive_offset();

    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_port = htons(47010);
    a.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&a, sizeof a) < 0) { perror("bind 47010"); return 1; }

    int fb_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in f = {0};
    f.sin_family = AF_INET; f.sin_port = htons(47004);
    f.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(fb_fd, (struct sockaddr *)&f, sizeof f) < 0) { perror("bind 47004"); return 1; }

    int out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in relay = {0};
    relay.sin_family = AF_INET; relay.sin_port = htons(47001);
    relay.sin_addr.s_addr = inet_addr("127.0.0.1");

    static unsigned char history[HIST][160];
    static unsigned char have[HIST];
    static unsigned char rcount[65536];   /* retransmits already sent per seq */
    unsigned char buf[2048];
    long long frames = 0, total_sent = 0;
    uint32_t last_seq = 0;

    for (;;) {
        fd_set r; FD_ZERO(&r); FD_SET(in_fd, &r); FD_SET(fb_fd, &r);
        int mx = (in_fd > fb_fd ? in_fd : fb_fd) + 1;
        if (select(mx, &r, NULL, NULL, NULL) <= 0) continue;

        if (FD_ISSET(fb_fd, &r)) {
            ssize_t n = recvfrom(fb_fd, buf, sizeof buf, 0, NULL, NULL);
            if (n == 4) {
                uint32_t ns; memcpy(&ns, buf, 4);
                uint32_t j = ntohl(ns);
                double retx_budget = frames * 160.0 * 1.99;
                if (have[j % HIST] && j <= last_seq && last_seq - j < HIST &&
                    j < 65536 && rcount[j] < 3 &&
                    (total_sent + 328) <= retx_budget) {
                    rcount[j]++;
                    unsigned char pkt[164];
                    uint32_t nj = htonl(j);
                    memcpy(pkt, &nj, 4);
                    memcpy(pkt + 4, history[j % HIST], 160);
                    sendto(out_fd, pkt, 164, 0, (struct sockaddr *)&relay, sizeof relay);
                    sendto(out_fd, pkt, 164, 0, (struct sockaddr *)&relay, sizeof relay);
                    total_sent += 328;
                }
            }
        }

        if (FD_ISSET(in_fd, &r)) {
            ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
            if (n != 164) continue;
            frames++;
            uint32_t seq; memcpy(&seq, buf, 4);
            uint32_t h = ntohl(seq);
            memcpy(history[h % HIST], buf + 4, 160);
            have[h % HIST] = 1;
            last_seq = h;

            double max_budget = frames * 160.0 * 1.975;
            if (h >= K && (total_sent + 324) <= max_budget) {
                unsigned char ob[324];
                memcpy(ob, buf, 164);
                if (K > 2) {
                    unsigned char *p2 = history[(h - 2) % HIST];
                    unsigned char *pk = history[(h - K) % HIST];
                    for (int i = 0; i < 160; i++) ob[164 + i] = p2[i] ^ pk[i];
                } else {
                    memcpy(ob + 164, history[(h - K) % HIST], 160);
                }
                sendto(out_fd, ob, 324, 0, (struct sockaddr *)&relay, sizeof relay);
                total_sent += 324;
            } else {
                sendto(out_fd, buf, 164, 0, (struct sockaddr *)&relay, sizeof relay);
                total_sent += 164;
            }
        }
    }
    return 0;
}
