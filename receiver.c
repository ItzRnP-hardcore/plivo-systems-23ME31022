/* RECEIVER — forwards media, decodes XOR FEC (with deferred retry), NACKs gaps.
 *
 * 324B packet at seq h carries X_h = P(h-2) XOR P(h-K). If exactly one of
 * the two covered frames is missing and the other is stored, recover it.
 * If both are missing, stash X and retry when a later arrival (media,
 * another XOR recovery, or a retransmit) supplies a partner. K <= 2 means
 * plain replication of P(h-K). NACKs fire the moment a gap is seen and
 * repeat every 25ms until the frame arrives or its deadline passes. */
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdint.h>

#define JITTER_HEADROOM_MS 80.0
#define MAXSEQ 65536
#define MAX_NACKS 4

static int derive_offset(void) {
    const char *d = getenv("DELAY_MS");
    double delay = d ? atof(d) : 120.0;
    int off = (int)((delay - JITTER_HEADROOM_MS) / 20.0);
    if (off < 1) off = 1;
    if (off > 8) off = 8;
    return off;
}

static double now_s(void) {
    struct timeval tv; gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec / 1e6;
}

static unsigned char pay[MAXSEQ][160];
static unsigned char got[MAXSEQ];        /* payload known (stored in pay) */
static unsigned char xbuf[MAXSEQ][160];  /* X carried by packet seq */
static unsigned char xgot[MAXSEQ];
static double last_nack[MAXSEQ];
static unsigned char nacks[MAXSEQ];

static int out_fd;
static struct sockaddr_in player;
static uint32_t K;

static void deliver(uint32_t j, const unsigned char *p);

/* try to decode X carried by packet h; returns 1 if it recovered a frame */
static int try_xor(uint32_t h) {
    if (!xgot[h] || h < K) return 0;
    uint32_t a = h - 2, b = h - K;   /* K > 2 guaranteed by caller */
    if (got[a] && !got[b]) {
        unsigned char p[160];
        for (int i = 0; i < 160; i++) p[i] = xbuf[h][i] ^ pay[a][i];
        deliver(b, p);
        return 1;
    }
    if (got[b] && !got[a]) {
        unsigned char p[160];
        for (int i = 0; i < 160; i++) p[i] = xbuf[h][i] ^ pay[b][i];
        deliver(a, p);
        return 1;
    }
    return 0;
}

static void deliver(uint32_t j, const unsigned char *p) {
    if (j >= MAXSEQ || got[j]) return;
    memcpy(pay[j], p, 160);
    got[j] = 1;
    unsigned char pkt[164];
    uint32_t nj = htonl(j);
    memcpy(pkt, &nj, 4);
    memcpy(pkt + 4, p, 160);
    sendto(out_fd, pkt, 164, 0, (struct sockaddr *)&player, sizeof player);
    if (K > 2) {           /* j may complete a stashed XOR as a partner */
        if (j + 2 < MAXSEQ) try_xor(j + 2);
        if (j + K < MAXSEQ) try_xor(j + K);
    }
}

int main(void) {
    K = (uint32_t)derive_offset();
    const char *e = getenv("T0");
    double t0 = e ? atof(e) : now_s();
    e = getenv("DELAY_MS");
    double delay_s = (e ? atof(e) : 120.0) / 1000.0;

    int in_fd = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_port = htons(47002);
    a.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (bind(in_fd, (struct sockaddr *)&a, sizeof a) < 0) { perror("bind 47002"); return 1; }

    out_fd = socket(AF_INET, SOCK_DGRAM, 0);
    player.sin_family = AF_INET; player.sin_port = htons(47020);
    player.sin_addr.s_addr = inet_addr("127.0.0.1");

    struct sockaddr_in fb = {0};
    fb.sin_family = AF_INET; fb.sin_port = htons(47003);
    fb.sin_addr.s_addr = inet_addr("127.0.0.1");

    unsigned char buf[2048];
    int64_t max_seq = -1;
    uint32_t scan_lo = 0;

    for (;;) {
        fd_set r; FD_ZERO(&r); FD_SET(in_fd, &r);
        struct timeval tv = {0, 10000};
        int rv = select(in_fd + 1, &r, NULL, NULL, &tv);

        if (rv > 0 && FD_ISSET(in_fd, &r)) {
            ssize_t n = recvfrom(in_fd, buf, sizeof buf, 0, NULL, NULL);
            if (n == 164 || n == 324) {
                uint32_t seq; memcpy(&seq, buf, 4);
                uint32_t h = ntohl(seq);
                if (h < MAXSEQ) {
                    if ((int64_t)h > max_seq) max_seq = h;
                    deliver(h, buf + 4);   /* forwards + stores + retries XORs */
                    if (n == 324 && h >= K) {
                        if (K > 2) {
                            memcpy(xbuf[h], buf + 164, 160);
                            xgot[h] = 1;
                            try_xor(h);
                        } else {
                            deliver(h - K, buf + 164);
                        }
                    }
                }
            }
        }

        double now = now_s();
        while (scan_lo < MAXSEQ && (int64_t)scan_lo < max_seq &&
               (got[scan_lo] || now > t0 + delay_s + scan_lo * 0.020))
            scan_lo++;
        /* NACK only frames whose fast XOR chance already failed: some
         * packet beyond j+2 has arrived and j is still missing. This keeps
         * retransmits to true double-losses instead of flooding the budget. */
        for (uint32_t j = scan_lo; (int64_t)j + 2 < max_seq && j < MAXSEQ; j++) {
            if (got[j] || nacks[j] >= MAX_NACKS) continue;
            if (now > t0 + delay_s + j * 0.020) continue;
            if (now - last_nack[j] < 0.040) continue;
            uint32_t nj = htonl(j);
            sendto(out_fd, &nj, 4, 0, (struct sockaddr *)&fb, sizeof fb);
            last_nack[j] = now;
            nacks[j]++;
        }
    }
    return 0;
}
