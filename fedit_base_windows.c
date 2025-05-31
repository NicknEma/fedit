#ifndef FEDIT_BASE_WINDOWS_C
#define FEDIT_BASE_WINDOWS_C

////////////////////////////////
//~ Memory

static void *
mem_reserve(u64 size) {
	// No need to align the size to a page boundary, Windows will do it for us.
	void *result = VirtualAlloc(NULL, size, MEM_RESERVE, PAGE_NOACCESS);
	assert(result != NULL);
	
	return result;
}

static void *
mem_commit(void *ptr, u64 size) {
	// No need to align the size to a page boundary, Windows will do it for us.
	void *result = VirtualAlloc(ptr, size, MEM_COMMIT, PAGE_READWRITE);
	assert(result != NULL);
	
	return result;
}

static void *
mem_reserve_and_commit(u64 size) {
	// No need to align the size to a page boundary, Windows will do it for us.
	void *result = VirtualAlloc(NULL, size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	assert(result != NULL);
	
	return result;
}

static bool
mem_decommit(void *ptr, u64 size) {
	return VirtualFree(ptr, size, MEM_DECOMMIT);
}

static bool
mem_release(void *ptr, u64 size) {
	(void)size; // Not needed on Windows
	
	return VirtualFree(ptr, 0, MEM_RELEASE);
}

////////////////////////////////
//~ Console IO

static void
write_console_unbuffered(String s) {
	i64 written = 0;
	
	HANDLE hstdout = GetStdHandle(STD_OUTPUT_HANDLE);
	while (written < s.len) {
		i64  desired_nwrite = s.len - written;
		u32  attempt_nwrite = cast(DWORD) desired_nwrite;
		DWORD actual_nwrite = 0;
		if (!WriteConsole(hstdout, s.data + written, attempt_nwrite, &actual_nwrite, NULL)) {
			int n = GetLastError();
			(void)n;
			
			panic();
		}
		
		written += cast(i64) actual_nwrite;
	}
}

#endif
