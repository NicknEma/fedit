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
# undef min
# undef max
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

////////////////////////////////
//~ Base

#define cast(t) (t)
#define array_count(a) (sizeof(a)/sizeof((a)[0]))
#define allow_break() do { int __x__ = 0; (void)__x__; } while (0)
#define panic(...) assert(0)

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

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

typedef struct Editor_State Editor_State;
struct Editor_State {
	Size window_size;
	Point cursor_position;
};

//- Editor global variables

static int exit_code = 0;
static Editor_State state;

static FILE *logfile;

//- Editor generic functions

static void editor_move_cursor(Editor_Key key);

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
	
	Write_Buffer screen_buffer;
	u8 screen_buffer_data[1024];
	write_buffer_init(&screen_buffer, screen_buffer_data, sizeof(screen_buffer_data));
	
	while (true) {
		
		{
			write_buffer_append(&screen_buffer, esc("?25l")); // Hide cursor
			
			write_buffer_append(&screen_buffer, get_clear_string()); // Clear screen
			
			// Temporary: draw a row
			// -1 because we don't print in the last column
			for (int x = 0; x < state.window_size.width - 1; x += 1) {
				write_buffer_append(&screen_buffer, string_from_lit("~"));
			}
			write_buffer_append(&screen_buffer, string_from_lit("\r\n"));
			
			{
				// Draw ~ for each row
				int y;
				for (y = 0; y < state.window_size.height - 1; y += 1) {
					write_buffer_append(&screen_buffer, string_from_lit("~\r\n"));
				}
				write_buffer_append(&screen_buffer, string_from_lit("~"));
			}
			
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
