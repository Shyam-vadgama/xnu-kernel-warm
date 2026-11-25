/*
 * First user-space program for the Darwin-on-redwood bring-up.
 *
 * A static arm64 Mach-O with no libc and no dyld: it talks to the kernel with
 * raw system calls. The kernel's mockfs presents the ramdisk (this very file)
 * as /sbin/launchd, so this runs as process 1.
 */
typedef unsigned long u64;

static long last_errno;

/* XNU arm64 system call: number in x16, arguments in x0.., "svc #0x80"; carry set on error with positive errno in x0 */
static long
syscall3(long num, long a, long b, long c)
{
	register long x16 __asm__("x16") = num;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	unsigned int carry = 0;
	__asm__ volatile (
	    "svc #0x80\n"
	    "cset %w1, cs\n"
	    : "+r" (x0), "=&r" (carry)
	    : "r" (x16), "r" (x1), "r" (x2)
	    : "memory", "cc");
	if (carry) {
		last_errno = x0;
		return -1;
	}
	last_errno = 0;
	return x0;
}

/* the same with six arguments, for sysctlbyname(name, namelen, old, &oldlen, new, newlen) */
static long
syscall6(long num, long a, long b, long c, long d, long e, long f)
{
	register long x16 __asm__("x16") = num;
	register long x0 __asm__("x0") = a;
	register long x1 __asm__("x1") = b;
	register long x2 __asm__("x2") = c;
	register long x3 __asm__("x3") = d;
	register long x4 __asm__("x4") = e;
	register long x5 __asm__("x5") = f;
	unsigned int carry = 0;
	__asm__ volatile (
	    "svc #0x80\n"
	    "cset %w1, cs\n"
	    : "+r" (x0), "=&r" (carry)
	    : "r" (x16), "r" (x1), "r" (x2), "r" (x3), "r" (x4), "r" (x5)
	    : "memory", "cc");
	if (carry) {
		last_errno = x0;
		return -1;
	}
	last_errno = 0;
	return x0;
}

#define SYS_exit    1
#define SYS_read    3
#define SYS_write   4
#define SYS_open    5
#define SYS_close   6
#define SYS_getpid  20
#define SYS_getuid  24
#define SYS_select  93
#define SYS_sysctlbyname 274
#define O_RDONLY    0
#define O_RDWR      2

static unsigned
length(const char *s)
{
	unsigned n = 0;
	while (s[n]) {
		n++;
	}
	return n;
}

static void
put(int fd, const char *s)
{
	syscall3(SYS_write, fd, (long)s, length(s));
}

static void
put_number(int fd, long v)
{
	char buf[24];
	int i = sizeof(buf);
	unsigned long u;
	if (v < 0) {
		put(fd, "-");
		u = (unsigned long)(-(v + 1)) + 1;
	} else {
		u = (unsigned long)v;
	}
	buf[--i] = 0;
	do {
		buf[--i] = (char)('0' + u % 10);
		u /= 10;
	} while (u);
	put(fd, buf + i);
}

static void
put_hex(int fd, const unsigned char *data, int len)
{
	static const char hex[] = "0123456789abcdef";
	char out[64];
	int pos = 0;
	for (int i = 0; i < len && pos + 3 < (int)sizeof(out); i++) {
		if (i > 0) {
			out[pos++] = ' ';
		}
		out[pos++] = hex[(data[i] >> 4) & 0xf];
		out[pos++] = hex[data[i] & 0xf];
	}
	out[pos] = 0;
	put(fd, out);
}

/* "name=value" for a string sysctl, or nothing at all if the kernel has no such name (the driver is off) */
static void
show_string(int fd, const char *name)
{
	char value[1100];
	u64 size = sizeof(value);

	if (syscall6(SYS_sysctlbyname, (long)name, length(name), (long)value, (long)&size, 0, 0) == 0) {
		put(fd, name);
		put(fd, "=");
		put(fd, value);
		put(fd, "\n");
	}
}

/* ... for an int sysctl (four bytes) */
static void
show_int(int fd, const char *name)
{
	int value = 0;
	u64 size = sizeof(value);

	if (syscall6(SYS_sysctlbyname, (long)name, length(name), (long)&value, (long)&size, 0, 0) == 0) {
		put(fd, name);
		put(fd, "=");
		put_number(fd, value);
		put(fd, "\n");
	}
}

/* ... for a 64-bit quad sysctl (eight bytes) */
static void
show_quad(int fd, const char *name)
{
	u64 value = 0;
	u64 size = sizeof(value);

	if (syscall6(SYS_sysctlbyname, (long)name, length(name), (long)&value, (long)&size, 0, 0) == 0) {
		put(fd, name);
		put(fd, "=");
		put_number(fd, (long)value);
		put(fd, "\n");
	}
}

static int
probe_ufs_part(int fd, const char *path, const char *expected, int len)
{
	unsigned char buf[4096];
	int ufd = (int)syscall3(SYS_open, (long)path, O_RDONLY, 0);
	long open_err = last_errno;
	if (ufd < 0) {
		put(fd, "UFS: open ");
		put(fd, path);
		put(fd, " failed (errno=");
		put_number(fd, open_err);
		put(fd, ")\n");
		return 0;
	}
	long want = (path[5] == 'u') ? 4096 : (long)len;
	long r = syscall3(SYS_read, ufd, (long)buf, want);
	long rd_err = last_errno;
	syscall3(SYS_close, ufd, 0, 0);
	put(fd, "UFS: ");
	put(fd, path);
	if (r < 0) {
		put(fd, " read failed (errno=");
		put_number(fd, rd_err);
		put(fd, ")\n");
		return 0;
	}
	put(fd, " read=");
	put_number(fd, r);
	if (r < len) {
		put(fd, "\n");
		return 0;
	}
	put(fd, " hex=");
	put_hex(fd, buf, len);
	put(fd, " [");
	char safe[17];
	int n = len > 16 ? 16 : len;
	for (int i = 0; i < n; i++) {
		safe[i] = (buf[i] >= 0x20 && buf[i] < 0x7f) ? (char)buf[i] : '.';
	}
	safe[n] = 0;
	put(fd, safe);
	put(fd, "]");
	int match = 1;
	for (int i = 0; i < len; i++) {
		if (buf[i] != (unsigned char)expected[i]) {
			match = 0;
			break;
		}
	}
	put(fd, match ? " OK\n" : " MISMATCH\n");
	return match;
}

/* hello_wait=N in the kernel's command line (kern.bootargs): N, or 0 */
static long
wait_seconds(void)
{
	char args[700];
	u64 size = sizeof(args);
	long n = 0;

	if (syscall6(SYS_sysctlbyname, (long)"kern.bootargs", 13, (long)args, (long)&size, 0, 0) != 0) {
		return 0;
	}
	for (char *c = args; *c; c++) {
		if (c[0] == 'h' && c[1] == 'e' && c[2] == 'l' && c[3] == 'l' && c[4] == 'o' && c[5] == '_' && c[6] == 'w' &&
		    c[7] == 'a' && c[8] == 'i' && c[9] == 't' && c[10] == '=') {
			for (c += 11; *c >= '0' && *c <= '9'; c++) {
				n = n * 10 + (*c - '0');
			}
			return n;
		}
	}
	return 0;
}

void
start(void)
{
	int fd = (int)syscall3(SYS_open, (long)"/dev/console", O_RDWR, 0);
	if (fd < 0) {
		fd = 1;
	}

	put(fd, "\n========================================\n");
	put(fd, "Hello from user space on Darwin!\n");
	put(fd, "I am process ");
	put_number(fd, syscall3(SYS_getpid, 0, 0, 0));
	put(fd, ", uid ");
	put_number(fd, syscall3(SYS_getuid, 0, 0, 0));
	put(fd, ", console fd ");
	put_number(fd, fd);
	put(fd, ".\n");
	put(fd, "========================================\n\n");

	/*
	 * The kernel's own version: sysctl kern.hakios.version, which the loader gave the kernel on its command line
	 * (hakios_kver, from the payload's kernel.ver). The same string the banner shows under hello hakios-kernel!
	 */
	{
		char version[96];
		u64 size = sizeof(version);
		long failed = syscall6(SYS_sysctlbyname, (long)"kern.hakios.version", 19, (long)version, (long)&size, 0, 0);

		put(fd, "kernel.ver=");
		put(fd, failed ? "(sysctl kern.hakios.version not available)" : version);
		put(fd, "\n");
	}

	/* What the drivers that are on say about the chip, through the same door (a driver that is off has no such name) */
	show_string(fd, "kern.hakios.soc_name");
	show_int(fd, "kern.hakios.soc_id");
	show_int(fd, "kern.hakios.pmic_peripherals");
	show_string(fd, "kern.hakios.thermal_sensors");

	/* UFS Storage verification */
	put(fd, "\n[UFS Storage Probe]\n");
	int ok_a = probe_ufs_part(fd, "/dev/rufs_boot_a", "ANDROID!", 8);
	(void)probe_ufs_part(fd, "/dev/rufs_boot_b", "ANDROID!", 8);
	(void)probe_ufs_part(fd, "/dev/rufs_vbmeta_a", "AVB0", 4);
	int ok_blk = probe_ufs_part(fd, "/dev/ufs_boot_a", "ANDROID!", 8);
	show_quad(fd, "kern.hakios.ufs_requests");
	show_quad(fd, "kern.hakios.ufs_bytes");
	show_quad(fd, "kern.hakios.ufs_errors");
	if (ok_a && ok_blk) {
		put(fd, ">>> UFS Storage Verified: Read physical UFS flash (char & block)! <<<\n");
	} else if (!ok_a) {
		show_string(fd, "kern.bootargs");
	}

	put(fd, "\nDarwin userland initialized and running successfully!\n");
	put(fd, "Staying alive...\n");

	/* Keep process 1 running indefinitely so the system does not panic */
	for (long second = 0;; second++) {
		long tv[2] = { 10, 0 };
		syscall6(SYS_select, 0, 0, 0, 0, (long)tv, 0);
		if (second % 6 == 0) {
			show_int(fd, "kern.hakios.thermal_max_mc");
			show_string(fd, "kern.hakios.thermal_hottest");
		}
	}
}
