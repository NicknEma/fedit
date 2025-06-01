#ifndef FEDIT_BASE_C
#define FEDIT_BASE_C

#if OS_WINDOWS
# include "fedit_base_windows.c"
#elif OS_LINUX
# include "fedit_base_linux.c"
#endif

/////////////////
//~ fsize

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
//~ Core

//- Basic types functions

static Text_Range
make_text_range(Point start, Point end) {
	Text_Range result = {
		.start = start,
		.end   = end,
	};
	return result;
}

//- Integer math

static bool
is_power_of_two(u64 i) {
	return i > 0 && (i & (i-1)) == 0;
}

static u64
align_forward(u64 ptr, u64 alignment) {
	assert(is_power_of_two(alignment));
	return (ptr + alignment-1) & ~(alignment-1);
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
//~ Arena

//- Arena operations: constructors/destructors

static void
arena_init(Arena *arena) {
	arena_init_size(arena, DEFAULT_ARENA_RESERVE_SIZE);
}

static void
arena_init_size(Arena *arena, u64 reserve_size) {
	u8 *base = mem_reserve(reserve_size);
	assert(base != NULL);
	
	arena->ptr  = base;
	arena->cap  = reserve_size;
	arena->pos  = 0;
	arena->peak = 0;
	arena->commit_pos = 0;
}

static bool
arena_fini(Arena *arena) {
	bool result = mem_release(arena->ptr, arena->cap);
	memset(arena, 0, sizeof(Arena));
	return result;
}

static void
arena_reset(Arena *arena) {
	pop_to(arena, 0);
}

//- Arena operations: push

static void *
push_nozero_aligned(Arena *arena, u64 size, u64 alignment) {
	void *result = NULL;
	
	if (size > 0) {
		u64 align_pos = align_forward(arena->pos, alignment);
		if (align_pos + size <= arena->cap) {
			arena->pos = align_pos;
			
			result = arena->ptr + arena->pos;
			arena->pos += size;
			
			if (arena->pos > arena->commit_pos) {
				u64 new_commit_pos = clamp_top(align_forward(arena->pos, ARENA_COMMIT_GRANULARITY), arena->cap);
				
				void *commit_base = arena->ptr + arena->commit_pos;
				u64   commit_size = new_commit_pos - arena->commit_pos;
				
				(void)mem_commit(commit_base, commit_size);
				arena->commit_pos = new_commit_pos;
			}
			
			arena->peak = max(arena->pos, arena->peak);
		} else {
			panic("Arena is out of memory.");
		}
	}
	
	return result;
}

static void *
push_zero_aligned(Arena *arena, u64 size, u64 alignment) {
	void *result = push_nozero_aligned(arena, size, alignment);
	memset(result, 0, size);
	return result;
}

#define push_nozero(arena, size) push_nozero_aligned(arena, size, sizeof(u8))
#define push_zero(arena, size)   push_zero_aligned(arena, size, sizeof(u8))

#define push_aligned(arena, size, alignment) push_zero_aligned(arena, size, alignment)
#define push(arena, size)                    push_zero(arena, size)

#define push_type(arena, type)       cast(type *) push_aligned(arena, sizeof(type), alignof(type))
#define push_array(arena, type, len) cast(type *) push_aligned(arena, (len)*sizeof(type), alignof(type))

//- Arena operations: pop

static void
pop_to(Arena *arena, u64 pos) {
	arena->pos = clamp_top(pos, arena->pos); // Prevent user from going forward, only go backward.
	
	u64 pos_aligned_to_commit_chunks = clamp_top(align_forward(arena->pos, ARENA_COMMIT_GRANULARITY), arena->cap);
	
	if (pos_aligned_to_commit_chunks + ARENA_DECOMMIT_THRESHOLD <= arena->commit_pos) {
		u64   decommit_size = arena->commit_pos - pos_aligned_to_commit_chunks;
		void *decommit_base = arena->ptr + pos_aligned_to_commit_chunks;
		
		mem_decommit(decommit_base, decommit_size);
		arena->commit_pos = pos_aligned_to_commit_chunks;
	}
}

static void
pop_amount(Arena *arena, u64 amount) {
	u64 amount_clamped = clamp_top(amount, arena->pos); // Prevent user from going to negative positions
	pop_to(arena, arena->pos - amount_clamped);
}

#define pop(arena, amount) pop_amount(arena, amount)

//- Arena operations: Temp

static Arena_Restore_Point
arena_begin_temp_region(Arena *arena) {
	Arena_Restore_Point point = {arena, arena->pos};
	return point;
}

static void
arena_end_temp_region(Arena_Restore_Point point) {
	pop_to(point.arena, point.pos);
}

////////////////////////////////
//~ Scratch Memory

static Scratch
scratch_begin(Arena **conflicts, i64 conflict_count) {
	Scratch scratch = {0};
	
#if SCRATCH_ARENA_COUNT > 0
	if (scratch_arenas[0].ptr == NULL) { // unlikely()
		for (int i = 0; i < array_count(scratch_arenas); i += 1) {
			arena_init_size(&scratch_arenas[i], SCRATCH_ARENA_RESERVE_SIZE);
		}
	}
#endif
	
	for (i64 scratch_arena_index = 0; scratch_arena_index < array_count(scratch_arenas); scratch_arena_index += 1) {
		bool is_conflicting = false;
		for (i64 conflict_index = 0; conflict_index < conflict_count; conflict_index += 1) {
			if (conflicts[conflict_index] == &scratch_arenas[scratch_arena_index]) {
				is_conflicting = true;
				break;
			}
		}
		
		if (!is_conflicting) {
			scratch = arena_begin_temp_region(&scratch_arenas[scratch_arena_index]);
			break;
		}
	}
	
	return scratch;
}

static void
scratch_end(Scratch scratch) {
	arena_end_temp_region(scratch);
}

////////////////////////////////
//~ Strings and slices

//- Slice functions

static SliceU8
make_sliceu8(u8 *data, i64 len) {
	SliceU8 result = {
		.data = data,
		.len  = len,
	};
	return result;
}

static SliceU8
push_sliceu8(Arena *arena, i64 len) {
	SliceU8 result = {
		.data = push(arena, cast(u64) len),
		.len  = len,
	};
	return result;
}

static SliceU8
sliceu8_from_string(String s) {
	return make_sliceu8(s.data, s.len);
}

static SliceU8
sliceu8_clone(Arena *arena, SliceU8 s) {
	// We don't call push_sliceu8() because that clears memory to 0 and we don't need that here.
	SliceU8 result = {
		.data = push_nozero(arena, s.len),
		.len  = s.len,
	};
	memcpy(result.data, s.data, s.len);
	return result;
}

//- String functions

static String
string(u8 *data, i64 len) {
	String result = {
		.data = data,
		.len  = len,
	};
	return result;
}

static String
push_string(Arena *arena, i64 len) {
	String result = {
		.data = push(arena, cast(u64) len),
		.len  = len,
	};
	return result;
}

static String
push_stringf(Arena *arena, char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	String result = push_stringf_va_list(arena, fmt, args);
	va_end(args);
	return result;
}

static String
push_stringf_va_list(Arena *arena, char *fmt, va_list args) {
	i64 len = vsnprintf(0, 0, fmt, args);
	String result = {
		.data = push_nozero(arena, sizeof(u8) * len),
		.len  = len,
	};
	vsnprintf(cast(char *) result.data, result.len, fmt, args);
	return result;
}

static String
string_from_sliceu8(SliceU8 s) {
	return string(s.data, s.len);
}

static String
string_clone(Arena *arena, String s) {
	// We don't call push_string() because that clears memory to 0 and we don't need that here.
	String result = {
		.data = push_nozero(arena, s.len),
		.len  = s.len,
	};
	memcpy(result.data, s.data, s.len);
	return result;
}

static String
string_clone_buffer(u8 *buffer, i64 buffer_len, String s) {
	memcpy(buffer, s.data, min(buffer_len, s.len));
	return string(buffer, min(buffer_len, s.len));
}

static char *
cstring_from_string(Arena *arena, String s) {
	char *result = push_nozero(arena, (s.len + 1) * sizeof(char));
	memcpy(result, s.data, s.len);
	result[s.len] = 0;
	return result;
}

////////////////////////////////
//~ String Builder

static void
string_builder_init(String_Builder *builder, SliceU8 backing) {
	builder->data = backing.data;
	builder->cap  = backing.len;
	builder->len  = 0;
}

static i64
string_builder_append(String_Builder *builder, String s) {
	assert(builder->data); // Not initialized
	
	i64 space = builder->cap - builder->len;
	i64 to_copy = min(space, s.len);
	memcpy(builder->data + builder->len, s.data, to_copy);
	builder->len += to_copy;
	
	return to_copy;
}

static String
string_from_builder(String_Builder builder) {
	return string(builder.data, builder.len);
}

////////////////////////////////
//~ File IO

static Read_File_Result
read_file(Arena *arena, String file_name) {
	Read_File_Result result = {0};
	
	Scratch scratch = scratch_begin(&arena, 1);
	char *file_name_null_terminated = cstring_from_string(scratch.arena, file_name);
	
	FILE *handle = fopen(file_name_null_terminated, "rb");
	if (handle) {
		errno = 0;
		size_t size = fsize(handle);
		if (errno == 0) {
			result.contents.len  = size;
			result.contents.data = push_nozero(arena, size * sizeof(u8));
			
			if (size > 0) {
				assert(result.contents.data); // TODO: Switch to an if-else?
			}
			
			i64 read_amount  = fread(result.contents.data,
									 sizeof(u8),
									 result.contents.len,
									 handle);
			if (read_amount == result.contents.len || feof(handle)) {
				result.ok = true;
			} else {
				// TODO: something. Write to a log queue? Then the caller can read it if necessary
			}
		} else {
			// TODO: something. Write to a log queue? Then the caller can read it if necessary
		}
		fclose(handle);
	} else {
		// TODO: something. Write to a log queue? Then the caller can read it if necessary
	}
	
	scratch_end(scratch);
	
	return result;
}

#endif
