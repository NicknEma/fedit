#include "fedit_ctx_crack.h"
#include "fedit_base.h"

#include "fedit_base.c"

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
	Editor_Key_BACKSPACE = 127,
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

typedef struct Editor_Relative_Span Editor_Relative_Span;
struct Editor_Relative_Span {
	Editor_Span *span;
	i64 at;
};

//- Editor global variables

static int exit_code = 0;
static Editor_State state;

static FILE *logfile;

//- Editor generic functions (declarations)

static void editor_init_buffer_contents(Editor_Buffer *buffer, SliceU8 contents);
static void editor_load_file(String file_name);
static bool editor_buffer_is_in_use(Editor_Buffer *buffer);

static i64 editor_line_len(Editor_Line *line);
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

static Editor_Relative_Span
editor_relative_span_from_line_and_pos(Editor_Line *line, i64 pos) {
	Editor_Relative_Span result = {0};
	
	Editor_Span *span = line->first_span;
	while (span) {
		// We need to add 1 here because empty spans (and therefore empty lines) are allowed.
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

static Editor_Relative_Line
editor_relative_from_absolute_line(i64 absolute_line) {
	Editor_Relative_Line result = {0};
	
	i64 line = absolute_line;
	Editor_Page *page = state.current_buffer->first_page;
	while (page) {
		// We *DON'T* add 1 here because a file must contain at least 1 page with at least 1 line.
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
	{
		// Check that global line-count is valid
		i64 line_count = 0;
		
		Editor_Page *page = buffer->first_page;
		while (page) {
			line_count += page->line_count;
			page = page->next;
		}
		
		assert(line_count == buffer->line_count);
		assert(line_count > 0);
	}
	
	{
		// Check that each line has at least a span
		Editor_Page *page = buffer->first_page;
		while (page) {
			for (i64 line_index = 0; line_index < page->line_count; line_index += 1) {
				assert(page->lines[line_index].first_span);
				assert(page->lines[line_index].last_span);
			}
			page = page->next;
		}
	}
	
	{
		// Check that first page has at least 1 line
		Editor_Page *page = buffer->first_page;
		assert(page->line_count > 0);
	}
	
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
			line->last_span  = NULL;
			
			i64 line_len = line_end - line_start;
			i64 copied = 0;
			
			while (copied < line_len || !line->first_span) {
				Editor_Span *span = push_type(&buffer->arena, Editor_Span);
				queue_push(line->first_span, line->last_span, span);
				
				span->data = push_array(&buffer->arena, u8, EDITOR_SPAN_SIZE);
				
				i64 to_copy = line_len - copied;
				i64 to_copy_now = min(to_copy, EDITOR_SPAN_SIZE);
				memcpy(span->data, contents.data + line_start + copied, to_copy_now);
				span->len = to_copy_now;
				
				copied += to_copy_now;
			}
			
			assert(editor_line_len(line) == line_len);
			
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
	
	Editor_Buffer *buffer = state.current_buffer;
	
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
	if (buffer->cursor_position.y < buffer->vscroll) {
		buffer->vscroll = buffer->cursor_position.y;
	}
	if (buffer->cursor_position.y >= buffer->vscroll + (state.window_size.height - 2)) {
		buffer->vscroll = buffer->cursor_position.y - (state.window_size.height - 2) + 1;
	}
	
	Scratch scratch = scratch_begin(0, 0);
	
	Editor_Line *line = editor_line_from_line_number(buffer->cursor_position.y);
	i64 cursor_render_x = editor_render_x_from_stored_x(string_from_editor_line(scratch.arena, line), buffer->cursor_position.x);
	
	// Horizontal scroll
	if (cursor_render_x < buffer->hscroll) {
		buffer->hscroll = cursor_render_x;
	}
	if (cursor_render_x >= buffer->hscroll + state.window_size.width) {
		buffer->hscroll = cursor_render_x - state.window_size.width + 1;
	}
	
	scratch_end(scratch);
#endif
	
}

static void
editor_move_cursor(Editor_Key key) {
	Editor_Buffer *buffer = state.current_buffer;
	
	switch (key) {
		case Editor_Key_ARROW_LEFT: {
			if (buffer->cursor_position.x > 0) {
				buffer->cursor_position.x -= 1;
			} else {
				// Move to end of previous line
				if (buffer->cursor_position.y > 0) {
					buffer->cursor_position.y -= 1;
					Editor_Line *line = editor_line_from_line_number(buffer->cursor_position.y);
					buffer->cursor_position.x = cast(i32) editor_line_len(line);
				}
			}
		} break;
		
		case Editor_Key_ARROW_RIGHT: {
			Editor_Line *line = editor_line_from_line_number(buffer->cursor_position.y);
			if (buffer->cursor_position.x < editor_line_len(line)) {
				buffer->cursor_position.x += 1;
			} else {
				// Move to start of next line
				if (buffer->cursor_position.y < buffer->line_count - 1) {
					buffer->cursor_position.y += 1;
					buffer->cursor_position.x = 0;
				}
			}
		} break;
		
		case Editor_Key_ARROW_UP: {
			if (buffer->cursor_position.y > 0) {
				buffer->cursor_position.y -= 1;
			}
		} break;
		
		case Editor_Key_ARROW_DOWN: {
			if (buffer->cursor_position.y < buffer->line_count - 1) {
				buffer->cursor_position.y += 1;
			}
		} break;
		
		default: {
			panic("Invalid argument 'key'");
		}
	}
	
	Editor_Line *line = editor_line_from_line_number(buffer->cursor_position.y);
	i64 line_len = editor_line_len(line);
	if (buffer->cursor_position.x > line_len) {
		buffer->cursor_position.x = cast(i32) line_len;
	}
}

//- Editor misc functions

static void
editor_set_status_message(String message) {
	state.status_message = string_clone_buffer(state.status_message_buffer, sizeof(state.status_message_buffer), message);
	state.status_message_timestamp = time(NULL);
}

#if OS_WINDOWS
# include "fedit_windows.c"
#elif OS_LINUX
# include "fedit_linux.c"
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
			
			case Editor_Key_BACKSPACE:
			case Editor_Key_DELETE:
			case CTRL_KEY('h'): {
				
			} break;
			
			case CTRL_KEY('l'):
			case '\x1b': {
				allow_break();
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
					if (!buffer->is_read_only) {
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
				}
			} break;
		}
	}
	
	main_loop_end:;
	
	fclose(logfile);
	
	disable_raw_mode();
	return exit_code;
}
