#ifndef FEDIT_LINUX_C
#define FEDIT_LINUX_C

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
