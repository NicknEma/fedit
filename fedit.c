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
# include <windows.h>
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

////////////////////////////////
//~ Base

#define cast(t) (t)
#define array_count(a) (sizeof(a)/sizeof((a)[0]))

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
	i64 width;
	i64 height;
};

typedef struct Point Point;
struct Point {
	i64 x;
	i64 y;
};

////////////////////////////////
//~ Editor

#define ESCAPE_PREFIX "\x1b["

#define CTRL_KEY(k) ((k) & 0x1f)

//- Editor global variables

static int exit_code = 0;

//- Editor functions

static void clear(void);

static bool query_window_size(Size *size);
static bool query_cursor_position(Point *position);

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

static void
clear_using_console_api(void) {
    COORD topLeft  = { 0, 0 };
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO screen;
    DWORD written;
	
    GetConsoleScreenBufferInfo(console, &screen);
    FillConsoleOutputCharacterA(
								console, ' ', screen.dwSize.X * screen.dwSize.Y, topLeft, &written
								);
    FillConsoleOutputAttribute(
							   console, FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_BLUE,
							   screen.dwSize.X * screen.dwSize.Y, topLeft, &written
							   );
    SetConsoleCursorPosition(console, topLeft);
}

static void
clear_using_escape_codes(void) {
	printf("\033[H\033[J");
}

// NOTE: Clearing the screen in Windows is weird, because the normal escape code doesn't work,
// so you either have to use the alternative one:
//   https://stackoverflow.com/questions/48612106/windows-console-esc2j-doesnt-really-clear-the-screen
// or use the deprecated console API:
//   https://stackoverflow.com/questions/6486289/how-to-clear-the-console-in-c
//   https://stackoverflow.com/questions/5866529/how-do-we-clear-the-console-in-assembly/5866648#5866648

#define clear() clear_using_escape_codes()

static char
read_byte(void) {
	INPUT_RECORD rec = {0};
	DWORD recn = 0;
	BOOL peek_ok = PeekConsoleInput(stdin_handle, &rec, 1, &recn);
	
	char c = 0;
	if (peek_ok && recn > 0 && rec.EventType == KEY_EVENT) {
		DWORD n = 0;
		BOOL ok = ReadConsole(stdin_handle, &c, 1, &n, NULL);
		assert(ok); // TODO: What to do?
	} else {
		assert(peek_ok);
	}
	return c;
}

static void
console_write(u8 *data, i64 len) {
	assert(cast(i64)cast(u32)len == len); // If false, the length is not supported
	DWORD len_written = 0;
	BOOL ok = WriteConsole(stdout_handle, data, cast(u32)len, &len_written, NULL);
	if (!ok) {
		int n = GetLastError();
		(void)n;
		
		assert(0);
	}
}

#elif OS_LINUX

typedef struct termios termios;

static termios original_mode;

//- Linux-specific initialization/finalization

static void
before_main(void) {
	;
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

typedef struct winsize winsize;

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
		// printf("\r\n&buf[1]: '%s'\r\n", &buf[1]);
		
		if (buf[0] == '\x1b' || buf[1] == '[') { // Make sure the response is an escape sequence
			int x = 0, y = 0;
			int sscanf_result = sscanf(&buf[2], "%d;%d", &y, &x); // Parse the report; the first number is the line (so the y) and the second is the column (so the x)
			// Is it 0-based or 1-based?
			
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

// https://github.com/scivision/detect-windows-subsystem-for-linux

static bool
_wsl_detect_str_ends_with(const char *s, const char *suffix) {
	/* https://stackoverflow.com/a/41652727 */
    size_t slen = strlen(s);
    size_t suffix_len = strlen(suffix);
	
    return suffix_len <= slen && !strcmp(s + slen - suffix_len, suffix);
}

int _wsl_detect(void) {
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
clear(void) {
	int wsl_mode = _wsl_detect(); // TODO: Do this in before_main()
	
#if 0
	// This was a hack to fix the fact that the normal escape sequence to clear the screen doesn't work
	// on Windows terminals, so it wouldn't work in WSL.
	// The hack doesn't work either, and I have no idea why.
	
	switch (wsl_mode) {
		case 1:
		case 2: {
			printf("\033[H\033[J");
		} break;
		
		case 0: {
			printf("\x1b[2J"); // Clear screen
			printf("\x1b[H"); // Reposition cursor
		} break;
		
		default: assert(0);
	}
#else
	(void)wsl_mode;
	
	printf("\x1b[2J"); // Clear screen
	printf("\x1b[H"); // Reposition cursor
#endif
}

static char
read_byte(void) {
	// NOTE: The tutorial has a loop here to wait until the user presses a key.
	// I don't see a point since we literally set a timeout for read() specifically
	// so we don't block here...
	// Maybe, instead of just returning the character, also return a bool indicating wether
	// we read a character or not; kind of what PeekMessage does in GUI apps.
	
	char c = 0;
	int r = read(STDIN_FILENO, &c, 1);
	
	// r can be 1 (if we read a character) or 0 (if we timed out)
	assert(r != -1 && errno != EAGAIN); // TODO: What to do? The tutorial just quits; NOTE: Maybe set a global 'should_quit' variable
	// See note on the tutorial about EAGAIN on Cygwin.
	return c;
}

static void
console_write(u8 *data, i64 len) {
	write(STDOUT_FILENO, data, len);
}

#endif

static void
report_error(char *message) {
	console_write(cast(u8 *) "\x1b[2J", 4); // Clear screen
	console_write(cast(u8 *) "\x1b[H", 3); // Reposition cursor
	
	// fprintf(stderr, ...);
	(void)message;
}

int main(void) {
	
	
	before_main();
	enable_raw_mode();
	
	Size size;
	bool size_ok = query_window_size(&size);
	assert(size_ok);
	
	while (true) {
		
		// Prepare screen
		{
			clear();
			
			// Temporary: draw a row
			for (int x = 0; x < size.width; x += 1) {
				console_write(cast(u8 *) "~", 1);
			}
			
			// Draw ~ for each row
			int y;
			for (y = 0; y < size.height; y += 1) {
				console_write(cast(u8 *) "~\r\n", 3);
			}
			
			console_write(cast(u8 *) "\x1b[H", 3); // Reposition cursor
		}
		
		// TODO: If there are things to animate, this will have to do. If there aren't,
		// we could switch to a wait_for_byte() call that blocks, so we only repaint the whole
		// screen when something changes instead of as often as possible.
		// TODO: An alternative would be to just keep this non-blocking and have a flag
		// that says wether we should repaint the screen...
		char c = read_byte();
		if (c == CTRL_KEY('q')) {
			clear();
			break;
		}
	}
	
	disable_raw_mode();
	return exit_code;
}
