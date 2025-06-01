#ifndef FEDIT_WINDOWS_C
#define FEDIT_WINDOWS_C

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

static ED_Key
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
	
	ED_Key key = 0;
	switch (record.Event.KeyEvent.wVirtualKeyCode) {
		case VK_UP:     key = ED_Key_ARROW_UP;    break;
		case VK_DOWN:   key = ED_Key_ARROW_DOWN;  break;
		case VK_RIGHT:  key = ED_Key_ARROW_RIGHT; break;
		case VK_LEFT:   key = ED_Key_ARROW_LEFT;  break;
		case VK_PRIOR:  key = ED_Key_PAGE_UP;     break;
		case VK_NEXT:   key = ED_Key_PAGE_DOWN;   break;
		case VK_HOME:   key = ED_Key_HOME;        break;
		case VK_END:    key = ED_Key_END;         break;
		case VK_DELETE: key = ED_Key_DELETE;      break;
		case VK_BACK:   key = ED_Key_BACKSPACE;   break;
		case VK_RETURN: key = '\n'; break;
		case VK_ESCAPE: key = '\x1b'; break;
		
		default: {
			key = record.Event.KeyEvent.uChar.AsciiChar;
		} break;
	}
	
	return key;
}

#endif
