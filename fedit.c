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

static int exit_code = 0;

#if OS_WINDOWS

static DWORD original_mode;
static HANDLE stdin_handle;

static void
before_main(void) {
	stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
	assert(stdin_handle && stdin_handle != INVALID_HANDLE_VALUE);
}

static void
enable_raw_mode(void) {
	if (GetConsoleMode(stdin_handle, &original_mode)) {
		DWORD mode = original_mode;
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
}

static void
disable_raw_mode(void) {
	(void)SetConsoleMode(stdin_handle, original_mode);
}

static char
read_byte(void) {
	INPUT_RECORD rec = {0};
	DWORD recn = 0;
	BOOL peek_ok = PeekConsoleInput(stdin_handle, &rec, 1, &recn);
	
	char c = 0;
	if (peek_ok && recn > 0) {
		DWORD n = 0;
		BOOL ok = ReadConsole(stdin_handle, &c, 1, &n, NULL);
		assert(ok); // TODO: What to do?
	} else {
		assert(peek_ok);
	}
	return c;
}

#elif OS_LINUX

typedef struct termios termios;

static termios original_mode;

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

static char
read_byte(void) {
	char c = 0;
	int r = read(STDIN_FILENO, &c, 1);
	
	// r can be 1 (if we read a character) or 0 (if we timed out)
	assert(r != -1 && errno != EAGAIN); // TODO: What to do? The tutorial just quits
	// See note on the tutorial about EAGAIN on Cygwin.
	return c;
}

#endif

int main(void) {
	before_main();
	enable_raw_mode();
	
	char c;
	while (true) {
		c = read_byte();
		if (iscntrl(c)) {
			printf("%d\r\n", c);
		} else {
			printf("%d ('%c')\r\n", c, c);
		}
		if (c == 'q') break;
	}
	
	disable_raw_mode();
	return exit_code;
}
