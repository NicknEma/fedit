#include "fedit_ctx_crack.h"
#include "fedit_base.h"

#include "fedit_base.c"

////////////////////////////////
//~ Editor

#include "fedit.h"

#if OS_WINDOWS
# include "fedit_windows.c"
#elif OS_LINUX
# include "fedit_linux.c"
#endif

// TODO: Review all casts

//- Editor span functions

static ED_Span *
ed_push_span(Arena *arena) {
	ED_Span *span;
	
	span = push_type(arena, ED_Span);
	span->data = push_array(arena, u8, ED_SPAN_SIZE);
	
	return span;
}

static ED_Span *
ed_alloc_span(ED_Buffer *buffer) {
	ED_Span *span = NULL;
	
	if (buffer->first_free_span) {
		span = stack_pop(buffer->first_free_span); // Get from free-list
		u8   *data = span->data;
		memset(data, 0, ED_SPAN_SIZE);
		memset(span, 0, sizeof(ED_Span));
		span->data = data;
	} else {
		span = ed_push_span(&buffer->arena); // Push from arena
	}
	
	return span;
}

static ED_Span *
ed_span_append_text(ED_Buffer *buffer, ED_Line *line, ED_Span *span, String text) {
	i64 appended = 0;
	
	while (appended < text.len) {
		if (span->len == ED_SPAN_SIZE) {
			ED_Span *new_span = ed_alloc_span(buffer);
			dll_insert(line->first_span, line->last_span, span, new_span);
			
			span = new_span;
		}
		
		i64 space   = ED_SPAN_SIZE - span->len;
		i64 to_copy = text.len - appended;
		i64 to_copy_now = min(space, to_copy);
		memcpy(span->data + span->len, text.data + appended, to_copy_now);
		span->len += to_copy_now;
		
		appended  += to_copy_now;
	}
	
	return span;
}

static void
ed_span_insert_text(ED_Buffer *buffer, ED_Line *line, ED_Span *span, i64 at_in_span, String text) {
	Scratch scratch = scratch_begin(0, 0);
	
	// TODO: Consider newlines
	
	// Create a backup of what comes after the cursor
	i64 after_in_span = span->len - at_in_span;
	String temp = string_clone(scratch.arena, string(span->data + at_in_span, after_in_span));
	
	// Pretend the span has more space (truncate at the cursor)
	span->len = at_in_span;
	
	// Append the text, potentially creating new spans
	ED_Span *new_span = ed_span_append_text(buffer, line, span, text);
	
	// Copy the backup back into the line
	ed_span_append_text(buffer, line, new_span, temp);
	
	// Expand newlines
	// ed_buffer_expand_newlines(buffer, );
	
	scratch_end(scratch);
	return;
}

//- Editor line functions

static i64
ed_line_len(ED_Line *line) {
	i64 len = 0;
	ED_Span *span = line->first_span;
	while (span) {
		len += span->len;
		span = span->next;
	}
	return len;
}

static String
string_from_ed_line(Arena *arena, ED_Line *line) {
	i64 len = ed_line_len(line);
	String result = push_string(arena, len);
	i64 at = 0;
	ED_Span *span = line->first_span;
	while (span) {
		memcpy(result.data + at, span->data, span->len);
		at += span->len;
		
		span = span->next;
	}
	return result;
}

static ED_Relative_Span
ed_relative_span_from_line_and_pos(ED_Line *line, i64 pos) {
	ED_Relative_Span result = {0};
	
	ED_Span *span = line->first_span;
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

static ED_Relative_Line
ed_relative_from_absolute_line(i64 absolute_line) {
	ED_Relative_Line result = {0};
	
	i64 line = absolute_line;
	ED_Page *page = state.current_buffer->first_page;
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

static ED_Line *
ed_line_from_line_number(i64 line_number) {
	ED_Relative_Line rel = ed_relative_from_absolute_line(line_number);
	ED_Line *result = &rel.page->lines[rel.line];
	return result;
}

static ED_Line *
ed_get_current_line() {
	return ed_line_from_line_number(state.current_buffer->cursor.y);
}

//- Editor buffer functions

static void
ed_buffer_insert_text_at_cursor(ED_Buffer *buffer, String text) {
	
	// Get all the variables
	ED_Line *current_line = ed_get_current_line();
	ED_Span *current_span = NULL;
	i64 at_in_span = 0;
	
	{
		ED_Relative_Span rel = ed_relative_span_from_line_and_pos(current_line, buffer->cursor.x);
		current_span = rel.span;
		at_in_span   = rel.at;
	}
	
	ED_Span *original_span = current_span; // For debugging only
	(void)original_span;
	
	ed_span_insert_text(buffer, current_line, current_span, at_in_span, text);
	
	buffer->cursor.x += cast(i32) text.len; // TODO: Assumes no newlines
	
	return;
}

#if 0
static void
ed_move_cursor(ED_Key key) {
	ED_Buffer *buffer = state.current_buffer;
	
	(void)buffer;
	(void)key;
	switch (key) {
		case ED_Key_ARROW_LEFT: {
			if (buffer->cursor.x > 0) {
				buffer->cursor.x -= 1;
			} else {
				// Move to end of previous line
				if (buffer->cursor.y > 0) {
					buffer->cursor.y -= 1;
					ED_Line *line = ed_line_from_line_number(buffer->cursor.y);
					buffer->cursor.x = cast(i32) ed_line_len(line);
				}
			}
		} break;
		
		case ED_Key_ARROW_RIGHT: {
			ED_Line *line = ed_line_from_line_number(buffer->cursor.y);
			if (buffer->cursor.x < ed_line_len(line)) {
				buffer->cursor.x += 1;
			} else {
				// Move to start of next line
				if (buffer->cursor.y < buffer->line_count - 1) {
					buffer->cursor.y += 1;
					buffer->cursor.x = 0;
				}
			}
		} break;
		
		case ED_Key_ARROW_UP: {
			if (buffer->cursor.y > 0) {
				buffer->cursor.y -= 1;
			}
		} break;
		
		case ED_Key_ARROW_DOWN: {
			if (buffer->cursor.y < buffer->line_count - 1) {
				buffer->cursor.y += 1;
			}
		} break;
		
		default: {
			panic("Invalid argument 'key'");
		}
	}
	
	ED_Line *line = ed_line_from_line_number(buffer->cursor.y);
	i64 line_len = ed_line_len(line);
	if (buffer->cursor.x > line_len) {
		buffer->cursor.x = cast(i32) line_len;
	}
	
}
#endif

static void
ed_buffer_apply_operation(ED_Buffer *buffer, ED_Text_Operation operation) {
	// Cursor navigation
	buffer->cursor = operation.new_cursor;
	
	// TODO: For now do the separate steps independently. If it's too slow, make them happen together.
	
	// Remove range
	ed_buffer_remove_range(buffer, operation.delete_range);
	
	// Paste replace_string
	ed_buffer_insert_text_at_cursor(buffer, operation.replace_string);
}

static void
ed_buffer_remove_range(ED_Buffer *buffer, Text_Range range) {
	ED_Page *start_page = NULL;
	ED_Line *start_line = NULL;
	i64 start_line_in_page = 0;
	
	{
		ED_Relative_Line rel = ed_relative_from_absolute_line(range.start.y);
		start_page = rel.page;
		start_line = &start_page->lines[rel.line];
		start_line_in_page = rel.line;
	}
	
	// Validate arguments
	assert(range.end.y >= range.start.y && range.end.x >= range.start.x);
	assert(ed_text_point_exists(buffer, range.start));
	assert(ed_text_point_exists(buffer, range.end));
	
	{
		// Delete lines from start downwards
		
		// i64 lines_remaining = ED_PAGE_SIZE - start_line_in_page - 1;
		i64 lines_remaining = start_page->line_count - start_line_in_page - 1;
		i64 line_diff = range.end.y - range.start.y;
		i64 lines_to_delete = min(line_diff - 1, lines_remaining);
		lines_to_delete = max(lines_to_delete, 0);
		
		for (i64 i = 0; i < lines_to_delete; i += 1) {
			i64 line_index = i + start_line_in_page + 1;
			
			ED_Line *line = &start_page->lines[line_index];
			while (line->first_span) {
				ED_Span *span = line->first_span;
				dll_remove(line->first_span, line->last_span, span);
				stack_push(buffer->first_free_span, span);
			}
			
			assert(!line->last_span);
		}
		
		memmove(start_page->lines + start_line_in_page + 1, start_page->lines + start_line_in_page + lines_to_delete + 1,
				(lines_remaining - lines_to_delete) * sizeof(ED_Line));
		start_page->line_count -= lines_to_delete;
		buffer->line_count -= lines_to_delete;
		
		
		
		range.end.y -= cast(i32) lines_to_delete;
	}
	
	ED_Page *end_page = NULL;
	ED_Line *end_line = NULL;
	i64 end_line_in_page = 0;
	
	{
		ED_Relative_Line rel = ed_relative_from_absolute_line(range.end.y);
		end_page = rel.page;
		end_line = &end_page->lines[rel.line];
		end_line_in_page = rel.line;
	}
	
	{
		// Delete lines from end upwards
		
		i64 lines_remaining = end_line_in_page;
		i64 line_diff = range.end.y - range.start.y;
		i64 lines_to_delete = min(line_diff - 1, lines_remaining);
		lines_to_delete = max(lines_to_delete, 0);
		
		for (i64 i = 0; i < lines_to_delete; i += 1) {
			i64 line_index = i + end_line_in_page - lines_to_delete;
			
			ED_Line *line = &start_page->lines[line_index];
			while (line->first_span) {
				ED_Span *span = line->first_span;
				dll_remove(line->first_span, line->last_span, span);
				stack_push(buffer->first_free_span, span);
			}
			
			assert(!line->last_span);
		}
		
		memmove(end_page->lines, end_page->lines + lines_to_delete, sizeof(ED_Line) * lines_to_delete);
		start_page->line_count -= lines_to_delete;
		buffer->line_count -= lines_to_delete;
		
		
		
		range.end.y -= cast(i32) lines_to_delete;
	}
	
	assert((start_line_in_page == start_page->line_count - 1 &&
			end_page->line_count == ED_PAGE_SIZE - end_line_in_page) ||
		   (start_page == end_page));
	
	{
		// TODO: Join lines
		
		ED_Span *start_span = NULL;
		i64 at_in_start_span = 0;
		
		{
			ED_Relative_Span rel = ed_relative_span_from_line_and_pos(start_line, range.start.x);
			start_span = rel.span;
			at_in_start_span = rel.at;
		}
		
		ED_Span *end_span = NULL;
		i64 at_in_end_span = 0;
		
		{
			ED_Relative_Span rel = ed_relative_span_from_line_and_pos(end_line, range.end.x);
			end_span = rel.span;
			at_in_end_span = rel.at;
		}
		
		if (start_span == end_span) {
			assert(at_in_start_span <= at_in_end_span);
			
			i64 to_copy = end_span->len - at_in_end_span;
			// i64 to_copy = (range.end.x - range.start.x);
			memmove(start_span->data + at_in_start_span, end_span->data + at_in_end_span, to_copy);
			start_span->len -= (range.end.x - range.start.x);
			
			if (to_copy > 0) {
				assert(start_span->len == at_in_start_span + to_copy);
			}
		} else {
			
			// Get variables that may have changed
			{
				ED_Relative_Line rel = ed_relative_from_absolute_line(range.end.y);
				end_page = rel.page;
				end_line = &end_page->lines[rel.line];
				end_line_in_page = rel.line;
			}
			
			// 1: Concatenate the 2 lines
			if (range.start.y < range.end.y) {
				assert(range.start.y == range.end.y - 1);
				
				
				start_line->last_span->next = end_line->first_span;
				end_line->first_span->prev = start_line->last_span;
				
				start_line->last_span = end_line->last_span;
				end_line->first_span = start_line->first_span;
				
				
#if 1
				i64 lines_remaining = end_page->line_count - end_line_in_page; // TODO: no -1 here?
				i64 lines_to_delete = 1;
				memmove(end_page->lines + end_line_in_page, end_page->lines + end_line_in_page + 1,
						(lines_remaining - lines_to_delete) * sizeof(ED_Line));
				end_page->line_count -= lines_to_delete;
				buffer->line_count -= lines_to_delete;
#endif
				
			}
			
			// 2: Remove spans in between
			while (start_span->next != end_span) {
				ED_Span *span_to_free = start_span->next;
				
				dll_remove(start_line->first_span, start_line->last_span, span_to_free);
				stack_push(buffer->first_free_span, span_to_free);
			}
			
			// 3: Make sure spans are correct
			
			start_span->len = at_in_start_span;
			
			
			
			// TODO: I see similarities between here and the case in which they're the same span.
			// Maybe they're the same case.
			
			i64 to_copy = end_span->len - at_in_end_span;
			memmove(end_span->data + 0, end_span->data + at_in_end_span, to_copy);
			end_span->len = to_copy;
			
			allow_break();
		}
		
	}
	
	return;
}

//- Editor input processing

static ED_Text_Action
ed_text_action_from_key(ED_Key key) {
	ED_Text_Action action = {0};
	
	action.delta.direction = -1; // To make sure every case sets it.
	
	if (key == ED_Key_ARROW_LEFT  ||
		key == ED_Key_ARROW_RIGHT ||
		key == ED_Key_ARROW_UP    ||
		key == ED_Key_ARROW_DOWN) {
		action.delta.cross_lines = true;
	}
	
	if (key == ED_Key_PAGE_UP ||
		key == ED_Key_PAGE_DOWN) {
		action.delta.clamp_by_window = true;
	}
	
	switch (key) {
		
		// Navigation
		case ED_Key_ARROW_LEFT:  { action.delta.delta = -1; action.delta.direction = Direction_HORIZONTAL; } break;
		case ED_Key_ARROW_RIGHT: { action.delta.delta = +1; action.delta.direction = Direction_HORIZONTAL; } break;
		
		case ED_Key_ARROW_UP:    { action.delta.delta = -1; action.delta.direction = Direction_VERTICAL; } break;
		case ED_Key_ARROW_DOWN:  { action.delta.delta = +1; action.delta.direction = Direction_VERTICAL; } break;
		
		case ED_Key_PAGE_UP:     { action.delta.delta = -1000; action.delta.direction = Direction_VERTICAL; } break;
		case ED_Key_PAGE_DOWN:   { action.delta.delta = +1000; action.delta.direction = Direction_VERTICAL; } break;
		
		case ED_Key_HOME:        { action.delta.delta = -1000; action.delta.direction = Direction_HORIZONTAL; } break; // TODO: I64_MAX
		case ED_Key_END:         { action.delta.delta = +1000; action.delta.direction = Direction_HORIZONTAL; } break; // TODO: I64_MAX
		
		// Deletion
		case ED_Key_BACKSPACE: {
			action.flags |= ED_Text_Action_Flags_DELETE;
			action.delta.delta = -1;
			action.delta.direction = Direction_HORIZONTAL;
		} break;
		
		case CTRL_KEY('h'):
		case ED_Key_DELETE: {
			action.flags |= ED_Text_Action_Flags_DELETE;
			action.delta.delta = +1;
			action.delta.direction = Direction_HORIZONTAL;
		} break;
		
		// Nothing
		case CTRL_KEY('l'):
		case '\x1b': {
			allow_break();
			action.delta.direction = Direction_HORIZONTAL; // Just to set it to something
		} break;
		
		// Insertion
		default: {
			if (isprint(key) || key == '\t' || key == '\n') {
				action.character = cast(u8) key;
				action.delta.direction = Direction_HORIZONTAL;
			}
		} break;
	}
	
	return action;
}

static ED_Text_Operation
ed_text_operation_from_action(Arena *arena, ED_Buffer *buffer, ED_Text_Action action) {
	ED_Text_Operation op = {0};
	
	// Set defaults
	op.new_cursor     = buffer->cursor;
	op.delete_range   = make_text_range(op.new_cursor, op.new_cursor);
	op.replace_string = string_from_lit("");
	
	// Apply delta
	op.new_cursor = ed_buffer_clamp_delta(buffer, op.new_cursor, action.delta);
	
	if (action.flags & ED_Text_Action_Flags_DELETE) {
		// Mark whole region to be deleted
		op.delete_range = make_text_range(buffer->cursor, op.new_cursor);
		
		// Reset the cursor
		op.new_cursor = op.delete_range.start;
	}
	
	// Insert text
	if (action.character != 0) {
		op.replace_string = string_clone(arena, string(&action.character, 1));
	}
	
	return op;
}

//- Editor helper functions

static bool
ed_buffer_is_in_use(ED_Buffer *buffer) {
	return buffer->name.len > 0;
}

static Point
ed_buffer_clamp_delta(ED_Buffer *buffer, Point point, ED_Delta delta) {
	Point result = point;
	
	// TODO: What if the delta is encoded as a point instead of a scalar + direction?
	
	if (delta.delta != 0) { // If this is fast enough we can eliminate this check
		switch (delta.direction) {
			case Direction_HORIZONTAL: {
				result.x += delta.delta;
				
				// @Speed: Store these in their respective structures?
				ED_Line *line = ed_line_from_line_number(result.y);
				i64 len = ed_line_len(line);
				
				// @Cleanup: Is there a way to simplify this codepath? It should be simple...
				if (result.x < 0) {
					if (delta.cross_lines) {
						// Move to end of previous line
						if (result.y > 0) {
							result.y -= 1;
							result.x = cast(i32) ed_line_len(ed_line_from_line_number(result.y)); // Get it again because it changed...
						} else {
							result.x = 0;
						}
					} else {
						result.x = 0;
					}
				} else if (result.x > len) {
					if (delta.cross_lines) {
						// Move to start of next line
						if (result.y < buffer->line_count - 1) {
							result.y += 1;
							result.x = 0;
						} else {
							result.x = cast(i32) len;
						}
					} else {
						result.x = cast(i32) len;
					}
				}
				
			} break;
			
			case Direction_VERTICAL: {
				i32 actual_delta = delta.delta;
				
				if (delta.clamp_by_window) {
					actual_delta = clamp(-state.window_size.height, delta.delta, +state.window_size.height);
				}
				
				result.y += actual_delta;
				result.y = clamp(0, result.y, cast(i32) buffer->line_count - 1);
				
				// @Speed: Store these in their respective structures?
				ED_Line *line = ed_line_from_line_number(result.y);
				i64 len = ed_line_len(line);
				
				if (result.x > len) {
					result.x = cast(i32) len;
				}
				
			} break;
			
			default: {
				result.x = 0;
				result.y = 0;
				
				panic();
			} break;
		}
	}
	
	return result;
}

//- Editor load/save functions

static void
ed_init_buffer_contents(ED_Buffer *buffer, SliceU8 contents) {
	// Assumes that the raw contents encode line breaks as LF.
	
	assert(buffer->arena.ptr);
	
	buffer->cursor.x = 0;
	buffer->cursor.y = 0;
	buffer->vscroll = 0;
	buffer->hscroll = 0;
	buffer->page_count = 0;
	buffer->line_count = 0;
	buffer->first_page = NULL;
	buffer->last_page  = NULL;
	buffer->first_free_page = NULL;
	buffer->first_free_span = NULL;
	
	ED_Page *page = NULL;
	{
		page = push_type(&buffer->arena, ED_Page);
		page->lines = push_array(&buffer->arena, ED_Line, ED_PAGE_SIZE);
		dll_push_back(buffer->first_page, buffer->last_page, page);
		buffer->page_count += 1;
	}
	
	i64 line_start = 0;
	i64 line_end = 0;
	for (i64 byte_index = 0; byte_index <= contents.len; byte_index += 1) {
		if (byte_index == contents.len || contents.data[byte_index] == '\n') {
			line_end = byte_index;
			
			// Get line
			ED_Line *line = NULL;
			{
				if (page->line_count >= ED_PAGE_SIZE) {
					page = push_type(&buffer->arena, ED_Page);
					page->lines = push_array(&buffer->arena, ED_Line, ED_PAGE_SIZE);
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
				// Always allocate from arena here, since the free-list has been cleared
				// at the top of this function
				ED_Span *span = ed_push_span(&buffer->arena);
				dll_push_back(line->first_span, line->last_span, span);
				
				// No need to subtract the length (we just allocated it so it will be 0)
				i64 space   = ED_SPAN_SIZE;
				i64 to_copy = line_len - copied;
				i64 to_copy_now = min(space, to_copy);
				memcpy(span->data, contents.data + line_start + copied, to_copy_now);
				span->len = to_copy_now;
				
				copied   += to_copy_now;
			}
			
			assert(ed_line_len(line) == line_len);
			
			// Prepare for next iteration
			line_start = line_end + 1;
		}
	}
	
	return;
}

static bool
ed_load_file(String file_name) {
	// Overwrite previously loaded file.
	
	bool ok = false;
	
	if (!state.single_buffer) {
		state.single_buffer = push_type(&state.arena, ED_Buffer);
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
		ed_init_buffer_contents(state.single_buffer, read_file_result.contents);
		
		state.single_buffer->file_name = string_clone(&state.single_buffer->arena, file_name);
		state.single_buffer->name      = state.single_buffer->file_name;
		
		ok = true;
	} else {
		;
	}
	
	scratch_end(scratch);
	return ok;
}

//- Editor rendering functions

static void
ed_update_scroll(void) {
	
	ED_Buffer *buffer = state.current_buffer;
	
#if 0
	// Scroll by 2
	
	// Vertical scroll
	if (state.cursor.y < state.vscroll) {
		state.vscroll = state.cursor.y - 1; // Scroll up by 2 lines
		state.vscroll = max(state.vscroll, 0);
	}
	if (state.cursor.y >= state.vscroll + state.window_size.height) {
		state.vscroll = state.cursor.y - state.window_size.height + 2;
		state.vscroll = min(state.vscroll, state.line_count);
	}
	
	// Horizontal scroll
	if (state.cursor.x < state.hscroll) {
		state.hscroll = state.cursor.x - 1;
		state.hscroll = max(state.hscroll, 0);
	}
	if (state.cursor.x >= state.hscroll + state.window_size.width) {
		state.hscroll = state.cursor.x - state.window_size.width + 2;
		state.hscroll = min(state.hscroll, state.window_size.width);
	}
#else
	// Simplified, scroll by 1
	
	// Vertical scroll
	if (buffer->cursor.y < buffer->vscroll) {
		buffer->vscroll = buffer->cursor.y;
	}
	if (buffer->cursor.y >= buffer->vscroll + (state.window_size.height - 2)) {
		buffer->vscroll = buffer->cursor.y - (state.window_size.height - 2) + 1;
	}
	
	Scratch scratch = scratch_begin(0, 0);
	
	ED_Line *line = ed_line_from_line_number(buffer->cursor.y);
	i64 cursor_render_x = ed_render_x_from_stored_x(string_from_ed_line(scratch.arena, line), buffer->cursor.x);
	
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

// Expands tab characters to spaces
static String
ed_render_string_from_stored_string(Arena *arena, String stored_string) {
	int tab_count = 0;
	for (i64 i = 0; i < stored_string.len; i += 1) {
		if (stored_string.data[i] == '\t') {
			tab_count += 1;
		}
	}
	
	String result;
	result.len  = stored_string.len + tab_count*(ED_TAB_WIDTH - 1);
	result.data = push_nozero(arena, result.len * sizeof(u8));
	
	i64 write_index = 0;
	for (i64 read_index = 0; read_index < stored_string.len; read_index += 1) {
		if (stored_string.data[read_index] == '\t') {
			for (i64 i = 0; i < ED_TAB_WIDTH; i += 1) {
				result.data[write_index + i] = ' ';
			}
			write_index += ED_TAB_WIDTH;
		} else {
			result.data[write_index] = stored_string.data[read_index];
			write_index += 1;
		}
	}
	
	return result;
}

static i64
ed_render_x_from_stored_x(String stored_string, i64 stored_x) {
	i64 tab_count = 0;
	for (i64 i = 0; i < stored_x; i += 1) {
		if (stored_string.data[i] == '\t') {
			tab_count += 1;
		}
	}
	
	i64 result = tab_count*ED_TAB_WIDTH + (stored_x - tab_count);
	return result;
}

static void
ed_render_buffer(ED_Buffer *buffer) {
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
		
		ED_Relative_Line rel_line = ed_relative_from_absolute_line(line_number);
		ED_Page *page = rel_line.page;
		i64 line_relative_to_start_of_page = rel_line.line;
		
		int num_rows_to_draw = state.window_size.height - 2; // Subtract the status bar and the status message
		
		for (int y = 0; y < num_rows_to_draw; y += 1) {
			if (page && line_relative_to_start_of_page < page->line_count) {
				{
					// Print line
					ED_Line *line = &page->lines[line_relative_to_start_of_page];
					String render_line = ed_render_string_from_stored_string(scratch.arena, string_from_ed_line(scratch.arena, line));
					
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
	
	ED_Line *current_line = ed_line_from_line_number(buffer->cursor.y);
	i64 cursor_render_x = ed_render_x_from_stored_x(string_from_ed_line(scratch.arena, current_line), buffer->cursor.x);
	
	i32 cursor_y_on_screen = buffer->cursor.y - cast(i32) buffer->vscroll; // TODO: Review this cast
	i32 cursor_x_on_screen = cast(i32) cursor_render_x - cast(i32) buffer->hscroll; // TODO: Review this cast
	snprintf(buf, sizeof(buf), "\x1b[%d;%dH", cursor_y_on_screen + 1, cursor_x_on_screen + 1);
	string_builder_append(&builder, string_from_cstring(buf));
	
	string_builder_append(&builder, esc_show_cursor);
	
	// assert(builder.len == builder.cap);
	write_console_unbuffered(string_from_builder(builder));
	
	scratch_end(scratch);
}

//- Editor debug functions

static bool
ed_text_point_exists(ED_Buffer *buffer, Point point) {
	bool exists = true;
	
	if (point.y < 0 || point.y > buffer->line_count) {
		exists = false;
	}
	
	if (exists) {
		ED_Line *line = ed_line_from_line_number(point.y);
		if (point.x < 0 || point.x > ed_line_len(line)) {
			exists = false;
		}
	}
	
	return exists;
}

static void
ed_validate_buffer(ED_Buffer *buffer) {
	{
		// Check that global line-count is valid
		i64 line_count = 0;
		
		ED_Page *page = buffer->first_page;
		while (page) {
			line_count += page->line_count;
			page = page->next;
		}
		
		assert(line_count == buffer->line_count);
		assert(line_count > 0);
	}
	
	{
		// Check that each line has at least a span
		ED_Page *page = buffer->first_page;
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
		ED_Page *page = buffer->first_page;
		assert(page->line_count > 0);
	}
	
	allow_break();
}

//- Editor global state functions

static void
ed_set_status_message(String message) {
	state.status_message = string_clone_buffer(state.status_message_buffer, sizeof(state.status_message_buffer), message);
	state.status_message_timestamp = time(NULL);
}

//- Entry point

int main(int argc, char **argv) {
	
	before_main();
	enable_raw_mode();
	
	logfile = fopen("log.txt", "w");
	assert(logfile);
	
	{
		// Init state
		arena_init(&state.arena);
		arena_init(&state.frame_arena);
		
		if (!query_window_size(&state.window_size)) {
			panic();
		}
		
		{
			// Init null buffer
			state.null_buffer = push_type(&state.arena, ED_Buffer);
			state.null_buffer->name = string_from_lit("*null*");
			
			// There's no point in creating an arena specifically for this buffer.
			// This is the only buffer that uses the global state's arena for
			// its allocations.
			
			state.null_buffer->first_page = push_type(&state.arena, ED_Page);
			state.null_buffer->last_page = state.null_buffer->first_page;
			state.null_buffer->page_count = 1;
			
			state.null_buffer->first_page->lines = push_array(&state.arena, ED_Line, ED_PAGE_SIZE);
			state.null_buffer->first_page->line_count = 1;
			state.null_buffer->line_count = 1;
			
			state.null_buffer->first_page->lines[0].first_span = push_type(&state.arena, ED_Span);
			state.null_buffer->first_page->lines[0].last_span = state.null_buffer->first_page->lines[0].first_span;
			
			state.null_buffer->first_page->lines[0].first_span->data = cast(u8 *) "~";
			state.null_buffer->first_page->lines[0].first_span->len = 1;
			
			state.null_buffer->is_read_only = true;
		}
		
		state.current_buffer = state.null_buffer;
		
		ed_set_status_message(string_from_lit("Ctrl-Q to quit"));
	}
	
	{
		// Parse command-line args
		if (argc > 1) {
			String file_name = string_from_cstring(argv[1]);
			bool loaded = ed_load_file(file_name);
			
			if (loaded) {
				state.current_buffer = state.single_buffer;
			} else {
				// If the file doesn't exist, simply leave the editor open with no loaded files
				// Display a log message at the bottom or something
				ed_set_status_message(string_from_lit("Failed to load file"));
				
				state.current_buffer = state.null_buffer;
			}
		}
	}
	
	assert(state.current_buffer); // Always!
	
	if (argc > 1) {
		// Temporary
#if 0
		{
			Point start = {3,1};
			Point end = {8,2};
			ed_buffer_remove_range(state.current_buffer, make_text_range(start, end));
		}
#else
		{
			Point start = {3,1};
			Point end = {8,3};
			ed_buffer_remove_range(state.current_buffer, make_text_range(start, end));
		}
#endif
	}
	
	while (true) {
		assert(state.current_buffer); // Always!
		ed_validate_buffer(state.current_buffer); // Always!
		
		ed_update_scroll();
		
		ed_render_buffer(state.current_buffer);
		
		if (!query_window_size(&state.window_size)) {
			panic();
		}
		
		ED_Key key = wait_for_key();
		if (key == CTRL_KEY('q')) {
			clear();
			goto main_loop_end;
		}
		
#if 1
		
		ED_Text_Action action = ed_text_action_from_key(key);
		ED_Text_Operation operation = ed_text_operation_from_action(&state.frame_arena, state.current_buffer, action);
		
		ed_buffer_apply_operation(state.current_buffer, operation);
		
		arena_reset(&state.frame_arena);
		
#else
		switch (key) {
			case ED_Key_ARROW_UP:
			case ED_Key_ARROW_LEFT:
			case ED_Key_ARROW_DOWN:
			case ED_Key_ARROW_RIGHT: {
				ed_move_cursor(key);
			} break;
			
			case ED_Key_PAGE_UP:
			case ED_Key_PAGE_DOWN: {
				ED_Key direction = key == ED_Key_PAGE_UP ? ED_Key_ARROW_UP : ED_Key_ARROW_DOWN;
				for (int step = 0; step < state.window_size.height; step += 1) {
					ed_move_cursor(direction);
				}
			} break;
			
			case ED_Key_HOME: {
				state.current_buffer->cursor.x = 0;
			} break;
			
			case ED_Key_END: {
				ED_Line *current_line = ed_get_current_line();
				state.current_buffer->cursor.x = cast(i32) ed_line_len(current_line);
			} break;
			
			case ED_Key_BACKSPACE:
			case ED_Key_DELETE:
			case CTRL_KEY('h'): {
				ED_Buffer *buffer = state.current_buffer;
				if (!buffer->is_read_only) {
					// Decide start and end coordinates of the deletion
					
					
				}
			} break;
			
			case CTRL_KEY('l'):
			case '\x1b': {
				allow_break();
			} break;
			
			case '\n': {
				//- Split line
				
				ED_Buffer *buffer = state.current_buffer;
				if (!buffer->is_read_only) {
					
					// Get all the variables
					ED_Page *current_page = NULL;
					ED_Line *current_line = NULL;
					i64 line_in_page = 0;
					
					{
						ED_Relative_Line rel = ed_relative_from_absolute_line(buffer->cursor.y);
						current_page = rel.page;
						current_line = &current_page->lines[rel.line];
						line_in_page = rel.line;
					}
					
					ED_Span *current_span = NULL;
					i64 at_in_span = 0;
					
					{
						ED_Relative_Span rel = ed_relative_span_from_line_and_pos(current_line, buffer->cursor.x);
						current_span = rel.span;
						at_in_span   = rel.at;
					}
					
					bool insert_before = false;
					if (current_page->line_count == ED_PAGE_SIZE) {
						// Allocate new page, to be placed after this one
						ED_Page *new_page = NULL;
						if (buffer->first_free_page) {
							// Get from free-list
							new_page = stack_pop(buffer->first_free_page);
							new_page->next = NULL;
							new_page->prev = NULL;
							new_page->line_count = 0;
						} else {
							// Push new page
							new_page = push_type(&buffer->arena, ED_Page);
							new_page->lines = push_array(&buffer->arena, ED_Line, ED_PAGE_SIZE);
						}
						
						// Insert page
						dll_insert(buffer->first_page, buffer->last_page, current_page, new_page);
						
						// Copy last line into new page
						new_page->lines[0] = current_page->lines[ED_PAGE_SIZE - 1];
						new_page->line_count += 1;
						
						current_page->line_count -= 1;
					} else {
						assert(line_in_page <= ED_PAGE_SIZE);
					}
					
					// Shift all the lines of this page
					i64 lines_after_cursor = ED_PAGE_SIZE - line_in_page - 1;
					memmove(current_page->lines + line_in_page + 1, current_page->lines + line_in_page, lines_after_cursor * sizeof(ED_Line));
					
					ED_Page *original_page = current_page; // For debugging only
					(void)original_page;
					
					// Fix for when the cursor is exactly on the last line of a page:
					// Undo some of the changes and pretend that the new page is the current page
					//
					// Feels like a @Hack but it works... ? I don't really like how I added
					// 'insert_before' but it seems like the only way.
					if (line_in_page == ED_PAGE_SIZE - 1) {
						current_page->line_count += 1;
						current_page = current_page->next;
						current_page->line_count -= 1;
						memset(&current_page->lines[0], 0, sizeof(ED_Line));
						line_in_page = 0;
						insert_before = true;
					}
					
					// Alloc new span
					ED_Span *new_span = NULL;
					{
						if (buffer->first_free_span) {
							// Get from free-list
							new_span = stack_pop(buffer->first_free_span);
							memset(new_span->data, 0, ED_SPAN_SIZE);
							new_span->next = NULL;
							new_span->len  = 0;
						} else {
							// Push new span
							new_span = push_type(&buffer->arena, ED_Span);
							new_span->data = push_array(&buffer->arena, u8, ED_SPAN_SIZE);
						}
					}
					
					// Copy chars after cursor to new span and truncate the current span
					i64 chars_after_cursor = current_span->len - at_in_span;
					memcpy(new_span->data, current_span->data + at_in_span, chars_after_cursor);
					new_span->len = chars_after_cursor;
					
					// Truncate current line
					current_span->next = NULL;
					current_span->len  = at_in_span;
					
					// Insert new span into the current page
					if (insert_before) {
						current_page->lines[line_in_page+0].first_span = new_span;
					} else {
						current_page->lines[line_in_page+1].first_span = new_span;
					}
					current_page->line_count += 1;
					buffer->line_count += 1;
					
					// Reposition cursor
					ed_move_cursor(ED_Key_ARROW_DOWN);
					state.current_buffer->cursor.x = 0; // Copy the behaviour of the HOME key
				}
			} break;
			
			default: {
				ED_Buffer *buffer = state.current_buffer;
				if ((isprint(key) || key == '\t') && !buffer->is_read_only) {
					u8 c = cast(u8) key;
					String text = string(&c, 1);
					
					ed_buffer_insert_text_at_cursor(buffer, text);
					
					// Reposition cursor
					for (i64 i = 0; i < text.len; i += 1) {
						ed_move_cursor(ED_Key_ARROW_RIGHT);
					}
					
					allow_break();
				}
			} break;
		}
#endif
	}
	
	main_loop_end:;
	
	fclose(logfile);
	
	disable_raw_mode();
	return exit_code;
}
