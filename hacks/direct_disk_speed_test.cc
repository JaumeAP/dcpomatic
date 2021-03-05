#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

int test(int block_size, int blocks, int gap, int rdisk)
{
	struct timeval start;
	gettimeofday(&start, NULL);

	FILE* f = NULL;
	if (rdisk) {
		f = fopen("/dev/rdisk3", "r+b");
     	} else {
		f = fopen("/dev/disk3", "r+b");
	}
        if (!f) {
		printf("fopen failed.\n");
		return -1;
	}

	setbuf(f, 0);

	void* buffer = malloc(block_size);
	if (!buffer) {
		printf("malloc failed.\n");
		return -1;
	}

	for (int i = 0; i < blocks; ++i) {
		fwrite(buffer, block_size, 1, f);
		if (gap > 0) {
			fseek(f, gap, SEEK_CUR);
		}
	}

	fclose(f);

	struct timeval end;
	gettimeofday(&end, NULL);
	float duration = ((end.tv_sec + end.tv_usec / 1e6) - (start.tv_sec + start.tv_usec / 1e6));
	printf("block_size=%d blocks=%d gap=%d rdisk=%d time=%f\n", block_size, blocks, gap, rdisk, duration);
	return 0;
}


int main()
{
	for (int i = 0; i < 2; i++) {
		test(4096, 4096, 0, i);
		test(4096, 4096, 0, i);
		test(8192, 2048, 0, i);
		test(4096, 4096, 4096, i);
		test(4096, 4096, 65536, i);
		test(8192, 2048, 65536, i);
		test(16384, 1024, 65536, i);
	}
}

