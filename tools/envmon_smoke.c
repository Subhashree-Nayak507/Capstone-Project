/* envmon_smoke.c - user-space smoke test for the kernel driver (needs /dev/envmon).
 * Build: gcc -Wall -I../include envmon_smoke.c -o envmon_smoke
 * Checks: ioctl set/get period, blocking read, sequence continuity,
 *         O_NONBLOCK -> EAGAIN, poll() readiness, bad ioctl -> EINVAL/ENOTTY,
 *         odd-sized read -> EINVAL, RESET and GET_STATS.                      */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "envmon_ioctl.h"

static int failures;
#define CHECK(c) do { if (!(c)) { ++failures; printf("  FAIL line %d: %s (errno=%d)\n", __LINE__, #c, errno); } \
                      else printf("  ok   %s\n", #c); } while (0)

int main(int argc, char **argv)
{
	const char *path = argc > 1 ? argv[1] : "/dev/envmon";
	struct envmon_sample s[8];
	struct envmon_stats st;
	unsigned int p = 50, got = 0;
	int fd = open(path, O_RDONLY);

	if (fd < 0) { perror(path); return 2; }

	CHECK(ioctl(fd, ENVMON_IOC_SET_PERIOD, &p) == 0);
	CHECK(ioctl(fd, ENVMON_IOC_GET_PERIOD, &got) == 0 && got == 50);
	p = 1;
	CHECK(ioctl(fd, ENVMON_IOC_SET_PERIOD, &p) == -1 && errno == EINVAL);
	CHECK(ioctl(fd, _IO('Z', 99)) == -1 && errno == ENOTTY);
	CHECK(ioctl(fd, ENVMON_IOC_RESET) == 0);

	CHECK(read(fd, s, 7) == -1 && errno == EINVAL);          /* smaller than one sample */

	ssize_t n = read(fd, s, sizeof(s[0]) * 5);               /* blocks until data */
	CHECK(n > 0 && n % sizeof(s[0]) == 0);
	for (int i = 1; i < n / (int)sizeof(s[0]); i++)
		CHECK(s[i].seq == s[i - 1].seq + 1 || (s[i].flags & ENVMON_F_OVERRUN));
	printf("  first sample: seq=%u temp=%.3fC hum=%.3f%%\n", s[0].seq,
	       s[0].temp_mC / 1000.0, s[0].humidity_mpct / 1000.0);

	struct pollfd pf = { fd, POLLIN, 0 };
	CHECK(poll(&pf, 1, 1000) == 1 && (pf.revents & POLLIN));

	int fl = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, fl | O_NONBLOCK);
	while (read(fd, s, sizeof(s)) > 0) ;                      /* drain */
	CHECK(read(fd, s, sizeof(s[0])) == -1 && errno == EAGAIN);

	CHECK(ioctl(fd, ENVMON_IOC_GET_STATS, &st) == 0 && st.period_ms == 50);
	printf("  stats: generated=%llu dropped=%llu depth=%u period=%u\n",
	       (unsigned long long)st.generated, (unsigned long long)st.dropped, st.buf_depth, st.period_ms);

	close(fd);
	printf("%s (%d failure(s))\n", failures ? "SMOKE TEST FAILED" : "SMOKE TEST PASSED", failures);
	return failures ? 1 : 0;
}
