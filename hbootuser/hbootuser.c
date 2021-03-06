#include <sys/types.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <linux/limits.h>
#include "hboot.h"
#define MAX_SOURCE_LEN 32

#define _STR(x) #x
#define STR(x) _STR(x)

#define perror(x...) \
	fprintf(stderr, x); \
	if (errno) fprintf(stderr, " err %d\n", errno); \
	else fprintf(stderr, "\n"); \
	errno = 0;

int handle_file(FILE *fp, int tag, int *buf);
int handle_nand(FILE *fp, int tag, int *buf);

static int ctrlfd;
static int buffers[MAX_BUFFERS_COUNT];
static int loader = 0;

struct source_type {
	const char *name;
	int (*handle)(FILE *fp, int tag, int *buf);
};
struct source_type g_sources[] = {
	{"file", handle_file},
	{"nand", handle_nand},
	{NULL  , NULL       },
};

int open_ctrl() {
	ctrlfd = open("/dev/hbootctrl", O_RDWR);
	if (ctrlfd < 0) {
		perror("open");
	}
	return ctrlfd;
}

size_t fdsize(int fd) {
	off_t seek = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	return (size_t)seek;
}

int try_file(const char *path, size_t *size) {
	int fd;

	if (access(path, R_OK) != 0) {
		perror("access");
		return -1;
	}
	if ((fd = open(path, O_RDONLY)) < 0) {
		perror("open");
		return fd;
	}
	if ((*size = fdsize(fd)) == (size_t)-1) {
		perror("fdsize");
		close(fd);
		return -1;
	}
	return fd;
}

int allocate_buffer(int tag, size_t size, int type, int attrs, uint32_t rest) {
	struct hboot_buffer_req req;
	int ret;
	printf("Enter to allocate_buffer, ctrlfd=%d \n", ctrlfd);
	req.tag = (uint8_t)tag;
	req.attrs = (uint8_t)attrs;
	req.type = (uint8_t)type;
	req.size = (uint32_t)size;
	req.rest = rest;

	if ((ret = ioctl(ctrlfd, HBOOT_ALLOCATE_BUFFER, &req)) < 0) {
		perror("allocate_buffer error, ioctl");
	}
	printf("ret of allocate_buffer %d \n", ret);
	return ret;
}

int free_buffer(int buffer) {
	int ret;
	printf("Enter to free_buffer, buffer=%d, ctrlfd=%d \n",buffer, ctrlfd);
	if ((ret = ioctl(ctrlfd, HBOOT_FREE_BUFFER, buffer)) < 0) {
		perror("free_buffer error, ioctl");
	}
	return ret;
	printf("ret of free_buffer %d \n", ret);
}

int select_buffer(int buffer) {
	int ret;
	printf("Enter to select_buffer, buffer=%d, ctrlfd=%d  \n",buffer, ctrlfd);
	if ((ret = ioctl(ctrlfd, HBOOT_SELECT_BUFFER, buffer)) < 0) {
		perror("select_buffer error, ioctl");
		printf("ret of select_buffer %d \n",ret);
	}
	printf("ret of select_buffer %d \n", ret);
	return ret;
}

int fd2fd(int in, size_t size, int out) {
	char buf[4096];
	size_t written = 0;
	int ret;
	while ((written < size) && ((ret = read(in, buf, sizeof buf)) > 0)) {
		if (written + ret > size) {
			ret = size - written;
		}
		if (write(out, buf, ret) < ret) {
			perror("write()");
			return -1;
		}
		written += ret;
	}
	return 0;
}

void boot(int handle) {
	int rv = ioctl(ctrlfd, HBOOT_BOOT, handle);
	sleep(5);
	printf("HBOOT_BOOT ioctl %d \n", rv);
}

int handle_file2(const char *fname, int tag, int *buf) {
	int btype;
	int battrs;
	size_t filesize;
	int fd;

	// Dont try to load next files if previous was bad
	if (loader == INVALID_BUFFER_HANDLE) {
		return -2;
	}
	loader = INVALID_BUFFER_HANDLE;

	if ((fd = try_file(fname, &filesize)) < 0) {
		perror("open(%s)", fname);
		return -1;
	}

//	if ((tag == 0) || (filesize < 4*4096)) {
	if (tag == 0) {
		btype = B_TYPE_PLAIN;
	} else {
		btype = B_TYPE_SCATTERED;
	}
	if (tag == 0) {
		battrs = 0;
	} else {
		battrs = B_ATTR_VERIFY;
	}
	if ((*buf = allocate_buffer(tag, filesize, btype, battrs, 0)) == INVALID_BUFFER_HANDLE) {
		fprintf(stderr, "failed to allocate buffer\n");
		close(fd);
		return -1;
	}
	select_buffer(*buf);
	printf("select_buffer(*buf)\n");
	if (fd2fd(fd, filesize, ctrlfd) < 0) {
		fprintf(stderr, "failed to copy file contents\n");
		close(fd);
		free_buffer(*buf);
		return -1;
	}
	printf("loaded file %s\n", fname);
	close(fd);

	// mark as loaded succesfully
	loader = tag;

	return 0;
}

int handle_file(FILE *fp, int tag, int *buf) {
	char fname[PATH_MAX+1];
	int btype;
	int battrs;
	size_t filesize;
	int fd;

	fscanf(fp, "%" STR(PATH_MAX) "s", fname);
	if ((fd = try_file(fname, &filesize)) < 0) {
		perror("open()");
		return -1;
	}

	if ((tag == 0) || (filesize < 4*4096)) {
		btype = B_TYPE_PLAIN;
	} else {
		btype = B_TYPE_SCATTERED;
	}
	if (tag == 0) {
		battrs = 0;
	} else {
		battrs = B_ATTR_VERIFY;
	}
	if ((*buf = allocate_buffer(tag, filesize, btype, battrs, 0)) == INVALID_BUFFER_HANDLE) {
		fprintf(stderr, "failed to allocate buffer\n");
		close(fd);
		return -1;
	}
	select_buffer(*buf);
	if (fd2fd(fd, filesize, ctrlfd) < 0) {
		fprintf(stderr, "failed to copy file contents\n");
		close(fd);
		free_buffer(*buf);
		return -1;
	}
	printf("loaded file %s\n", fname);
	close(fd);
	return 0;
}

int handle_nand(FILE *fp, int tag, int *buf) {
	unsigned int offset;
	unsigned int size;

	if (fscanf(fp, "%u@0x%x", &size, &offset) != 2) {
		fprintf(stderr, "can't parse data\n");
		return -1;
	}
	if ((*buf = allocate_buffer(tag, size, B_TYPE_NAND, 0, (uint32_t)offset)) == INVALID_BUFFER_HANDLE) {
		fprintf(stderr, "failed to allocate buffer\n");
		return -1;
	}
	printf("loaded nand %x@%x\n", size, offset);
	return 0;
}

int main(int argc, char **argv) {
	FILE *fp = NULL;
	int n = MAX_BUFFERS_COUNT;
	while (n--) {
		buffers[n] = INVALID_BUFFER_HANDLE;
	}

	if (open_ctrl() < 0) {
		perror("Unable to open hboot control device !");
		return 1;
	}

/*	if (argc != 2) {
		fprintf(stderr, "Usage: %s <descriptor>\n", argv[0]);
		return 1;
	}
	if ((fp = fopen(argv[1], "r")) == NULL) {
		perror("open(descriptor file)");
		return 1;
	}

	while (buffers_count < MAX_BUFFERS_COUNT) {
		int tag;
		int ret;
		char source[MAX_SOURCE_LEN+1];
		struct source_type *s_type;

		ret = fscanf(fp, "%d", &tag);
		if (ret == EOF) {
			if (errno) {
				perror("fscanf()");
				goto out;
			}
			break;
		} else if (ret != 1) {
			fscanf(fp, "%*[^\n]");
			continue;
		}
		fscanf(fp, "%" STR(MAX_SOURCE_LEN) "s", source);
		for (s_type = g_sources; s_type->name != NULL; ++s_type) {
			if (!strcmp(s_type->name, source)) {
				if (s_type->handle(fp, tag, &buffers[buffers_count]) < 0) {
					goto out;
				}
				if (tag == 0) {
					loader = buffers[buffers_count];
				}
				buffers_count++;
				break;
			}
		}
		if (s_type->name == NULL) {
			printf("don't know how to handle source %s\n", source);
			goto out;
		}
		fscanf(fp, "%*[^\n]");
	}
*/
	n = 0;
	handle_file2("/system/2ndboot/boot",       0, &buffers[n++]);
	handle_file2("/system/2ndboot/kernel",     1, &buffers[n++]);
	handle_file2("/system/2ndboot/ramdisk",    2, &buffers[n++]);
	handle_file2("/system/2ndboot/devtree",    3, &buffers[n++]);
	handle_file2("/system/2ndboot/cmdline",    4, &buffers[n++]);

	if (fp) fclose(fp);
	fp = NULL;
	if (loader < 4) {
		perror("Unable to load all parts");
		goto out;
	}
	printf("Everything is loaded, booting ^^\n");
	fflush(stdout);
	sleep(2);
	boot(loader);
out:
	printf("exiting....\n");
	if (fp) fclose(fp);
	n = MAX_BUFFERS_COUNT;
	while (n--) {
		if (buffers[n] != INVALID_BUFFER_HANDLE) {
			free_buffer(buffers[n]);
			buffers[n] = INVALID_BUFFER_HANDLE;
		}
	}
	return 1;
}
