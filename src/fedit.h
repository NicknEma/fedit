#ifndef FEDIT_H
#define FEDIT_H

/* TODOs:
** Fix cursor positioning with tabs
** "Ghost" cursor position
** Save, Save-as, Open
** Basic syntax highlighting
** Better status bar
*/

////////////////////////////////
//~ Editor

//- Editor constants

#define FEDIT_VERSION "1"

#define CTRL_KEY(k) ((k) & 0x1f)

#define ESCAPE_BYTE   '\x1b'
#define ESCAPE_PREFIX "\x1b["

#define esc(code) string_from_lit(ESCAPE_PREFIX code)

#define ED_SPAN_SIZE 64
#define ED_PAGE_SIZE  4

#define ED_TAB_WIDTH 4

//- Editor types

enum ED_Key {
	ED_Key_BACKSPACE = 127,
	ED_Key_ARROW_UP  = 256 + 1,
	ED_Key_ARROW_LEFT,
	ED_Key_ARROW_DOWN,
	ED_Key_ARROW_RIGHT,
	ED_Key_PAGE_UP,
	ED_Key_PAGE_DOWN,
	ED_Key_HOME,
	ED_Key_END,
	ED_Key_DELETE,
};
typedef enum ED_Key ED_Key;


enum ED_Text_Action_Flags {
	ED_Text_Action_Flags_COPY   = (1<<0),
	ED_Text_Action_Flags_PASTE  = (1<<1),
	ED_Text_Action_Flags_DELETE = (1<<2),
};
typedef enum ED_Text_Action_Flags ED_Text_Action_Flags;

typedef struct ED_Delta ED_Delta;
struct ED_Delta {
	i32 delta;
	Direction direction;
	
	// TODO: Make flags
	bool cross_lines;
	bool clamp_by_window;
};

typedef struct ED_Text_Action ED_Text_Action;
struct ED_Text_Action {
	ED_Text_Action_Flags flags;
	ED_Delta delta;
	u8 character;
};

typedef struct ED_Text_Operation ED_Text_Operation;
struct ED_Text_Operation {
	Text_Range delete_range;
	String     replace_string;
	Point      new_cursor;
};


typedef struct ED_Span ED_Span;
struct ED_Span {
	ED_Span *next;
	ED_Span *prev;
	
	u8  *data;
	i64  len;
};

typedef struct ED_Line ED_Line;
struct ED_Line {
	ED_Span *first_span;
	ED_Span *last_span;
};

typedef struct ED_Page ED_Page;
struct ED_Page {
	ED_Page *next;
	ED_Page *prev;
	
	ED_Line *lines;
	i64 line_count;
};

typedef struct ED_Buffer ED_Buffer;
struct ED_Buffer {
	bool is_read_only;
	
	Arena arena;
	
	String name;
	String file_name;
	Point  cursor;
	i64    vscroll;
	i64    hscroll;
	
	ED_Page *first_page;
	ED_Page *last_page;
	i64 page_count;
	i64 line_count;
	
	ED_Page *first_free_page;
	ED_Span *first_free_span;
};

typedef struct ED_State ED_State;
struct ED_State {
	Arena arena;
	Arena frame_arena;
	
	Size  window_size;
	
	String status_message;
	u8 status_message_buffer[64];
	time_t status_message_timestamp;
	
	ED_Buffer *current_buffer;
	ED_Buffer *single_buffer;
	ED_Buffer *null_buffer;
};

//- Sinthetic types only used as return values for functions

typedef struct ED_Page_I64 ED_Page_I64;
struct ED_Page_I64 {
	ED_Page *page;
	i64 i;
};

typedef struct ED_Span_I64 ED_Span_I64;
struct ED_Span_I64 {
	ED_Span *span;
	i64 i;
};

#if 0
typedef struct ED_Page_Line ED_Page_Line;
struct ED_Page_Line {
	ED_Page *page;
	ED_Line *line;
};

typedef struct ED_Page_Line_Span_Point ED_Page_Line_Span_Point;
struct ED_Page_Line_Span_Point {
	ED_Page *page;
	ED_Line *line;
	ED_Span *span;
	Point point;
};
#endif

//- Editor elements allocation functions

static ED_Span *ed_push_span(Arena *arena);
static ED_Span *ed_alloc_span(ED_Buffer *buffer);

static ED_Page *ed_push_page(Arena *arena);
static ED_Page *ed_alloc_page(ED_Buffer *buffer);

//- Main buffer modification functions

static void  ed_buffer_remove_range(ED_Buffer *buffer, Text_Range range);
static Point ed_buffer_insert_text_at_point(ED_Buffer *buffer, Point point, String text);

//- Buffer modification helper functions

static void ed_move_lines_across_pages(ED_Page *dest_page, i64 dest_index, ED_Page *src_page, i64 src_index, i64 count);
static ED_Span *ed_span_append_text_without_newlines(ED_Buffer *buffer, ED_Line *line, ED_Span *span, String text);
static void ed_clear_line(ED_Buffer *buffer, ED_Line *line, bool deep_clean);
static void ed_clear_line_range_across_pages(ED_Buffer *buffer, ED_Page *page, i64 start, i64 end, bool deep_clean);

//- General helper functions

static i64 ed_line_len(ED_Line *line);
static String ed_string_from_line(Arena *arena, ED_Line *line);

static Point ed_buffer_clamp_delta(ED_Buffer *buffer, Point point, ED_Delta delta);
static bool ed_buffer_is_in_use(ED_Buffer *buffer);

static ED_Span_I64 ed_relative_span_from_line_and_pos(ED_Line *line, i64 pos);
static ED_Page_I64 ed_relative_from_absolute_line(ED_Buffer *buffer, i64 absolute_line);

static ED_Line *ed_line_from_line_number(ED_Buffer *buffer, i64 line_number);

//- Input processing functions

static ED_Text_Action    ed_text_action_from_key(ED_Key key); // TODO: Replace key with event
static ED_Text_Operation ed_text_operation_from_action(Arena *arena, ED_Buffer *buffer, ED_Text_Action action);

static void ed_buffer_apply_operation(ED_Buffer *buffer, ED_Text_Operation op);

//- Load/save functions

static void ed_init_buffer_contents(ED_Buffer *buffer, SliceU8 contents);
static bool ed_load_file(String file_name);

//- Main rendering functions

static void ed_buffer_update_scroll(ED_Buffer *buffer);
static void ed_render_buffer(ED_Buffer *buffer);

//- Rendering helper functions

static String ed_render_string_from_stored_string(Arena *arena, String stored_string);
static i64 ed_render_x_from_stored_x(String stored_string, i64 stored_x);

//- Editor debug functions

static void ed_validate_buffer(ED_Buffer *buffer);
static bool ed_text_point_exists(ED_Buffer *buffer, Point point);

//- Editor global state functions

static void ed_set_status_message(String message);

//- Editor platform-specific functions

static void before_main(void);
static void enable_raw_mode(void);
static void disable_raw_mode(void);

static void clear(void);
static String get_clear_string(void);

static bool query_window_size(Size *size);
static bool query_cursor_position(Point *position);

static ED_Key wait_for_key(void);

//- Editor global variables

static int exit_code = 0;
static ED_State state;

static FILE *logfile;

#endif
