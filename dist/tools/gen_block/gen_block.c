#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static char _get_char(unsigned i)
{
	i %= 2 * (1 + 'z' - 'a') + 10;

	if (i < 10) {
		return '0' + i;
	}
	i -= 10;

	if (i <= 'z' - 'a') {
		return 'a' + i;
	}
	i -= 1 + 'z' - 'a';

	return 'A' + i;
}

static void _write_block(int fd, unsigned bs, unsigned i)
{
	char block[bs];
	char *buf = block;

	buf += snprintf(buf, bs, "|%03u|", i);

	memset(buf, _get_char(i), &block[bs] - buf);
	block[bs - 1] = '\n';

	write(fd, block, bs);
}

int main(int argc, char **argv)
{
	unsigned blocksize = 64;
	unsigned blocks = 512;
	int fd = STDOUT_FILENO;

	char c;
	while ((c = getopt(argc, argv, "b:o:n:")) != -1) {
		switch (c) {
		case 'o':
			fd = open(optarg, O_CREAT | O_TRUNC | O_WRONLY, 0644);
			break;
		case 'b':
			blocksize = atoi(optarg);
			break;
		case 'n':
			blocks = atoi(optarg);
			break;
		default:
			return 1;
		}
	}

	if (fd < 0) {
		return fd;
	}

	for (unsigned i = 0; i < blocks; ++i) {
		_write_block(fd, blocksize, i);
	}

	close(fd);
}
