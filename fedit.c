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
# include <sys/utsname.h>
#else
# error Platform not supported.
#endif

#if 0
#ifndef  __has_include
# define __has_include(x) 0
#endif

#if __has_include(<sys/utsname.h>)
# include <sys/utsname.h> // If it's a standard Linux header, there's no point in checking with __has_include: just check for OS_LINUX
#endif
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

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

static void *alloc_safe(size_t size);

static void *
alloc_safe(size_t size) {
	void *result = malloc(size);
	assert(result);
	return result;
}

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

#define cast(t) (t)
#define array_count(a) (sizeof(a)/sizeof((a)[0]))
#define allow_break() do { int __x__ = 0; (void)__x__; } while (0)
#define panic(...) assert(0)

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

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
string_clone(String s) {
	// TODO: Arenas
	
	String result = {
		.data = alloc_safe(s.len),
		.len  = s.len,
	};
	memcpy(result.data, s.data, s.len);
	return result;
}

static char *
cstring_from_string(String s) {
	// TODO: Arenas
	
	char *result = alloc_safe((s.len + 1) * sizeof(char));
	memcpy(result, s.data, s.len);
	result[s.len] = 0;
	return result;
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
//~ File IO

//- File IO types

typedef struct Read_File_Result Read_File_Result;
struct Read_File_Result {
	SliceU8 contents;
	bool ok;
};

//- File IO functions

static Read_File_Result read_file(String file_name);

static Read_File_Result
read_file(String file_name) {
	Read_File_Result result = {0};
	
	char *file_name_ = cstring_from_string(file_name);
	assert(file_name_); // TODO: Arenas
	
	FILE *handle = fopen(file_name_, "rb");
	if (handle) {
		errno = 0;
		size_t size = fsize(handle);
		if (errno == 0) {
			result.contents.len  = size;
			result.contents.data = malloc(size * sizeof(u8)); // Caller is responsible for freeing. TODO: Arenas
			assert(result.contents.data); // TODO: Switch to an if-else?
			
			i64 read_amount  = fread(result.contents.data,
									 sizeof(u8),
									 result.contents.len,
									 handle);
			if (read_amount == result.contents.len || feof(handle)) {
				result.ok = true;
			} else {
				// TODO: something
			}
		} else {
			// TODO: something
		}
		fclose(handle);
	} else {
		// TODO: something
	}
	
	free(file_name_);
	
	return result;
}

////////////////////////////////
//~ Console output helpers

//- Console types

typedef struct Write_Buffer Write_Buffer;
struct Write_Buffer {
	i64  cap;
	i64  len;
	u8  *data;
};

//- Console platform-specific functions

static void write_console_unbuffered(String s);

//- Console generic functions

static void write_buffer_init(Write_Buffer *buffer, u8 *data, i64 cap);
static void write_buffer_append(Write_Buffer *buffer, String s);
static void write_buffer_flush(Write_Buffer *buffer);

static void
write_buffer_init(Write_Buffer *buffer, u8 *data, i64 cap) {
	buffer->data = data;
	buffer->cap  = cap;
	buffer->len  = 0;
}

static void
write_buffer_append(Write_Buffer *buffer, String s) {
	assert(buffer->data); // Not initialized
	
	i64 available = buffer->cap - buffer->len;
	i64 to_copy = min(available, s.len);
	memcpy(buffer->data + buffer->len, s.data, to_copy);
	buffer->len += to_copy;
	
	if (s.len > available) {
		assert(buffer->len == buffer->cap);
		
		write_buffer_flush(buffer);
		memcpy(buffer->data, s.data + available, s.len - available);
		buffer->len += (s.len - available);
	}
}

static void
write_buffer_flush(Write_Buffer *buffer) {
	write_console_unbuffered(string(buffer->data, buffer->len));
	buffer->len = 0;
}

////////////////////////////////
//~ Editor

#define ESCAPE_PREFIX "\x1b["

#define CTRL_KEY(k) ((k) & 0x1f)

#define esc(code) string_from_lit(ESCAPE_PREFIX code)

#define EDITOR_DEFAULT_LINE_SIZE 64
#define EDITOR_DEFAULT_PAGE_SIZE 64

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

typedef struct Editor_Line Editor_Line;
struct Editor_Line {
	Editor_Line *next;
	
	i64  len;
	i64  cap;
	u8  *data;
};

typedef struct Editor_Page Editor_Page;
struct Editor_Page {
	Editor_Page *next;
	Editor_Page *prev;
	
	Editor_Line *lines;
	i64 line_count;
	i64 line_capacity;
};

typedef struct Editor_State Editor_State;
struct Editor_State {
	Size window_size;
	Point cursor_position;
	
	String file_name;
	Editor_Page *first_page;
	Editor_Page *last_page;
	i64 page_count;
	i64 line_count;
	
	Editor_Page *first_free_page;
	Editor_Line *first_free_line;
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

//- Editor generic functions

static void editor_move_cursor(Editor_Key key);
static void editor_hardcode_initial_contents(void);
static void editor_init_buffer(SliceU8 contents);
static void editor_load_file(String file_name);

static Editor_Relative_Line editor_relative_from_absolute_line(i64 absolute_line);

static Editor_Relative_Line
editor_relative_from_absolute_line(i64 absolute_line) {
	Editor_Relative_Line result = {0};
	
	i64 line_remaining = absolute_line;
	
	Editor_Page *page = state.first_page;
	i64 page_index = 0;
	while (page) {
		if (line_remaining < page->line_count) {
			result.page = page;
			result.line = line_remaining;
			break;
		}
		
		line_remaining -= page->line_count;
		
		page_index += 1;
		page = page->next;
	}
	
	return result;
}

//- Editor platform-specific functions

static void clear(void);
static String get_clear_string(void);

static bool query_window_size(Size *size);
static bool query_cursor_position(Point *position);

static Editor_Key wait_for_key(void);

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
			
			assert(0); // Temporary
		}
	} else {
		int n = GetLastError();
		(void)n;
		// TODO: Set errno + call perror()
		exit_code = 1;
		
		assert(0); // Temporary
	}
	
	if (GetConsoleMode(stdout_handle, &original_stdout_mode)) {
		DWORD mode = original_stdout_mode;
		mode |= (ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
		
		if (!SetConsoleMode(stdout_handle, mode)) {
			assert(0); // Temporary
		}
	} else {
		assert(0); // Temporary
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
	
	// TODO: When we set the cursor position with the escape codes, the position is 1-based
	// When we query it with this API, it is 0-based.
	// Change this to use escape codes like the Linux version.
	
#if 0
	
	CONSOLE_SCREEN_BUFFER_INFO info = {0};
	if (GetConsoleScreenBufferInfo(stdout_handle, &info)) {
		position->x = info.dwCursorPosition.X;
		position->y = info.dwCursorPosition.Y;
		ok = true;
	} else {
		// As a fallback method, we could do what Linux does: send a 'n' command
		// and parse the response. I don't know if it makes sense though, for Windows
		
		assert(0); // Temporary
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
		size->width  = info.srWindow.Right - info.srWindow.Left;
		size->height = info.srWindow.Bottom - info.srWindow.Top;
		ok = true;
	} else {
		assert(0); // Temporary
	}
	
	return ok;
}

// NOTE: Clearing the screen in Windows is weird, because the normal escape code doesn't work,
// so you either have to use the alternative one:
//   https://stackoverflow.com/questions/48612106/windows-console-esc2j-doesnt-really-clear-the-screen
// or use the deprecated console API:
//   https://stackoverflow.com/questions/6486289/how-to-clear-the-console-in-c
//   https://stackoverflow.com/questions/5866529/how-do-we-clear-the-console-in-assembly/5866648#5866648

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

static String
get_clear_string(void) {
	return string_from_lit("\033[H\033[J");
}

#if 0
static bool
_read_console_char_with_timeout(char *c, int *n) {
	bool ok = false;
	
	while (true) {
		DWORD wres = WSAWaitForMultipleEvents(1, &stdin_handle, FALSE, 100, TRUE);
		if (wres == WSA_WAIT_TIMEOUT) {
			ok = true;
			*n = 0;
			break;
		}
		
		if (wres == WSA_WAIT_EVENT_0) {
			
			INPUT_RECORD record;
			DWORD numRead;
			
			if (ReadConsoleInput(GetStdHandle(STD_INPUT_HANDLE), &record, 1, &numRead)) {
				assert(numRead > 0); // Because we're in the WSA_WAIT_EVENT_0 case.
				
				if (record.EventType == KEY_EVENT &&
					record.Event.KeyEvent.bKeyDown) {
					*c = record.Event.KeyEvent.uChar.AsciiChar;
					*n = 1;
					ok = true;
					break;
				} // else: just keep spinning, spinning, spinning...
			} else {
				// It's an error, break the loop without setting 'ok' to true.
				break;
			}
		}
	}
	
	return ok;
}
#endif

static Editor_Key
wait_for_key(void) {
	
#if 0
	INPUT_RECORD rec = {0};
	DWORD recn = 0;
	BOOL peek_ok = PeekConsoleInput(stdin_handle, &rec, 1, &recn);
	
	char c = 0;
	if (peek_ok) {
		allow_break();
	}
	if (peek_ok && recn > 0) {
		allow_break();
	}
	
	if (peek_ok && recn > 0) {
		if (rec.EventType == KEY_EVENT) {
			DWORD n = 0;
			BOOL ok = ReadConsole(stdin_handle, &c, 1, &n, NULL);
			assert(ok); // TODO: What to do?
		} else {
			BOOL read_ok = ReadConsoleInput(stdin_handle, &rec, 1, &recn);
			assert(read_ok); // Temporary
		}
	} else {
		assert(peek_ok); // Temporary
	}
#elif 0
	
	/*
while (true) {
  PeekConsoleInput(&rec, &nrec)
if (nrec > 0 && rec.kind == KEY) break; // If there's one key event, break
}

ReadConsoleInput(&rec, &nrec);

if rec.key == '\x1b' {
while(true) {
PeekConsoleInput(&rec, &nrec)
if nrec == 0 break
if rec.kind == KEY {
seq[i++] = rec.key
}
}

if seq[0] blah ...
}
*/
	
	INPUT_RECORD rec = {0};
	DWORD nrec = 0;
	while (true) {
		if (PeekConsoleInput(stdin_handle, &rec, 1, &nrec)) {
			if (nrec > 0 && rec.EventType == KEY_EVENT) break;
		} else {
			assert(0); // Temporary; TODO: What to do?
		}
	}
	
	if (ReadConsoleInput(stdin_handle, &rec, 1, &nrec)) {
		
	} else {
		assert(0); // Temporary; TODO: What to do?
	}
	
#else
	
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
					// TODO: If mouse scrolling can't be disable, redraw everything also when
					// mouse scrolls?
				} else {
					// It's an error, break the loop without setting 'ok' to true.
					break;
				}
			}
		}
		
		if (!ok) { assert(0); }
		if (n > 0) break;
	}
	
	assert(record.EventType == KEY_EVENT);
	
	Editor_Key key = 0;
	switch (record.Event.KeyEvent.wVirtualKeyCode) {
		case VK_UP: key = Editor_Key_ARROW_UP;    break;
		case VK_DOWN: key = Editor_Key_ARROW_DOWN;  break;
		case VK_RIGHT: key = Editor_Key_ARROW_RIGHT; break;
		case VK_LEFT: key = Editor_Key_ARROW_LEFT;  break;
		case VK_PRIOR: key = Editor_Key_PAGE_UP; break;
		case VK_NEXT: key = Editor_Key_PAGE_DOWN; break;
		case VK_HOME: key = Editor_Key_HOME; break;
		case VK_END: key = Editor_Key_END; break;
		case VK_DELETE: key = Editor_Key_DELETE; break;
		
		default: {
			key = record.Event.KeyEvent.uChar.AsciiChar;
		} break;
	}
	
#endif
	
	return key;
}

static void
write_console_unbuffered(String s) {
	assert(cast(i64)cast(u32)s.len == s.len); // If false, the length is not supported
	
	DWORD len_written = 0;
	BOOL ok = WriteConsole(stdout_handle, s.data, cast(u32)s.len, &len_written, NULL);
	if (!ok) {
		int n = GetLastError();
		(void)n;
		
		assert(0);
	}
}

#elif OS_LINUX

//- Linux-specific types

typedef struct termios termios;
typedef struct winsize winsize;

//- Linux-specific global variables

static termios original_mode;
static int wsl_mode;

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
	
#if __has_include(<sys/utsname.h>) // See if it makes sense to check for the header
	struct utsname buf;
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
#endif
	
	return result;
}

static void
before_main(void) {
	wsl_mode = _wsl_detect();
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
			
			assert(0); // Temporary
		}
	} else {
		perror("tcgetattr");
		exit_code = 1;
		
		assert(0); // Temporary
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
		assert(0); // Temporary
	}
	
	return ok;
}

static void
clear(void) {
	write_console_unbuffered(get_clear_string());
}

static String
get_clear_string(void) {
	String result = {0};
	
#if 0
	// This was a hack to fix the fact that the normal escape sequence to clear the screen doesn't work
	// on Windows terminals, so it wouldn't work in WSL.
	// The hack doesn't work either, and I have no idea why.
	
	switch (wsl_mode) {
		case 1:
		case 2: {
			// printf("\033[H\033[J");
			
			result = string(string_from_lit("\033[H\033[J"));
		} break;
		
		case 0: {
			// write_console_unbuffered(esc("2J")); // Clear screen
			// write_console_unbuffered(esc("H")); // Reposition cursor
			
			result = string_from_lit("\x1b[2J\x1b[H");
		} break;
		
		default: assert(0);
	}
#else
	(void)wsl_mode;
	
	// write_console_unbuffered(esc("2J")); // Clear screen
	// write_console_unbuffered(esc("H")); // Reposition cursor
	
	result = string_from_lit("\x1b[2J\x1b[H");
#endif
	
	return result;
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
			assert(0); // read failed - Temporary; TODO: What to do? The tutorial just quits; NOTE: Maybe set a global 'should_quit' variable
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

static void
write_console_unbuffered(String s) {
	int nwrite = write(STDOUT_FILENO, s.data, s.len);
	(void)nwrite;
}

#endif

#if 0
static void
report_error(char *message) {
	console_write(cast(u8 *) "\x1b[2J", 4); // Clear screen
	console_write(cast(u8 *) "\x1b[H", 3); // Reposition cursor
	
	// fprintf(stderr, ...);
	(void)message;
}
#endif

static void
editor_hardcode_initial_contents(void) {
	String line_text = string_from_lit("Hello, world!");
	SliceU8 contents = { // TODO: Conversion function
		.data = line_text.data,
		.len  = line_text.len,
	};
	
	editor_init_buffer(contents);
}

static void
editor_init_buffer(SliceU8 contents) {
	
	state.first_page = 0;
	state.last_page = 0;
	state.page_count = 0;
	state.line_count = 0;
	state.first_free_page = 0;
	state.first_free_line = 0;
	
	Editor_Page *page = NULL;
	
	page = alloc_safe(sizeof(Editor_Page));
	page->line_count = 0;
	page->line_capacity = EDITOR_DEFAULT_PAGE_SIZE;
	page->lines = alloc_safe(page->line_capacity * sizeof(Editor_Line));
	page->next = NULL;
	dll_push_back(state.first_page, state.last_page, page);
	state.page_count += 1;
	
	// TODO: For now let's pretend that every file is LF
	
	i64 line_start = 0;
	i64 line_end = 0;
	for (i64 byte_index = 0; byte_index < contents.len; byte_index += 1) {
		if (contents.data[byte_index] == '\n') {
			line_end = byte_index;
			
			// Fill line
			Editor_Line *line = NULL;
			{
				if (page->line_count >= page->line_capacity) {
					page = alloc_safe(sizeof(Editor_Page));
					page->line_count = 0;
					page->line_capacity = EDITOR_DEFAULT_PAGE_SIZE;
					page->lines = alloc_safe(page->line_capacity * sizeof(Editor_Line));
					page->next = NULL;
					dll_push_back(state.first_page, state.last_page, page);
					state.page_count += 1;
				}
				
				line = &page->lines[page->line_count];
				page->line_count += 1;
				state.line_count += 1;
			}
			
			line->next = NULL;
			
			i64 line_len = line_end - line_start; // NOTE: This could be 0. Is alloc_safe(0) well-defined?
			i64 line_size = round_up_to_multiple_of_i64(line_len, EDITOR_DEFAULT_LINE_SIZE);
			line->data = malloc(line_size * sizeof(u8));
			if (line_len > 0) { assert(line->data); }
			
			line->len = line_len;
			line->cap = line_size;
			memcpy(line->data, contents.data + line_start, line_len);
			
			// Prepare for next iteration
			line_start = line_end + 1;
			
			allow_break();
		}
	}
	
	allow_break();
}

static void
editor_load_file(String file_name) {
	// NOTE: !!!!! For now, overwrite previously loaded file. @Leak
	
	Read_File_Result rf_result = read_file(file_name);
	if (rf_result.ok) {
		editor_init_buffer(rf_result.contents);
		free(rf_result.contents.data);
		state.file_name = string_clone(file_name);
	} else {
		// TODO: Something
	}
}

static void
editor_move_cursor(Editor_Key key) {
	
	switch (key) {
		case Editor_Key_ARROW_LEFT: {
			if (state.cursor_position.x > 0) {
				state.cursor_position.x -= 1;
			}
		} break;
		case Editor_Key_ARROW_RIGHT: {
			if (state.cursor_position.x < state.window_size.width - 1) {
				state.cursor_position.x += 1;
			}
		} break;
		case Editor_Key_ARROW_UP: {
			if (state.cursor_position.y > 0) {
				state.cursor_position.y -= 1;
			}
		} break;
		case Editor_Key_ARROW_DOWN: {
			if (state.cursor_position.y < state.window_size.height - 1) {
				state.cursor_position.y += 1;
			}
		} break;
		default: {
			panic("Invalid switch case");
		}
	}
	
}

int main(void) {
	
	before_main();
	enable_raw_mode();
	
	logfile = fopen("log.txt", "w");
	assert(logfile);
	
	bool size_ok = query_window_size(&state.window_size);
	assert(size_ok);
	
	// editor_hardcode_initial_contents();
	// editor_load_file(string_from_lit("fedit.c"));
	
	Write_Buffer screen_buffer;
	u8 screen_buffer_data[1024];
	write_buffer_init(&screen_buffer, screen_buffer_data, sizeof(screen_buffer_data));
	
	while (true) {
		
		{
			write_buffer_append(&screen_buffer, esc("?25l")); // Hide cursor
			
			write_buffer_append(&screen_buffer, get_clear_string()); // Clear screen
			
#if 0
			// Temporary: draw a row
			// -1 because we don't print in the last column
			for (int x = 0; x < state.window_size.width - 1; x += 1) {
				write_buffer_append(&screen_buffer, string_from_lit("~"));
			}
			write_buffer_append(&screen_buffer, string_from_lit("\r\n"));
#endif
			
			
#if 0
			{
				// Draw ~ for each row
				for (int y = 0; y < state.window_size.height; y += 1) {
					if (y >= state.line_count) {
						// TODO: Display the welcome message
						
						write_buffer_append(&screen_buffer, string_from_lit("~"));
					} else {
						i64 to_write = min(state.lines[y].len, state.window_size.width);
						write_buffer_append(&screen_buffer, string(state.lines[y].data, to_write));
					}
					
					// This doesn't work in Windows terminals, we have to clear the whole screen
					// using the special code because Windows is a special kid.
					// write_buffer_append(&screen_buffer, string_from_lit("\x1b[K")); // Clear row
					if (y < state.window_size.height - 1) {
						write_buffer_append(&screen_buffer, string_from_lit("\r\n"));
					}
				}
			}
#else
			{
				i64 line_number = 0; // Absolute line number from the start of the buffer, the first that is visible
				
				Editor_Relative_Line rel_line = editor_relative_from_absolute_line(line_number);
				Editor_Page *page = rel_line.page;
				i64 line_relative_to_start_of_page = rel_line.line;
				
				for (int y = 0; y < state.window_size.height; y += 1) {
					if (page && line_relative_to_start_of_page < page->line_count) {
						// print line
						Editor_Line *line = &page->lines[line_relative_to_start_of_page];
						
						i64 to_write = min(line->len, state.window_size.width);
						write_buffer_append(&screen_buffer, string(line->data, to_write));
						
						
						// check for end of page
						line_relative_to_start_of_page += 1;
						if (line_relative_to_start_of_page == page->line_count) {
							page = page->next;
							line_relative_to_start_of_page = 0;
						}
					} else {
						if (!state.first_page && y == state.window_size.height / 3) {
							
#define FEDIT_VERSION "1"
							char welcome[80];
							int  welcomelen = snprintf(welcome, sizeof(welcome), "Fedit -- version %s", FEDIT_VERSION);
							int to_write = min(welcomelen, state.window_size.width);
							int padding = (state.window_size.width - to_write) / 2;
							if (padding != 0) {
								write_buffer_append(&screen_buffer, string_from_lit("~"));
								padding -= 1;
							}
							while (padding != 0) {
								write_buffer_append(&screen_buffer, string_from_lit(" "));
								padding -= 1;
							}
							write_buffer_append(&screen_buffer, string(cast(u8 *) welcome, welcomelen));
						} else {
							write_buffer_append(&screen_buffer, string_from_lit("~"));
						}
					}
					
					// This doesn't work in Windows terminals, we have to clear the whole screen
					// using the special code because Windows is a special kid.
					// write_buffer_append(&screen_buffer, string_from_lit("\x1b[K")); // Clear row
					if (y < state.window_size.height - 1) {
						write_buffer_append(&screen_buffer, string_from_lit("\r\n"));
					}
				}
			}
#endif
			
			write_buffer_append(&screen_buffer, esc("H")); // Reset cursor
			
			// Move cursor
			char buf[32] = {0};
			snprintf(buf, sizeof(buf), "\x1b[%d;%dH", state.cursor_position.y + 1, state.cursor_position.x + 1);
			write_buffer_append(&screen_buffer, string_from_cstring(buf));
			
			write_buffer_append(&screen_buffer, esc("?25h")); // Show cursor
			
			write_buffer_flush(&screen_buffer);
		}
		
		size_ok = query_window_size(&state.window_size);
		assert(size_ok);
		
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
				state.cursor_position.x = 0;
			} break;
			
			case Editor_Key_END: {
				state.cursor_position.x = state.window_size.width - 1;
			} break;
		}
	}
	
	main_loop_end:;
	
	fclose(logfile);
	
	disable_raw_mode();
	return exit_code;
}
