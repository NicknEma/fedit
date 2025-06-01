#ifndef FEDIT_H
#define FEDIT_H

////////////////////////////////
//~ Editor

//- Editor constants

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
	Editor_Span *prev;
	
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

//- Editor generic functions

static Editor_Span *editor_push_span(Arena *arena);
static Editor_Span *editor_alloc_span(Editor_Buffer *buffer);

static Editor_Span *editor_span_append_text(Editor_Buffer *buffer, Editor_Line *line, Editor_Span *span, String text);
static void         editor_span_insert_text(Editor_Buffer *buffer, Editor_Line *line, Editor_Span *span,
											i64 at_in_span, String text);

static void editor_init_buffer_contents(Editor_Buffer *buffer, SliceU8 contents);
static bool editor_load_file(String file_name);
static bool editor_buffer_is_in_use(Editor_Buffer *buffer);

static i64 editor_line_len(Editor_Line *line);
static String string_from_editor_line(Arena *arena, Editor_Line *line);
static Editor_Relative_Line editor_relative_from_absolute_line(i64 absolute_line);
static Editor_Line *editor_line_from_line_number(i64 line_number);
static Editor_Line *editor_get_current_line();
static Editor_Relative_Span editor_relative_span_from_line_and_pos(Editor_Line *line, i64 pos);

static void editor_move_cursor(Editor_Key key);
static void editor_update_scroll(void);
static void editor_set_status_message(String message);

static void editor_render_buffer(Editor_Buffer *buffer);
static String editor_render_string_from_stored_string(Arena *arena, String stored_string);
static i64 editor_render_x_from_stored_x(String stored_string, i64 stored_x);

static void editor_validate_buffer(Editor_Buffer *buffer);

//- Editor platform-specific functions

static void before_main(void);
static void enable_raw_mode(void);
static void disable_raw_mode(void);

static void clear(void);
static String get_clear_string(void);

static bool query_window_size(Size *size);
static bool query_cursor_position(Point *position);

static Editor_Key wait_for_key(void);

//- Editor global variables

static int exit_code = 0;
static Editor_State state;

static FILE *logfile;

#endif
