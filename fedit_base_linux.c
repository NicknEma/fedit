#ifndef FEDIT_BASE_LINUX_C
#define FEDIT_BASE_LINUX_C

////////////////////////////////
//~ Memory

static void *
mem_reserve(u64 size) {
	void *result = mmap(0, size, 0, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
	assert(result != NULL);
	
	return result;
}

static void *
mem_commit(void *ptr, u64 size) {
	int r = mprotect(ptr, size, PROT_READ|PROT_WRITE);
	assert(r != -1);
	
	return ptr;
}

static void *
mem_reserve_and_commit(u64 size) {
	return mem_commit(mem_reserve(size), size);
}

static bool
mem_decommit(void *ptr, u64 size) {
	return (mprotect(ptr, size, 0) != -1 &&
			madvise(ptr, size, MADV_FREE) != -1);
}

static bool
mem_release(void *ptr, u64 size) {
	return munmap(ptr, size) != -1;
}

////////////////////////////////
//~ Console IO

static void
write_console_unbuffered(String s) {
	if (s.len > 0) {
		int nwrite = write(STDOUT_FILENO, s.data, s.len);
		if (nwrite != -1) {
			if (nwrite < s.len) {
				nwrite = write(STDOUT_FILENO, s.data + nwrite, s.len - nwrite);
				if (nwrite == -1) {
					panic(errno);
				}
			}
		} else {
			panic(errno);
		}
	} else {
		// If writing with a length of 0 to something other than a regular file
		// the result of write() is unspecified, so we better not do that.
	}
}

#endif
