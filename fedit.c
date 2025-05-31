////////////////////////////////
//~ Context Crack: MSVC Extraction

#if defined(_MSC_VER)

# define COMPILER_MSVC 1

# if defined(_WIN32)
#  define OS_WINDOWS 1
# else
#  error _MSC_VER is defined, but _WIN32 is not. This setup is not supported.
# endif

////////////////////////////////
//~ Context Crack: Clang Extraction

#elif defined(__clang__)

# define COMPILER_CLANG 1

# if defined(__APPLE__) && defined(__MACH__)
#  define OS_MAC 1
# elif defined(__gnu_linux__)
#  define OS_LINUX 1
# else
#  error __clang__ is defined, but one of {__APPLE__, __gnu_linux__} is not. This setup is not supported.
# endif

////////////////////////////////
//~ Context Crack: GCC Extraction

#elif defined(__GNUC__) || defined(__GNUG__)

# define COMPILER_GCC 1

# if defined(__gnu_linux__)
#  define OS_LINUX 1
# else
#  error __GNUC__ or __GNUG__ is defined, but __gnu_linux__ is not. This setup is not supported.
# endif

#else
# error Compiler is not supported. _MSC_VER, __clang__, __GNUC__, or __GNUG__ must be defined.
#endif

////////////////////////////////
//~ Context Crack: Zero

#if !defined(COMPILER_MSVC)
# define COMPILER_MSVC 0
#endif
#if !defined(COMPILER_GCC)
# define COMPILER_GCC 0
#endif
#if !defined(COMPILER_CLANG)
# define COMPILER_CLANG 0
#endif
#if !defined(OS_WINDOWS)
# define OS_WINDOWS 0
#endif
#if !defined(OS_LINUX)
# define OS_LINUX 0
#endif
#if !defined(OS_MAC)
# define OS_MAC 0
#endif

////////////////////////////////
//~ System Headers

#if OS_WINDOWS

# define WIN32_LEAN_AND_MEAN
# include <windows.h>
# include <winsock2.h>

#elif OS_LINUX

# include <termios.h>
# include <unistd.h>
# include <sys/ioctl.h>
# include <sys/mman.h>
# include <sys/utsname.h>

#else

# error Platform not supported.

#endif

////////////////////////////////
//~ Standard Headers

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#ifdef min
# undef min
#endif

#ifdef max
# undef max
#endif

/////////////////
//~ fsize

static size_t fsize(FILE *fp);

// Sets EBADF if fp is not a seekable stream
// EINVAL if fp was NULL
static size_t
fsize(FILE *fp) {
	size_t fs = 0;
	
	if (fp) {
		fseek(fp, 0L, SEEK_END);
		
		if (errno == 0) {
			fs = ftell(fp);
			
			// If fseek succeeded before, it means that fp was
			// a seekable stream, so we don't check the error again.
			
			fseek(fp, 0L, SEEK_SET);
		}
	} else {
		errno = EINVAL;
	}
	
	return fs;
}

////////////////////////////////
//~ Base

//- Keywords

#define cast(t) (t)

#if COMPILER_MSVC
# define per_thread __declspec(thread)
#elif COMPILER_CLANG
# define per_thread __thread
#elif COMPILER_GCC
# define per_thread __thread
#endif

#define alignof(t) _Alignof(t)

//- Integer/pointer/array/type manipulations

#define array_count(a) (i64)(sizeof(a)/sizeof((a)[0]))

#define bytes(n)     (   1ULL * n)
#define kilobytes(n) (1024ULL * bytes(n))
#define megabytes(n) (1024ULL * kilobytes(n))
#define gigabytes(n) (1024ULL * megabytes(n))

//- Clamps, mins, maxes

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))
#define clamp_top(a, b) min(a, b)
#define clamp_bot(a, b) max(a, b)

#define clamp(a, x, b) clamp_bot(a, clamp_top(x, b))

//- Assertions

#define allow_break() do { int __x__ = 0; (void)__x__; } while (0)
#define panic(...) assert(0)

#if OS_WINDOWS
#define force_break() __debugbreak()
#else
#define force_break() (*(volatile int *)0 = 0)
#endif

//- Linked list helpers

#define check_null(p) ((p)==0)
#define set_null(p) ((p)=0)

#define stack_push_n(f,n,next) ((n)->next=(f),(f)=(n))
#define stack_pop_nz(f,next,zchk) (zchk(f)?0:((f)=(f)->next))

#define queue_push_nz(f,l,n,next,zchk,zset) (zchk(f)?\
(((f)=(l)=(n)), zset((n)->next)):\
((l)->next=(n),(l)=(n),zset((n)->next)))
#define queue_push_front_nz(f,l,n,next,zchk,zset) (zchk(f) ? (((f) = (l) = (n)), zset((n)->next)) :\
((n)->next = (f)), ((f) = (n)))
#define queue_pop_nz(f,l,next,zset) ((f)==(l)?\
(zset(f),zset(l)):\
((f)=(f)->next))

#define dll_insert_npz(f,l,p,n,next,prev,zchk,zset) \
(zchk(f) ? (((f) = (l) = (n)), zset((n)->next), zset((n)->prev)) :\
zchk(p) ? (zset((n)->prev), (n)->next = (f), (zchk(f) ? (0) : ((f)->prev = (n))), (f) = (n)) :\
((zchk((p)->next) ? (0) : (((p)->next->prev) = (n))), (n)->next = (p)->next, (n)->prev = (p), (p)->next = (n),\
((p) == (l) ? (l) = (n) : (0))))
#define dll_push_back_npz(first,last,elem,next_ident,prev_ident,zero_check,zero_set) dll_insert_npz(first,last,last,elem,next_ident,prev_ident,zero_check,zero_set)
#define dll_remove_npz(f,l,n,next,prev,zchk,zset) (((f)==(n))?\
((f)=(f)->next, (zchk(f) ? (zset(l)) : zset((f)->prev))):\
((l)==(n))?\
((l)=(l)->prev, (zchk(l) ? (zset(f)) : zset((l)->next))):\
((zchk((n)->next) ? (0) : ((n)->next->prev=(n)->prev)),\
(zchk((n)->prev) ? (0) : ((n)->prev->next=(n)->next))))

#define stack_push(f,n)           stack_push_n(f,n,next)
#define stack_pop(f)              stack_pop_nz(f,next,check_null)

#define queue_push(f,l,n)         queue_push_nz(f,l,n,next,check_null,set_null)
#define queue_push_front(f,l,n)   queue_push_front_nz(f,l,n,next,check_null,set_null)
#define queue_pop(f,l)            queue_pop_nz(f,l,next,set_null)

#define dll_push_back(f,l,n)      dll_push_back_npz(f,l,n,next,prev,check_null,set_null)
#define dll_push_front(f,l,n)     dll_push_back_npz(l,f,n,prev,next,check_null,set_null)
#define dll_insert(f,l,p,n)       dll_insert_npz(f,l,p,n,next,prev,check_null,set_null)
#define dll_remove(f,l,n)         dll_remove_npz(f,l,n,next,prev,check_null,set_null)

//- Basic types

typedef  uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef   int8_t  i8;
typedef  int16_t i16;
typedef  int32_t i32;
typedef  int64_t i64;

typedef struct Size Size;
struct Size {
	i32 width;
	i32 height;
};

typedef struct Point Point;
struct Point {
	i32 x;
	i32 y;
};

//- Integer math

static bool
is_power_of_two(u64 i) {
	return i > 0 && (i & (i-1)) == 0;
}

static u64
align_forward(u64 ptr, u64 alignment) {
	assert(is_power_of_two(alignment));
	return (ptr + alignment-1) & ~(alignment-1);
}

static u64
round_up_to_multiple_of_u64(u64 n, u64 r) {
    u64 result;
    
    result = r - 1;
    result = n + result;
    result = result / r;
    result = result * r;
    
    return result;
}

static i64
round_up_to_multiple_of_i64(i64 n, i64 r) {
    i64 result;
    
    result = r - 1;
    result = n + result;
    result = result / r;
    result = result * r;
    
    return result;
}

////////////////////////////////
//~ Memory procedures

static void *mem_reserve(u64 size);
static void *mem_commit(void *ptr, u64 size);
static void *mem_reserve_and_commit(u64 size);
static bool  mem_decommit(void *ptr, u64 size);
static bool  mem_release(void *ptr, u64 size);

#if OS_WINDOWS

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

#elif OS_LINUX

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

#endif

////////////////////////////////
//~ Arena

//- Arena Constants

#if !defined(ARENA_COMMIT_GRANULARITY)
#define ARENA_COMMIT_GRANULARITY kilobytes(4)
#endif

#if !defined(ARENA_DECOMMIT_THRESHOLD)
#define ARENA_DECOMMIT_THRESHOLD megabytes(64)
#endif

#if !defined(DEFAULT_ARENA_RESERVE_SIZE)
#define DEFAULT_ARENA_RESERVE_SIZE gigabytes(1)
#endif

//- Arena Types

typedef struct Arena Arena;
struct Arena {
	u8  *ptr;
	u64  pos;
	u64  cap;
	u64  peak;
	u64  commit_pos;
};

typedef struct Arena_Restore_Point Arena_Restore_Point;
struct Arena_Restore_Point {
	Arena *arena;
	u64    pos;
};

//- Arena procedures

static void arena_init(Arena *arena);
static void arena_init_size(Arena *arena, u64 reserve_size);
static bool arena_fini(Arena *arena);
static void arena_reset(Arena *arena);

static void *push_nozero(Arena *arena, u64 size, u64 alignment);
static void *push_zero(Arena *arena, u64 size, u64 alignment);

static void pop_to(Arena *arena, u64 pos);
static void pop_amount(Arena *arena, u64 amount);

static Arena_Restore_Point arena_begin_temp_region(Arena *arena);
static void arena_end_temp_region(Arena_Restore_Point point);

//- Arena operations: constructors/destructors

static void
arena_init(Arena *arena) {
	arena_init_size(arena, DEFAULT_ARENA_RESERVE_SIZE);
}

static void
arena_init_size(Arena *arena, u64 reserve_size) {
	u8 *base = mem_reserve(reserve_size);
	assert(base != NULL);
	
	arena->ptr  = base;
	arena->cap  = reserve_size;
	arena->pos  = 0;
	arena->peak = 0;
	arena->commit_pos = 0;
}

static bool
arena_fini(Arena *arena) {
	bool result = mem_release(arena->ptr, arena->cap);
	memset(arena, 0, sizeof(Arena));
	return result;
}

static void
arena_reset(Arena *arena) {
	pop_to(arena, 0);
}

//- Arena operations: push

static void *
push_nozero_aligned(Arena *arena, u64 size, u64 alignment) {
	void *result = NULL;
	
	if (size > 0) {
		u64 align_pos = align_forward(arena->pos, alignment);
		if (align_pos + size <= arena->cap) {
			arena->pos = align_pos;
			
			result = arena->ptr + arena->pos;
			arena->pos += size;
			
			if (arena->pos > arena->commit_pos) {
				u64 new_commit_pos = clamp_top(align_forward(arena->pos, ARENA_COMMIT_GRANULARITY), arena->cap);
				
				void *commit_base = arena->ptr + arena->commit_pos;
				u64   commit_size = new_commit_pos - arena->commit_pos;
				
				(void)mem_commit(commit_base, commit_size);
				arena->commit_pos = new_commit_pos;
			}
			
			arena->peak = max(arena->pos, arena->peak);
		} else {
			panic("Arena is out of memory.");
		}
	}
	
	return result;
}

static void *
push_zero_aligned(Arena *arena, u64 size, u64 alignment) {
	void *result = push_nozero_aligned(arena, size, alignment);
	memset(result, 0, size);
	return result;
}

#define push_nozero(arena, size) push_nozero_aligned(arena, size, sizeof(u8))
#define push_zero(arena, size)   push_zero_aligned(arena, size, sizeof(u8))

#define push_aligned(arena, size, alignment) push_zero_aligned(arena, size, alignment)
#define push(arena, size)                    push_zero(arena, size)

#define push_type(arena, type)       cast(type *) push_aligned(arena, sizeof(type), alignof(type))
#define push_array(arena, type, len) cast(type *) push_aligned(arena, (len)*sizeof(type), alignof(type))

//- Arena operations: pop

static void
pop_to(Arena *arena, u64 pos) {
	arena->pos = clamp_top(pos, arena->pos); // Prevent user from going forward, only go backward.
	
	u64 pos_aligned_to_commit_chunks = clamp_top(align_forward(arena->pos, ARENA_COMMIT_GRANULARITY), arena->cap);
	
	if (pos_aligned_to_commit_chunks + ARENA_DECOMMIT_THRESHOLD <= arena->commit_pos) {
		u64   decommit_size = arena->commit_pos - pos_aligned_to_commit_chunks;
		void *decommit_base = arena->ptr + pos_aligned_to_commit_chunks;
		
		mem_decommit(decommit_base, decommit_size);
		arena->commit_pos = pos_aligned_to_commit_chunks;
	}
}

static void
pop_amount(Arena *arena, u64 amount) {
	u64 amount_clamped = clamp_top(amount, arena->pos); // Prevent user from going to negative positions
	pop_to(arena, arena->pos - amount_clamped);
}

#define pop(arena, amount) pop_amount(arena, amount)

//- Arena operations: Temp

static Arena_Restore_Point
arena_begin_temp_region(Arena *arena) {
	Arena_Restore_Point point = {arena, arena->pos};
	return point;
}

static void
arena_end_temp_region(Arena_Restore_Point point) {
	pop_to(point.arena, point.pos);
}

////////////////////////////////
//~ Scratch Memory

//- Scratch Memory Constants

#if !defined(SCRATCH_ARENA_COUNT)
#define SCRATCH_ARENA_COUNT 2
#endif

#if !defined(SCRATCH_ARENA_RESERVE_SIZE)
#define SCRATCH_ARENA_RESERVE_SIZE gigabytes(8)
#endif

//- Scratch Memory Types

typedef Arena_Restore_Point Scratch;

//- Scratch Memory Variables

#if SCRATCH_ARENA_COUNT > 0
per_thread Arena scratch_arenas[SCRATCH_ARENA_COUNT];
#endif

//- Scratch Memory Functions

static Scratch scratch_begin(Arena **conflicts, i64 conflict_count);
static void    scratch_end(Scratch scratch);

static Scratch
scratch_begin(Arena **conflicts, i64 conflict_count) {
	Scratch scratch = {0};
	
#if SCRATCH_ARENA_COUNT > 0
	if (scratch_arenas[0].ptr == NULL) { // unlikely()
		for (int i = 0; i < array_count(scratch_arenas); i += 1) {
			arena_init_size(&scratch_arenas[i], SCRATCH_ARENA_RESERVE_SIZE);
		}
	}
#endif
	
	for (i64 scratch_arena_index = 0; scratch_arena_index < array_count(scratch_arenas); scratch_arena_index += 1) {
		bool is_conflicting = false;
		for (i64 conflict_index = 0; conflict_index < conflict_count; conflict_index += 1) {
			if (conflicts[conflict_index] == &scratch_arenas[scratch_arena_index]) {
				is_conflicting = true;
				break;
			}
		}
		
		if (!is_conflicting) {
			scratch = arena_begin_temp_region(&scratch_arenas[scratch_arena_index]);
			break;
		}
	}
	
	return scratch;
}

static void
scratch_end(Scratch scratch) {
	arena_end_temp_region(scratch);
}

////////////////////////////////
//~ Strings and slices

//- String and slice types

typedef struct SliceU8 SliceU8;
struct SliceU8 {
	i64  len;
	u8  *data;
};

typedef struct String String;
struct String {
	i64  len;
	u8  *data;
};

//- Slice functions

static SliceU8
make_sliceu8(u8 *data, i64 len) {
	SliceU8 result = {
		.data = data,
		.len  = len,
	};
	return result;
}

static SliceU8
push_sliceu8(Arena *arena, i64 len) {
	SliceU8 result = {
		.data = push(arena, cast(u64) len),
		.len  = len,
	};
	return result;
}

static SliceU8
sliceu8_from_string(String s) {
	return make_sliceu8(s.data, s.len);
}

static SliceU8
sliceu8_clone(Arena *arena, SliceU8 s) {
	// We don't call push_sliceu8() because that clears memory to 0 and we don't need that here.
	SliceU8 result = {
		.data = push_nozero(arena, s.len),
		.len  = s.len,
	};
	memcpy(result.data, s.data, s.len);
	return result;
}

//- String functions

#define string_lit_expand(s)   s, (sizeof(s)-1)
#define string_expand(s)       cast(int)(s).len, (s).data

#define string_from_lit(s)     string(cast(u8 *)s, sizeof(s)-1)
#define string_from_cstring(s) string(cast(u8 *)s, strlen(s))

static String
string(u8 *data, i64 len) {
	String result = {
		.data = data,
		.len  = len,
	};
	return result;
}

static String
push_string(Arena *arena, i64 len) {
	String result = {
		.data = push(arena, cast(u64) len),
		.len  = len,
	};
	return result;
}

static String
string_from_sliceu8(SliceU8 s) {
	return string(s.data, s.len);
}

static String
string_clone(Arena *arena, String s) {
	// We don't call push_string() because that clears memory to 0 and we don't need that here.
	String result = {
		.data = push_nozero(arena, s.len),
		.len  = s.len,
	};
	memcpy(result.data, s.data, s.len);
	return result;
}

static String
string_clone_buffer(u8 *buffer, i64 buffer_len, String s) {
	memcpy(buffer, s.data, min(buffer_len, s.len));
	return string(buffer, min(buffer_len, s.len));
}

static char *
cstring_from_string(Arena *arena, String s) {
	char *result = push_nozero(arena, (s.len + 1) * sizeof(char));
	memcpy(result, s.data, s.len);
	result[s.len] = 0;
	return result;
}

////////////////////////////////
//~ String Builder

//- String builder types

typedef struct String_Builder String_Builder;
struct String_Builder {
	u8  *data;
	i64  len;
	i64  cap;
};

//- String builder functions

static void string_builder_init(String_Builder *builder, SliceU8 backing);
static i64  string_builder_append(String_Builder *builder, String s);

static String string_from_builder(String_Builder builder);

static void
string_builder_init(String_Builder *builder, SliceU8 backing) {
	builder->data = backing.data;
	builder->cap  = backing.len;
	builder->len  = 0;
}

static i64
string_builder_append(String_Builder *builder, String s) {
	assert(builder->data); // Not initialized
	
	i64 space = builder->cap - builder->len;
	i64 to_copy = min(space, s.len);
	memcpy(builder->data + builder->len, s.data, to_copy);
	builder->len += to_copy;
	
	return to_copy;
}

static String
string_from_builder(String_Builder builder) {
	return string(builder.data, builder.len);
}

////////////////////////////////
//~ File IO

//- File IO types

typedef struct Read_File_Result Read_File_Result;
struct Read_File_Result {
	SliceU8 contents;
	bool    ok;
};

//- File IO functions

static Read_File_Result read_file(Arena *arena, String file_name);

static Read_File_Result
read_file(Arena *arena, String file_name) {
	Read_File_Result result = {0};
	
	Scratch scratch = scratch_begin(&arena, 1);
	char *file_name_null_terminated = cstring_from_string(scratch.arena, file_name);
	
	FILE *handle = fopen(file_name_null_terminated, "rb");
	if (handle) {
		errno = 0;
		size_t size = fsize(handle);
		if (errno == 0) {
			result.contents.len  = size;
			result.contents.data = push_nozero(arena, size * sizeof(u8));
			assert(result.contents.data); // TODO: Switch to an if-else?
			
			i64 read_amount  = fread(result.contents.data,
									 sizeof(u8),
									 result.contents.len,
									 handle);
			if (read_amount == result.contents.len || feof(handle)) {
				result.ok = true;
			} else {
				// TODO: something. Write to a log queue? Then the caller can read it if necessary
			}
		} else {
			// TODO: something. Write to a log queue? Then the caller can read it if necessary
		}
		fclose(handle);
	} else {
		// TODO: something. Write to a log queue? Then the caller can read it if necessary
	}
	
	scratch_end(scratch);
	
	return result;
}

////////////////////////////////
//~ Console output helpers

//- Console platform-specific functions

static void write_console_unbuffered(String s);

#if OS_WINDOWS

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

#elif OS_LINUX

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

////////////////////////////////
//~ Editor

// TODO: Review all casts

#define ESCAPE_PREFIX "\x1b["

#define CTRL_KEY(k) ((k) & 0x1f)

#define esc(code) string_from_lit(ESCAPE_PREFIX code)

#define EDITOR_SPAN_SIZE 64
#define EDITOR_PAGE_SIZE 128

#define EDITOR_TAB_WIDTH 4

//- Editor types

enum Editor_Key {
	Editor_Key_ARROW_UP = 256 + 1,
	Editor_Key_ARROW_LEFT,
	Editor_Key_ARROW_DOWN,
	Editor_Key_ARROW_RIGHT,
	Editor_Key_PAGE_UP,
	Editor_Key_PAGE_DOWN,
	Editor_Key_HOME,
	Editor_Key_END,
	Editor_Key_DELETE,
};
typedef enum Editor_Key Editor_Key;

typedef struct Editor_Span Editor_Span;
struct Editor_Span {
	Editor_Span *next;
	
	u8  *data;
	i64  len;
};

typedef struct Editor_Line Editor_Line;
struct Editor_Line {
	Editor_Span *first_span;
	Editor_Span *last_span;
};

typedef struct Editor_Page Editor_Page;
struct Editor_Page {
	Editor_Page *next;
	Editor_Page *prev;
	
	Editor_Line *lines;
	i64 line_count;
};

typedef struct Editor_Buffer Editor_Buffer;
struct Editor_Buffer {
	bool is_read_only;
	
	Arena arena;
	String name;
	String file_name;
	Point cursor_position;
	
	Editor_Page *first_page;
	Editor_Page *last_page;
	i64 page_count;
	i64 line_count;
	
	i64 vscroll;
	i64 hscroll;
	
	Editor_Page *first_free_page;
	Editor_Span *first_free_span;
};

typedef struct Editor_State Editor_State;
struct Editor_State {
	Arena arena;
	
	Size window_size;
	
	String status_message;
	u8 status_message_buffer[64];
	time_t status_message_timestamp;
	
	Editor_Buffer *current_buffer;
	Editor_Buffer *single_buffer;
	Editor_Buffer *null_buffer;
};

typedef struct Editor_Relative_Line Editor_Relative_Line;
struct Editor_Relative_Line {
	Editor_Page *page;
	i64 line;
};

//- Editor global variables

static int exit_code = 0;
static Editor_State state;

static FILE *logfile;

//- Editor generic functions (declarations)

static void editor_init_buffer_contents(Editor_Buffer *buffer, SliceU8 contents);
static void editor_load_file(String file_name);
static bool editor_buffer_is_in_use(Editor_Buffer *buffer);

// static String string_from_editor_line(Editor_Line *line);
static String string_from_editor_line(Arena *arena, Editor_Line *line);
static Editor_Relative_Line editor_relative_from_absolute_line(i64 absolute_line);
static Editor_Line *editor_line_from_line_number(i64 line_number);
static Editor_Line *editor_get_current_line();

static void editor_move_cursor(Editor_Key key);
static void editor_update_scroll(void);
static void editor_set_status_message(String message);

static void editor_render_buffer(Editor_Buffer *buffer);
static String editor_render_string_from_stored_string(Arena *arena, String stored_string);
static i64 editor_render_x_from_stored_x(String stored_string, i64 stored_x);

static void editor_validate_buffer(Editor_Buffer *buffer);

//- Editor platform-specific functions (declarations)

static void clear(void);
static String get_clear_string(void);

static bool query_window_size(Size *size);
static bool query_cursor_position(Point *position);

static Editor_Key wait_for_key(void);

//- Editor helper functions

#if 0
static String
string_from_editor_line(Editor_Line *line) {
	return string(line->data, line->len);
}
#else
static i64
editor_line_len(Editor_Line *line) {
	i64 len = 0;
	Editor_Span *span = line->first_span;
	while (span) {
		len += span->len;
		span = span->next;
	}
	return len;
}

static String
string_from_editor_line(Arena *arena, Editor_Line *line) {
	i64 len = editor_line_len(line);
	String result = push_string(arena, len);
	i64 at = 0;
	Editor_Span *span = line->first_span;
	while (span) {
		memcpy(result.data + at, span->data, span->len);
		at += span->len;
		
		span = span->next;
	}
	return result;
}

typedef struct Editor_Relative_Span Editor_Relative_Span;
struct Editor_Relative_Span {
	Editor_Span *span;
	i64 at;
};

static Editor_Relative_Span
editor_relative_span_from_line_and_pos(Editor_Line *line, i64 pos) {
	Editor_Relative_Span result = {0};
	
	Editor_Span *span = line->first_span;
	while (span) {
		if (pos < span->len + 1) {
			result.span = span;
			result.at = pos;
			break;
		}
		
		pos -= span->len;
		span = span->next;
	}
	
	assert(result.span && result.span->data);
	
	return result;
}

#endif



static Editor_Relative_Line
editor_relative_from_absolute_line(i64 absolute_line) {
	Editor_Relative_Line result = {0};
	
	i64 line = absolute_line;
	Editor_Page *page = state.current_buffer->first_page;
	while (page) {
		if (line < page->line_count) {
			result.page = page;
			result.line = line;
			break;
		}
		
		line -= page->line_count;
		page  = page->next;
	}
	
	assert(result.page && result.page->lines);
	
	return result;
}

static Editor_Line *
editor_line_from_line_number(i64 line_number) {
	Editor_Relative_Line rel = editor_relative_from_absolute_line(line_number);
	Editor_Line *result = &rel.page->lines[rel.line];
	return result;
}

static Editor_Line *
editor_get_current_line() {
	return editor_line_from_line_number(state.current_buffer->cursor_position.y);
}

//- Editor debug functions

static void
editor_validate_buffer(Editor_Buffer *buffer) {
	i64 line_count = 0;
	
	Editor_Page *page = buffer->first_page;
	while (page) {
		line_count += page->line_count;
		page = page->next;
	}
	
	assert(line_count == buffer->line_count);
	assert(line_count > 0);
	
	allow_break();
}

//- Editor line rendering functions

// Expands tab characters to spaces
static String
editor_render_string_from_stored_string(Arena *arena, String stored_string) {
	int tab_count = 0;
	for (i64 i = 0; i < stored_string.len; i += 1) {
		if (stored_string.data[i] == '\t') {
			tab_count += 1;
		}
	}
	
	String result;
	result.len  = stored_string.len + tab_count*(EDITOR_TAB_WIDTH - 1);
	result.data = push_nozero(arena, result.len * sizeof(u8));
	
	i64 write_index = 0;
	for (i64 read_index = 0; read_index < stored_string.len; read_index += 1) {
		if (stored_string.data[read_index] == '\t') {
			for (i64 i = 0; i < EDITOR_TAB_WIDTH; i += 1) {
				result.data[write_index + i] = ' ';
			}
			write_index += EDITOR_TAB_WIDTH;
		} else {
			result.data[write_index] = stored_string.data[read_index];
			write_index += 1;
		}
	}
	
	return result;
}

static i64
editor_render_x_from_stored_x(String stored_string, i64 stored_x) {
	i64 tab_count = 0;
	for (i64 i = 0; i < stored_x; i += 1) {
		if (stored_string.data[i] == '\t') {
			tab_count += 1;
		}
	}
	
	i64 result = tab_count*EDITOR_TAB_WIDTH + (stored_x - tab_count);
	return result;
}

//- Editor main render loop

static void
editor_render_buffer(Editor_Buffer *buffer) {
	Scratch scratch = scratch_begin(0, 0);
	
	// TODO: Undo this pull-out of the strings and simply put in a big safety padding,
	// this is stupid
	
	String esc_hide_cursor = esc("?25l");
	String esc_clear_screen = get_clear_string();
	String esc_invert_colors = esc("7m");
	String esc_reset_colors = esc("m");
	String esc_reset_cursor = esc("H");
	String esc_show_cursor = esc("?25h");
	
	i64 builder_cap = (state.window_size.width * state.window_size.height +
					   2 * state.window_size.height +
					   esc_hide_cursor.len +
					   esc_clear_screen.len +
					   esc_invert_colors.len +
					   esc_reset_colors.len +
					   esc_reset_cursor.len +
					   esc_show_cursor.len +
					   1024);
	
	String_Builder builder;
	string_builder_init(&builder, push_sliceu8(scratch.arena, builder_cap));
	
	string_builder_append(&builder, esc_hide_cursor);
	
	string_builder_append(&builder, esc_clear_screen);
	
	{
		i64 line_number = buffer->vscroll; // Absolute line number from the start of the buffer, the first that is visible
		
		Editor_Relative_Line rel_line = editor_relative_from_absolute_line(line_number);
		Editor_Page *page = rel_line.page;
		i64 line_relative_to_start_of_page = rel_line.line;
		
		int num_rows_to_draw = state.window_size.height - 2; // Subtract the status bar and the status message
		
		for (int y = 0; y < num_rows_to_draw; y += 1) {
			if (page && line_relative_to_start_of_page < page->line_count) {
				{
					// Print line
					Editor_Line *line = &page->lines[line_relative_to_start_of_page];
					String render_line = editor_render_string_from_stored_string(scratch.arena, string_from_editor_line(scratch.arena, line));
					
					i64 to_write = clamp(0, render_line.len - buffer->hscroll, state.window_size.width);
					string_builder_append(&builder, string(render_line.data + buffer->hscroll, to_write));
				}
				
				// check for end of page
				line_relative_to_start_of_page += 1;
				if (line_relative_to_start_of_page == page->line_count) {
					page = page->next;
					line_relative_to_start_of_page = 0;
				}
			} else {
				if (buffer == state.null_buffer && y == state.window_size.height / 3) {
					
#define FEDIT_VERSION "1"
					char welcome[80];
					int  welcomelen = snprintf(welcome, sizeof(welcome), "Fedit -- version %s", FEDIT_VERSION);
					int to_write = min(welcomelen, state.window_size.width);
					int padding = (state.window_size.width - to_write) / 2;
					if (padding != 0) {
						string_builder_append(&builder, string_from_lit("~"));
						padding -= 1;
					}
					while (padding != 0) {
						string_builder_append(&builder, string_from_lit(" "));
						padding -= 1;
					}
					string_builder_append(&builder, string(cast(u8 *) welcome, welcomelen));
				} else {
					string_builder_append(&builder, string_from_lit("~"));
				}
			}
			
			// This doesn't work in Windows terminals, we have to clear the whole screen
			// using the special code because Windows is a special kid.
			// string_builder_append(&builder, string_from_lit("\x1b[K")); // Clear row
			string_builder_append(&builder, string_from_lit("\r\n"));
		}
		
		{
			// Draw status bar:
			
			string_builder_append(&builder, esc_invert_colors);
			
			int len = 0;
			char status[80];
			if (buffer != state.null_buffer) {
				String buffer_name = buffer->name;
				len = snprintf(status, sizeof(status), "%.*s - %d lines",
							   string_expand(buffer_name), cast(i32) buffer->line_count);
				len = min(len, state.window_size.width);
				string_builder_append(&builder, string(cast(u8 *) status, len));
			} else {
				len = snprintf(status, sizeof(status), "No buffer selected");
				string_builder_append(&builder, string(cast(u8 *) status, len));
			}
			
			for (int x = len; x < state.window_size.width; x += 1) {
				string_builder_append(&builder, string_from_lit(" "));
			}
			
			string_builder_append(&builder, esc_reset_colors);
		}
		
		{
			// Draw status message:
			
			string_builder_append(&builder, string_from_lit("\r\n"));
			
			i64 to_write = min(state.status_message.len, state.window_size.width);
			if (time(NULL) - state.status_message_timestamp < 5) {
				string_builder_append(&builder, string(state.status_message.data, to_write));
			}
		}
	}
	
	string_builder_append(&builder, esc_reset_cursor);
	
	// Move cursor
	char buf[32] = {0};
	
	Editor_Line *current_line = editor_line_from_line_number(buffer->cursor_position.y);
	i64 cursor_render_x = editor_render_x_from_stored_x(string_from_editor_line(scratch.arena, current_line), buffer->cursor_position.x);
	
	i32 cursor_y_on_screen = buffer->cursor_position.y - cast(i32) buffer->vscroll; // TODO: Review this cast
	i32 cursor_x_on_screen = cast(i32) cursor_render_x - cast(i32) buffer->hscroll; // TODO: Review this cast
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cursor_y_on_screen + 1, cursor_x_on_screen + 1);
	string_builder_append(&builder, string_from_cstring(buf));
	
	string_builder_append(&builder, esc_show_cursor);
	
	// assert(builder.len == builder.cap);
	write_console_unbuffered(string_from_builder(builder));
	
	scratch_end(scratch);
}

//- Editor buffer functions

static bool
editor_buffer_is_in_use(Editor_Buffer *buffer) {
	return buffer->name.len > 0;
}

static void
editor_init_buffer_contents(Editor_Buffer *buffer, SliceU8 contents) {
	// Assumes that the raw contents encode line breaks as LF.
	
	assert(buffer->arena.ptr);
	
	buffer->cursor_position.x = 0;
	buffer->cursor_position.y = 0;
	buffer->vscroll = 0;
	buffer->hscroll = 0;
	buffer->page_count = 0;
	buffer->line_count = 0;
	buffer->first_page = NULL;
	buffer->last_page  = NULL;
	buffer->first_free_page = NULL;
	buffer->first_free_span = NULL;
	
	Editor_Page *page;
	{
		page = push_type(&buffer->arena, Editor_Page);
		page->line_count = 0;
		page->lines = push_array(&buffer->arena, Editor_Line, EDITOR_PAGE_SIZE);
		page->next  = NULL;
		page->prev  = NULL;
		dll_push_back(buffer->first_page, buffer->last_page, page);
		buffer->page_count += 1;
	}
	
	i64 line_start = 0;
	i64 line_end = 0;
	for (i64 byte_index = 0; byte_index <= contents.len; byte_index += 1) {
		if (byte_index == contents.len || contents.data[byte_index] == '\n') {
			line_end = byte_index;
			
			// Get line
			Editor_Line *line = NULL;
			{
				if (page->line_count >= EDITOR_PAGE_SIZE) {
					page = push_type(&buffer->arena, Editor_Page);
					page->line_count = 0;
					page->lines = push_array(&buffer->arena, Editor_Line, EDITOR_PAGE_SIZE);
					page->next  = NULL;
					page->prev  = NULL;
					dll_push_back(buffer->first_page, buffer->last_page, page);
					buffer->page_count += 1;
				}
				
				line = &page->lines[page->line_count];
				page->line_count += 1;
				buffer->line_count += 1;
			}
			
			// Fill line
			line->first_span = NULL;
			line->last_span = NULL;
			
			i64 line_len = line_end - line_start;
			i64 copied = 0;
			
#if 0
			{
				Editor_Span *span = line->last_span;
				if (span) {
					assert(span->data);
					
					i64 to_copy = line_len - copied;
					i64 to_copy_now = min(to_copy, EDITOR_SPAN_SIZE - span->len);
					memcpy(span->data, contents.data + line_start, to_copy_now);
					
					copied += to_copy_now;
				}
			}
#endif
			
			if (line_len > 0) {
				while (copied < line_len) {
					Editor_Span *span = push_type(&buffer->arena, Editor_Span);
					queue_push(line->first_span, line->last_span, span);
					
					span->data = push_array(&buffer->arena, u8, EDITOR_SPAN_SIZE);
					
					i64 to_copy = line_len - copied;
					i64 to_copy_now = min(to_copy, EDITOR_SPAN_SIZE);
					memcpy(span->data, contents.data + line_start + copied, to_copy_now);
					span->len = to_copy_now;
					
					copied += to_copy_now;
				}
			} else {
				Editor_Span *span = push_type(&buffer->arena, Editor_Span);
				queue_push(line->first_span, line->last_span, span);
				
				span->data = push_array(&buffer->arena, u8, EDITOR_SPAN_SIZE);
			}
			
			// Prepare for next iteration
			line_start = line_end + 1;
		}
	}
	
	return;
}

static void
editor_load_file(String file_name) {
	// Overwrite previously loaded file.
	
	if (!state.single_buffer) {
		state.single_buffer = push_type(&state.arena, Editor_Buffer);
	}
	
	if (!state.single_buffer->arena.ptr) {
		arena_init(&state.single_buffer->arena);
	} else {
		arena_reset(&state.single_buffer->arena);
	}
	
	Scratch scratch = scratch_begin(0, 0);
	
	Read_File_Result read_file_result = read_file(scratch.arena, file_name);
	if (read_file_result.ok) {
		// TODO: For now let's pretend that every file is LF
		editor_init_buffer_contents(state.single_buffer, read_file_result.contents);
		
		state.single_buffer->file_name = string_clone(&state.single_buffer->arena, file_name);
		state.single_buffer->name      = state.single_buffer->file_name;
	} else {
		// TODO: Something
		
		// If the file doesn't exist, simply leave the editor open with no loaded files
		// Display a log message at the bottom or something
		// For now just panic
		panic("Could not read file");
	}
	
	scratch_end(scratch);
}

//- Editor cursor/view functions

static void
editor_update_scroll(void) {
	
	Editor_Buffer *curr_buffer = state.current_buffer;
	
#if 0
	// Scroll by 2
	
	// Vertical scroll
	if (state.cursor_position.y < state.vscroll) {
		state.vscroll = state.cursor_position.y - 1; // Scroll up by 2 lines
		state.vscroll = max(state.vscroll, 0);
	}
	if (state.cursor_position.y >= state.vscroll + state.window_size.height) {
		state.vscroll = state.cursor_position.y - state.window_size.height + 2;
		state.vscroll = min(state.vscroll, state.line_count);
	}
	
	// Horizontal scroll
	if (state.cursor_position.x < state.hscroll) {
		state.hscroll = state.cursor_position.x - 1;
		state.hscroll = max(state.hscroll, 0);
	}
	if (state.cursor_position.x >= state.hscroll + state.window_size.width) {
		state.hscroll = state.cursor_position.x - state.window_size.width + 2;
		state.hscroll = min(state.hscroll, state.window_size.width);
	}
#else
	// Simplified, scroll by 1
	
	// Vertical scroll
	if (curr_buffer->cursor_position.y < curr_buffer->vscroll) {
		curr_buffer->vscroll = curr_buffer->cursor_position.y;
	}
	if (curr_buffer->cursor_position.y >= curr_buffer->vscroll + (state.window_size.height - 2)) {
		curr_buffer->vscroll = curr_buffer->cursor_position.y - (state.window_size.height - 2) + 1;
	}
	
	Scratch scratch = scratch_begin(0, 0);
	
	Editor_Line *current_line = editor_line_from_line_number(curr_buffer->cursor_position.y);
	i64 cursor_render_x = editor_render_x_from_stored_x(string_from_editor_line(scratch.arena, current_line), curr_buffer->cursor_position.x);
	
	// Horizontal scroll
	if (cursor_render_x < curr_buffer->hscroll) {
		curr_buffer->hscroll = cursor_render_x;
	}
	if (cursor_render_x >= curr_buffer->hscroll + state.window_size.width) {
		curr_buffer->hscroll = cursor_render_x - state.window_size.width + 1;
	}
	
	scratch_end(scratch);
#endif
	
}

static void
editor_move_cursor(Editor_Key key) {
	
	Editor_Buffer *curr_buffer = state.current_buffer;
	
	switch (key) {
		case Editor_Key_ARROW_LEFT: {
			if (curr_buffer->cursor_position.x > 0) {
				curr_buffer->cursor_position.x -= 1;
			} else {
				// Move to end of previous line
				if (curr_buffer->cursor_position.y > 0) {
					curr_buffer->cursor_position.y -= 1;
					Editor_Line *current_line = editor_line_from_line_number(curr_buffer->cursor_position.y);
					curr_buffer->cursor_position.x = cast(i32) editor_line_len(current_line);
				}
			}
		} break;
		case Editor_Key_ARROW_RIGHT: {
			Editor_Line *current_line = editor_line_from_line_number(curr_buffer->cursor_position.y);
			if (curr_buffer->cursor_position.x < editor_line_len(current_line)) {
				curr_buffer->cursor_position.x += 1;
			} else {
				if (curr_buffer->cursor_position.y < curr_buffer->line_count - 1) {
					curr_buffer->cursor_position.y += 1;
					curr_buffer->cursor_position.x = 0;
				}
			}
		} break;
		case Editor_Key_ARROW_UP: {
			if (curr_buffer->cursor_position.y > 0) {
				curr_buffer->cursor_position.y -= 1;
			}
		} break;
		case Editor_Key_ARROW_DOWN: {
			if (curr_buffer->cursor_position.y < curr_buffer->line_count - 1) {
				curr_buffer->cursor_position.y += 1;
			}
		} break;
		default: {
			panic("Invalid switch case");
		}
	}
	
	Editor_Line *current_line = editor_line_from_line_number(curr_buffer->cursor_position.y);
	if (curr_buffer->cursor_position.x > editor_line_len(current_line)) {
		curr_buffer->cursor_position.x = cast(i32) editor_line_len(current_line); // @Speed
	}
}

//- Editor misc functions

static void
editor_set_status_message(String message) {
	state.status_message = string_clone_buffer(state.status_message_buffer, sizeof(state.status_message_buffer), message);
	state.status_message_timestamp = time(NULL);
}

#if OS_WINDOWS

//- Windows-specific global variables

static DWORD original_stdin_mode;
static HANDLE stdin_handle;

static DWORD original_stdout_mode;
static HANDLE stdout_handle;

//- Windows-specific initialization/finalization

static void
before_main(void) {
	stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
	assert(stdin_handle && stdin_handle != INVALID_HANDLE_VALUE);
	
	stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
	assert(stdout_handle && stdout_handle != INVALID_HANDLE_VALUE);
}

static void
enable_raw_mode(void) {
	if (GetConsoleMode(stdin_handle, &original_stdin_mode)) {
		DWORD mode = original_stdin_mode;
		mode &= ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
		
		// TODO: Find a way to disable mouse scrolling
		
		if (!SetConsoleMode(stdin_handle, mode)) {
			int n = GetLastError();
			(void)n;
			// TODO: Set errno + call perror()
			exit_code = 1;
			
			panic(); // Temporary
		}
	} else {
		int n = GetLastError();
		(void)n;
		// TODO: Set errno + call perror()
		exit_code = 1;
		
		panic(); // Temporary
	}
	
	if (GetConsoleMode(stdout_handle, &original_stdout_mode)) {
		DWORD mode = original_stdout_mode;
		mode |= (ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
		
		if (!SetConsoleMode(stdout_handle, mode)) {
			panic(); // Temporary
		}
	} else {
		panic(); // Temporary
	}
}

static void
disable_raw_mode(void) {
	(void)SetConsoleMode(stdin_handle, original_stdin_mode);
}

//- Windows-specific console functions

static bool
query_cursor_position(Point *position) {
	bool ok = false;
	
	// When we set the cursor position with the escape codes, the position is 1-based
	// When we query it with this API, it is 0-based.
	
#if 0
	
	CONSOLE_SCREEN_BUFFER_INFO info = {0};
	if (GetConsoleScreenBufferInfo(stdout_handle, &info)) {
		position->x = info.dwCursorPosition.X;
		position->y = info.dwCursorPosition.Y;
		ok = true;
	} else {
		// As a fallback method, we could do what Linux does: send a 'n' command
		// and parse the response. I don't know if it makes sense though, for Windows
		
		panic(); // Temporary
	}
	
#else
	
	// @Copypaste from the Linux version: the only differences are the system API calls
	
	DWORD ww = 0;
	BOOL wok = WriteConsole(stdout_handle, "\x1b[6n", 4, &ww, NULL); // n command = Device Status Report; arg 6 = cursor position
	if (wok && ww == 4) {
		char buf[32];
		unsigned int i = 0;
		
		while (i < sizeof(buf) - 1) {
			DWORD rr = 0;
			if (!ReadConsole(stdin_handle, &buf[i], 1, &rr, NULL)) break;
			if (buf[i] == 'R') break;
			i += 1;
		}
		
		buf[i] = '\0';
		
		if (buf[0] == '\x1b' || buf[1] == '[') { // Make sure the response is an escape sequence
			int x = 0, y = 0;
			int sscanf_result = sscanf(&buf[2], "%d;%d", &y, &x); // Parse the report; the first number is the line (so the y) and the second is the column (so the x)
			// The position is 1-based
			
			if (sscanf_result == 2) {
				position->x = x;
				position->y = y;
				ok = true;
			}
		}
	}
	
#endif
	
	return ok;
}

static bool
query_window_size(Size *size) {
	bool ok = false;
	
	CONSOLE_SCREEN_BUFFER_INFO info = {0};
	if (GetConsoleScreenBufferInfo(stdout_handle, &info)) {
		// The reason we don't use the dwSize member of the struct is because that
		// contains the size of the *buffer* (so even stuff we scrolled past).
		size->width  = info.srWindow.Right - info.srWindow.Left + 1;
		size->height = info.srWindow.Bottom - info.srWindow.Top + 1;
		ok = true;
	} else {
		panic(); // Temporary
	}
	
	return ok;
}

// NOTE: Clearing the screen in Windows is weird, because the normal escape code doesn't work,
// so you either have to use the alternative one:
//   https://stackoverflow.com/questions/48612106/windows-console-esc2j-doesnt-really-clear-the-screen
// or use the deprecated console API:
//   https://stackoverflow.com/questions/6486289/how-to-clear-the-console-in-c
//   https://stackoverflow.com/questions/5866529/how-do-we-clear-the-console-in-assembly/5866648#5866648

static String
get_clear_string(void) {
	return string_from_lit("\033[H\033[J");
}

#define WIN32_CLEAR_USING_ESCAPE_CODES 1

static void
clear(void) {
	
#if WIN32_CLEAR_USING_ESCAPE_CODES
	
	write_console_unbuffered(get_clear_string());
	
#else
	
    CONSOLE_SCREEN_BUFFER_INFO info;
    GetConsoleScreenBufferInfo(stdout_handle, &info);
	
    DWORD written = 0;
	COORD top_left = {0, 0};
    FillConsoleOutputCharacterA(stdout_handle, ' ', info.dwSize.X * info.dwSize.Y, top_left, &written);
	
    FillConsoleOutputAttribute(stdout_handle, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE,
							   info.dwSize.X * info.dwSize.Y, top_left, &written);
    SetConsoleCursorPosition(stdout_handle, top_left);
	
#endif
	
}

static Editor_Key
wait_for_key(void) {
	
	// With the help of:
	//   https://stackoverflow.com/a/22310673
	
	INPUT_RECORD record = {0};
	
	bool ok = false;
	while (true) {
		int n = 0;
		while (true) {
			DWORD wres = WSAWaitForMultipleEvents(1, &stdin_handle, FALSE, 100, TRUE);
			if (wres == WSA_WAIT_TIMEOUT) {
				ok = true;
				n = 0;
				break;
			}
			
			if (wres == WSA_WAIT_EVENT_0) {
				
				DWORD num_read = 0;
				
				if (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &record, 1, &num_read)) {
					assert(num_read > 0); // Because we're in the WSA_WAIT_EVENT_0 case.
					
					if (record.EventType == KEY_EVENT &&
						record.Event.KeyEvent.bKeyDown) {
						n = 1;
						ok = true;
						break;
					} // else: just keep spinning, spinning, spinning...
					
					// TODO: In case of a window resize, redraw everything?
					// TODO: If mouse scrolling can't be disabled, redraw everything also when
					// mouse scrolls?
				} else {
					// It's an error, break the loop without setting 'ok' to true.
					break;
				}
			}
		}
		
		if (!ok) { panic(); }
		if (n > 0) break;
	}
	
	assert(record.EventType == KEY_EVENT);
	
	Editor_Key key = 0;
	switch (record.Event.KeyEvent.wVirtualKeyCode) {
		case VK_UP:     key = Editor_Key_ARROW_UP;    break;
		case VK_DOWN:   key = Editor_Key_ARROW_DOWN;  break;
		case VK_RIGHT:  key = Editor_Key_ARROW_RIGHT; break;
		case VK_LEFT:   key = Editor_Key_ARROW_LEFT;  break;
		case VK_PRIOR:  key = Editor_Key_PAGE_UP;     break;
		case VK_NEXT:   key = Editor_Key_PAGE_DOWN;   break;
		case VK_HOME:   key = Editor_Key_HOME;        break;
		case VK_END:    key = Editor_Key_END;         break;
		case VK_DELETE: key = Editor_Key_DELETE;      break;
		case VK_RETURN: key = '\n'; break;
		
		default: {
			key = record.Event.KeyEvent.uChar.AsciiChar;
		} break;
	}
	
	return key;
}

#elif OS_LINUX

//- Linux-specific types

typedef struct termios termios;
typedef struct winsize winsize;
typedef struct utsname utsname;

//- Linux-specific global variables

static termios original_mode;
static int _wsl_mode;

//- Linux-specific initialization/finalization

// Code for detecting wether we are in a WSL or in actual Linux:
//   https://github.com/scivision/detect-windows-subsystem-for-linux

static bool
_wsl_detect_str_ends_with(const char *s, const char *suffix) {
	/* https://stackoverflow.com/a/41652727 */
    size_t slen = strlen(s);
    size_t suffix_len = strlen(suffix);
	
    return suffix_len <= slen && !strcmp(s + slen - suffix_len, suffix);
}

static int
_wsl_detect(void) {
	// -1: Windows, not WSL
	//  0: Actual Linux
	//  1: WSL 1
	//  2: WSL 2
	int result = -1;
	
	utsname buf;
	if (uname(&buf) == 0) {
		if (strcmp(buf.sysname, "Linux") != 0) {
			result = 0;
		} else if (_wsl_detect_str_ends_with(buf.release, "microsoft-standard-WSL2")) {
			result = 2;
		} else if (_wsl_detect_str_ends_with(buf.release, "-Microsoft")) {
			result = 1;
		} else {
			result = 0;
		}
	}
	
	return result;
}

static void
before_main(void) {
	_wsl_mode = _wsl_detect();
}

static void
enable_raw_mode(void) {
	if (tcgetattr(STDIN_FILENO, &original_mode) != -1) {
		
		termios raw = original_mode;
		raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
		raw.c_oflag &= ~(OPOST);
		raw.c_cflag |= (CS8);
		raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
		raw.c_cc[VMIN] = 0;
		raw.c_cc[VTIME] = 1; // Set read() timeout, in tenth of a second
		
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) {
			perror("tsgetattr");
			exit_code = 1;
			
			panic(); // Temporary
		}
	} else {
		perror("tcgetattr");
		exit_code = 1;
		
		panic(); // Temporary
	}
}

static void
disable_raw_mode(void) {
	// This returns -1 and sets errno on fail. We don't care.
	(void)tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_mode);
}

//- Linux-specific console functions

static bool
query_cursor_position(Point *position) {
	bool ok = false;
	
	// As I understand it, the 'n' command will *send information to the program
	// through its standard input*, meaning the program will have to read from
	// stdin to get the response and then parse it.
	//
	// So we write the 'n' command, then we read until we find an 'R' which is the end
	// of the response...
	
	int n_written = write(STDOUT_FILENO, "\x1b[6n", 4); // n command = Device Status Report; arg 6 = cursor position
	if (n_written == 4) {
		char buf[32];
		unsigned int i = 0;
		
		while (i < sizeof(buf) - 1) {
			if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
			if (buf[i] == 'R') break;
			i += 1;
		}
		
		buf[i] = '\0';
		
		if (buf[0] == '\x1b' || buf[1] == '[') { // Make sure the response is an escape sequence
			int x = 0, y = 0;
			int sscanf_result = sscanf(&buf[2], "%d;%d", &y, &x); // Parse the report; the first number is the line (so the y) and the second is the column (so the x)
			// The position is 1-based
			
			if (sscanf_result == 2) {
				position->x = x;
				position->y = y;
				ok = true;
			}
		}
	}
	
	return ok;
}

static bool
query_window_size(Size *size) {
	bool ok = false;
	
	winsize ws = {0};
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 && ws.ws_col != 0 && ws.ws_row != 0) {
		size->width  = ws.ws_col;
		size->height = ws.ws_row;
		ok = true;
	} else if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) == 12) {
		Point cursor_position = {0};
		if (query_cursor_position(&cursor_position)) {
			size->width  = cursor_position.x;
			size->height = cursor_position.y;
			ok = true;
		}
	} else {
		panic(); // Temporary
	}
	
	return ok;
}

static String
get_clear_string(void) {
	String result = {0};
	
#if 0
	// This was a hack to fix the fact that the normal escape sequence to clear the screen doesn't work
	// on Windows terminals, so it wouldn't work in WSL.
	// The hack doesn't work either, and I have no idea why.
	
	switch (_wsl_mode) {
		case 1:
		case 2: {
			result = string(string_from_lit("\033[H\033[J"));
		} break;
		
		case 0: {
			result = string_from_lit("\x1b[2J" "\x1b[H");
		} break;
		
		default: panic();
	}
#else
	result = string_from_lit("\x1b[2J" "\x1b[H"); // Clear screen + reset cursor
#endif
	
	return result;
}

static void
clear(void) {
	write_console_unbuffered(get_clear_string());
}

static Editor_Key
wait_for_key(void) {
	char c = 0;
	while (true) {
		// 'nread' can be 1 (if we read a character) or 0 (if we timed out)
		int nread = read(STDIN_FILENO, &c, 1);
		if (nread == 1) break;
		
		if (nread == -1 && errno != EAGAIN) {
			// See note on the tutorial about EAGAIN on Cygwin.
			panic(); // read failed - Temporary; TODO: What to do? The tutorial just quits; NOTE: Maybe set a global 'should_quit' variable
		}
	}
	
	Editor_Key key = c;
	if (key == '\x1b') {
		// If we read the escape character (\x1b), there's a possibility that
		// it was the first byte in an escape sequence, so try to read
		// more characters.
		
		char seq[3] = {0};
		if (read(STDIN_FILENO, &seq[0], 1) == 1 &&
			read(STDIN_FILENO, &seq[1], 1) == 1) {
			// If we read 2 more characters after 1x1b before read() timed out:
			// It probabily is an escape code, so try to parse it.
			
			if (seq[0] == '[') {
				if (isdigit(seq[1])) {
					if (read(STDIN_FILENO, &seq[2], 1) == 1) {
						if (seq[2] == '~') {
							switch (seq[1]) {
								case '1': key = Editor_Key_HOME; break;
								case '3': key = Editor_Key_DELETE; break;
								case '4': key = Editor_Key_END; break;
								case '5': key = Editor_Key_PAGE_UP; break;
								case '6': key = Editor_Key_PAGE_DOWN; break;
								case '7': key = Editor_Key_HOME; break;
								case '8': key = Editor_Key_END; break;
							}
						}
					}
				} else {
					switch (seq[1]) {
						case 'A': key = Editor_Key_ARROW_UP;    break;
						case 'B': key = Editor_Key_ARROW_DOWN;  break;
						case 'C': key = Editor_Key_ARROW_RIGHT; break;
						case 'D': key = Editor_Key_ARROW_LEFT;  break;
						case 'H': key = Editor_Key_HOME; break;
						case 'F': key = Editor_Key_END; break;
					}
				}
			} else if (seq[0] == '0') {
				switch (seq[1]) {
					case 'H': key = Editor_Key_HOME; break;
					case 'F': key = Editor_Key_END; break;
				}
			}
		}
	}
	
	return key;
}

#endif

int main(int argc, char **argv) {
	
	before_main();
	enable_raw_mode();
	
	logfile = fopen("log.txt", "w");
	assert(logfile);
	
	{
		// Init state
		arena_init(&state.arena);
		
		if (!query_window_size(&state.window_size)) {
			panic();
		}
		
		{
			// Init null buffer
			state.null_buffer = push_type(&state.arena, Editor_Buffer);
			state.null_buffer->name = string_from_lit("*null*");
			
			// There's no point in creating an arena specifically for this buffer.
			// This is the only buffer that uses the global state's arena for
			// its allocations.
			
			state.null_buffer->first_page = push_type(&state.arena, Editor_Page);
			state.null_buffer->last_page = state.null_buffer->first_page;
			state.null_buffer->page_count = 1;
			
			state.null_buffer->first_page->lines = push_array(&state.arena, Editor_Line, EDITOR_PAGE_SIZE);
			state.null_buffer->first_page->line_count = 1;
			state.null_buffer->line_count = 1;
			
			state.null_buffer->first_page->lines[0].first_span = push_type(&state.arena, Editor_Span);
			state.null_buffer->first_page->lines[0].last_span = state.null_buffer->first_page->lines[0].first_span;
			
			state.null_buffer->first_page->lines[0].first_span->data = cast(u8 *) "~";
			state.null_buffer->first_page->lines[0].first_span->len = 1;
			
			state.null_buffer->is_read_only = true;
		}
		
		state.current_buffer = state.null_buffer;
	}
	
	{
		// Parse command-line args
		if (argc > 1) {
			String file_name = string_from_cstring(argv[1]);
			editor_load_file(file_name);
			
			state.current_buffer = state.single_buffer;
		}
	}
	
	assert(state.current_buffer); // Always!
	
	editor_set_status_message(string_from_lit("Ctrl-Q to quit"));
	
	while (true) {
		assert(state.current_buffer); // Always!
		editor_validate_buffer(state.current_buffer); // Always!
		
		editor_update_scroll();
		
		editor_render_buffer(state.current_buffer);
		
		if (!query_window_size(&state.window_size)) {
			panic();
		}
		
		Editor_Key key = wait_for_key();
		switch (key) {
			case CTRL_KEY('q'): {
				clear();
				goto main_loop_end;
			} break;
			
			case Editor_Key_ARROW_UP:
			case Editor_Key_ARROW_LEFT:
			case Editor_Key_ARROW_DOWN:
			case Editor_Key_ARROW_RIGHT: {
				editor_move_cursor(key);
			} break;
			
			case Editor_Key_PAGE_UP:
			case Editor_Key_PAGE_DOWN: {
				Editor_Key direction = key == Editor_Key_PAGE_UP ? Editor_Key_ARROW_UP : Editor_Key_ARROW_DOWN;
				for (int step = 0; step < state.window_size.height; step += 1) {
					editor_move_cursor(direction);
				}
			} break;
			
			case Editor_Key_HOME: {
				state.current_buffer->cursor_position.x = 0;
			} break;
			
			case Editor_Key_END: {
				Editor_Line *current_line = editor_get_current_line();
				state.current_buffer->cursor_position.x = cast(i32) editor_line_len(current_line);
			} break;
			
			default: {
				if (isprint(key)) {
#if 0
					char c = cast(char) key;
					
					Editor_Line *current_line = editor_get_current_line();
					if (current_line->len < current_line->cap) {
						i64 at = state.current_buffer->cursor_position.x;
						i64 after = current_line->len - at;
						memmove(current_line->data + at, current_line->data + at + 1, after);
						(current_line->data + at)[0] = c;
						current_line->len += 1;
					}
					
					allow_break();
#endif
				} else if (key == '\n') {
					//- Split line
					
					Editor_Buffer *buffer = state.current_buffer;
					Editor_Relative_Line rel = editor_relative_from_absolute_line(buffer->cursor_position.y);
					
					Editor_Page *current_page = rel.page;
					Editor_Line *current_line = &current_page->lines[rel.line];
					i64 relative_cursor_line = rel.line;
					
					i64 at = buffer->cursor_position.x;
					Editor_Relative_Span rels = editor_relative_span_from_line_and_pos(current_line, at);
					Editor_Span *current_span = rels.span;
					
					bool insert_before = false;
					if (current_page->line_count == EDITOR_PAGE_SIZE) {
						// Allocate new page after this one
						Editor_Page *new_page;
						if (buffer->first_free_page) {
							new_page = stack_pop(buffer->first_free_page);
							new_page->next = NULL;
							new_page->prev = NULL;
							new_page->line_count = 0;
						} else {
							new_page = push_type(&buffer->arena, Editor_Page);
							new_page->lines = push_array(&buffer->arena, Editor_Line, EDITOR_PAGE_SIZE);
						}
						
						// Insert page
						dll_insert(buffer->first_page, buffer->last_page, current_page, new_page);
						
						// Copy last line into new page
						new_page->lines[0] = current_page->lines[EDITOR_PAGE_SIZE - 1];
						new_page->line_count += 1;
						
						current_page->line_count -= 1;
						
					} else {
						assert(relative_cursor_line <= EDITOR_PAGE_SIZE);
					}
					
					// Shift all the lines of this page
					i64 lines_after_cursor = EDITOR_PAGE_SIZE - relative_cursor_line - 1;
					memmove(current_page->lines + relative_cursor_line + 1, current_page->lines + relative_cursor_line, lines_after_cursor * sizeof(Editor_Line));
					
					if (relative_cursor_line == EDITOR_PAGE_SIZE - 1) {
						current_page->line_count += 1;
						current_page = current_page->next;
						current_page->line_count -= 1;
						memset(&current_page->lines[0], 0, sizeof(Editor_Line));
						relative_cursor_line = 0;
						insert_before = true;
						// @Hack?
					}
					
					// Alloc new span
					Editor_Span *new_span;
					if (buffer->first_free_span) {
						new_span = stack_pop(buffer->first_free_span);
						memset(new_span->data, 0, EDITOR_SPAN_SIZE);
						new_span->next = NULL;
						new_span->len  = 0;
					} else {
						new_span = push_type(&buffer->arena, Editor_Span);
						new_span->data = push_array(&buffer->arena, u8, EDITOR_SPAN_SIZE);
					}
					
					// Copy data
					i64 at_in_span = rels.at;
					i64 after_in_span = current_span->len - at_in_span;
					memcpy(new_span->data, current_span->data + at_in_span, after_in_span);
					current_span->len = at_in_span;
					
					// memmove(current_span->data, current_span->data + at_in_span, after_in_span);
					new_span->len = after_in_span;
					
					// Insert span
					current_span->next = NULL;
					if (insert_before) {
						current_page->lines[relative_cursor_line].first_span = new_span;
					} else {
						current_page->lines[relative_cursor_line+1].first_span = new_span;
					}
					current_page->line_count += 1;
					buffer->line_count += 1;
					
					// Reposition cursor
					editor_move_cursor(Editor_Key_ARROW_DOWN);
					state.current_buffer->cursor_position.x = 0; // Copy the behaviour of HOME
				}
			} break;
		}
	}
	
	main_loop_end:;
	
	fclose(logfile);
	
	disable_raw_mode();
	return exit_code;
}
