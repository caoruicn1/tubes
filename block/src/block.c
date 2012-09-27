
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <signal.h>
#include <execinfo.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <inttypes.h>

#include "block.h"
#include "std_helpers.h" // includes C source actually

static char ** running_command = NULL;
void block_segfault_handler(int sig)
{
  fprintf(stderr, "\nSIGSEGV (segfault) in : ");
  if (running_command != NULL)
  {
    int i = 0;
    while (running_command[i] != NULL)
      fprintf(stderr, "%s ", running_command[i++]);
  }
  fprintf(stderr, "\n");
  
  void * array[32];
  size_t size = backtrace(array, 32);
  
  char ** strings = backtrace_symbols(array, size);
  
  int i;
  for (i = 0 ; i < size ; i++) fprintf(stderr, "%s\n", strings[i]);
  
  fprintf(stderr, "\n\n");
  exit(EXIT_FAILURE);
}

void setup_segfault_handling(char ** command)
{
  running_command = command;
  signal(SIGSEGV, block_segfault_handler);
}

struct Block * new_block()
{
  if (sizeof(struct Block) != SIZEOF_STRUCT_BLOCK) { fprintf(stderr, "sizeof(struct Block) is the wrong size (%ld) - should be %ld, perhaps padding or memory alignment works differently for your machine?\n", sizeof(struct Block), SIZEOF_STRUCT_BLOCK); exit(1); }
  
  struct Block * block = (struct Block*)malloc(sizeof(struct Block));
  memset(block, 0, sizeof(struct Block));
  block->version = TUBE_BLOCK_VERSION;
  return block;
}

struct Block * realloc_block(struct Block * block)
{
  return (struct Block *)realloc(block, sizeof(struct Block) + block->attributes_bsize + block->columns_bsize + block->data_bsize);
}

struct Block * set_num_rows(struct Block * block, uint32_t num_rows)
{
  block->num_rows = num_rows;
  block->data_bsize = block->row_bsize * block->num_rows;
  block = realloc_block(block);
  return block;
}

struct Block * read_block(FILE * fp)
{
  if (fp == NULL) { fprintf(stderr, "trying to read from NULL file pointer\n"); return 0; }
  
  float header;
  if (fread(&header, sizeof(header), 1, fp) != 1) return NULL;
  if (isfinite(header)) { fprintf(stderr, "header (%f) is finite (it's suppose to be infinite, either no data or invalid data was read)\n", header); return NULL; }
  
  uint32_t is_block;
  if (fread(&is_block, sizeof(is_block), 1, fp) != 1) { fprintf(stderr, "fread block is_block failed.\n"); return NULL; }
  if (is_block != 1) { fprintf(stderr, "is_block != 1\n"); return NULL; }
  
  //int32_t block_version;
  //if (fread(&block_version, sizeof(block_version), 1, fp) != 1) { fprintf(stderr, "fread block block_version failed.\n"); return NULL; }
  
  struct Block * block = (struct Block*)malloc(sizeof(struct Block));
  if (fread(block, sizeof(struct Block), 1, fp) != 1) { fprintf(stderr, "fread block failed (error 1)\n"); return NULL; }
  if (block->version != TUBE_BLOCK_VERSION) { fprintf(stderr, "fread block is %d (compiled source is for version %d)\n", block->version, TUBE_BLOCK_VERSION); return NULL; }
  block = realloc_block(block);
  
  if (fread(&block[1], block->attributes_bsize + block->columns_bsize + block->data_bsize, 1, fp) != 1) { fprintf(stderr, "fread block failed (error 2)\n"); return NULL; }
  
  return block;
}

void write_block(FILE * fp, struct Block * block)
{
  if (block == NULL) { fprintf(stderr, "write_block passed null block.\n"); return; }
  
  float inf = INFINITY;
  if (fwrite(&inf, sizeof(inf), 1, fp) != 1) { fprintf(stderr, "fwrite block header failed.\n"); return; }
  
  uint32_t is_block = 1;
  if (fwrite(&is_block, sizeof(is_block), 1, fp) != 1) { fprintf(stderr, "fwrite block is_block failed.\n"); return; }
  
  if (fwrite(block, sizeof(struct Block) + block->attributes_bsize + block->columns_bsize + block->data_bsize, 1, fp) != 1) { fprintf(stderr, "fwrite block failed.\n"); return; }
}

void free_block(struct Block * block)
{
  free(block);
}

struct Block * add_command(struct Block * block, int argc, char ** argv)
{
  if (block == NULL || argc == 0 || argv == NULL) return block;
  
  int length = 100;
  char * full_command = (char*)malloc(length);
  sprintf(full_command, "");
  int i = 0;
  for (i = 0 ; i < argc ; i++)
  {
    if (strlen(full_command) + strlen(argv[i]) + 2 > length) {
      length += 100;
      full_command = (char*)realloc(full_command, length);
    }
    strcat(full_command, argv[i]);
    if (i < argc-1) strcat(full_command, " ");
  }
  block = add_string_attribute(block, "command", full_command);
  free(full_command);
  return block;
}

uint32_t memory_pad(uint32_t i, uint32_t bsize)
{
  return ceil(i / (float)bsize) * bsize;
}

/*int32_t get_type_size(int32_t type)
{
  if (type == TYPE.INT)               return sizeof(int32_t);
  else if (type == TYPE.LONG)         return sizeof(long);
  else if (type == TYPE.FLOAT)        return sizeof(float);
  else if (type == TYPE.DOUBLE)       return sizeof(double);
  else if (type >= MIN_STRING_LENGTH) return memory_pad(type+1);
  else fprintf(stderr, "column_get_cell_size invalid type '%d'\n", type);
  return 0;
}*/

////////////////////////////////////////////////////////////////////////////////////////////////////////////

int32_t * get_attribute_offsets(struct Block * block) {
  return (int32_t*)((char*)block + sizeof(struct Block));
}

struct Attribute * get_first_attribute(struct Block * block) {
  return (block == NULL) ? NULL : (struct Attribute *)((char*)block + sizeof(struct Block) + sizeof(int32_t)*block->num_attributes);
}

struct Attribute * get_next_attribute(struct Block * block, struct Attribute * attribute) {
  return (block == NULL || attribute == NULL) ? NULL : (struct Attribute *)((char*)attribute + sizeof(struct Attribute) + attribute->name_length + attribute->value_length);
}

struct Attribute * get_attribute(struct Block * block, uint32_t attribute_id)
{
  if (block == NULL || attribute_id < 0 || attribute_id > block->num_attributes)
  {
    fprintf(stderr, "get_attribute(%ld, %d) FAILED\n", (long int)block, attribute_id);
    return NULL;
  }
  
  int32_t * attribute_offsets = get_attribute_offsets(block);
  return (struct Attribute*)((char*)block + sizeof(struct Block) + attribute_offsets[attribute_id]);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline int32_t * get_column_offsets(const struct Block * block) {
  return (int32_t*)((char*)block + sizeof(struct Block) + block->attributes_bsize);
}

inline int32_t * get_cell_offsets(const struct Block * block) {
  return (int32_t*)((char*)block + sizeof(struct Block) + block->attributes_bsize + sizeof(int32_t)*block->num_columns);
}

inline struct Column * get_first_column(const struct Block * block) {
  return (block == NULL) ? NULL : (struct Column *)((char*)block + sizeof(struct Block) + block->attributes_bsize + (sizeof(int32_t)+sizeof(int32_t))*block->num_columns);
}

inline struct Column * get_next_column(const struct Block * block, const struct Column * column) {
  if (block == NULL || column == NULL) return NULL;
  if (column >= get_first_column(block) + block->columns_bsize) return NULL;
  return (struct Column *)((char*)column + sizeof(struct Column) + column->name_length);
}

struct Column * get_column(struct Block * block, uint32_t column_id)
{
  if (block == NULL || column_id < 0 || column_id > block->num_columns)
  {
    fprintf(stderr, "get_column(%ld, %d) FAILED\n", (long int)block, column_id);
    return NULL;
  }
  
  int32_t * column_offsets = get_column_offsets(block);
  return (struct Column*)((char*)block + sizeof(struct Block) + block->attributes_bsize + column_offsets[column_id]);
  
  int i = 0;
  struct Column * column = get_first_column(block);
  for (i = 0 ; i != column_id ; i++)
    column = get_next_column(block, column);
  
  return column;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void * get_row(struct Block * block, uint32_t row_id)
{
  return (void*)((char*)&block[1] + block->attributes_bsize + block->columns_bsize + block->row_bsize * row_id);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

uint32_t get_row_bsize_from_columns(struct Block * block)
{
  uint32_t single_row_bsize = 0;
  uint32_t column_id;
  for (column_id = 0 ; column_id < block->num_columns ; column_id++)
  {
    struct Column * column = get_column(block, column_id);
    single_row_bsize += column->bsize;
  }
  
  return single_row_bsize;
}

int32_t get_column_id_by_name(struct Block * block, char * column_name)
{
  if (block == NULL || column_name == NULL) return -1;
  uint32_t column_id;
  for (column_id = 0 ; column_id < block->num_columns ; column_id++)
  {
    struct Column * column = get_column(block, column_id);
    if (strcmp(column_name, column_get_name(column))==0) return column_id;
  }
  return -1;
}

int32_t get_column_id_by_name_or_exit(struct Block * block, char * column_name)
{
  int32_t id = get_column_id_by_name(block, column_name);
  if (id == -1)
  {
    fprintf(stderr, "get_column_id_by_name_or_exit failed on column '%s'\n", column_name);
    exit(EXIT_FAILURE);
  }
  return id;
}

struct Column * get_column_by_name(struct Block * block, char * column_name)
{
  return get_column(block, get_column_id_by_name(block, column_name));
}

int32_t get_attribute_id_by_name(struct Block * block, char * attribute_name)
{
  if (block == NULL || attribute_name == NULL) return -1;
  int32_t attribute_id;
  for (attribute_id = 0 ; attribute_id < block->num_attributes ; attribute_id++)
  {
    struct Attribute * attribute = get_attribute(block, attribute_id);
    if (strcmp(attribute_name, attribute_get_name(attribute))==0) return attribute_id;
  }
  return -1;
}

struct Attribute * get_attribute_by_name(struct Block * block, char * attribute_name)
{
  return get_attribute(block, get_attribute_id_by_name(block, attribute_name));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

char * attribute_get_name(struct Attribute * attribute)  { return (char*)attribute + sizeof(struct Attribute); }
void * attribute_get_value(struct Attribute * attribute) { return (char*)attribute + sizeof(struct Attribute) + attribute->name_length; }
void attribute_set_name(struct Attribute * attribute, char * name)   { strncpy((char*)attribute + sizeof(struct Attribute), name, attribute->name_length); }
void attribute_set_value(struct Attribute * attribute, void * value)
{
  if (attribute->type == TYPE_CHAR) strncpy((char*)attribute + sizeof(struct Attribute) + attribute->name_length, (char*)value, attribute->value_length);
  else memcpy((char*)attribute + sizeof(struct Attribute) + attribute->name_length, value, attribute->value_length);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Block * _add_attribute(struct Block * block, enum TYPE type, uint32_t value_length, char * name, void * value)
{
  if (block == NULL) { fprintf(stderr, "add_attribute called on a NULL block\n"); return NULL; }
  
  if (value_length != memory_pad(value_length, 4)) { fprintf(stderr, "warning: value_length for _add_attribute wasn't memory_pad'ed\n"); }
  
  int32_t name_length = memory_pad(strlen(name)+1, 4);
  //value_length = memory_pad(value_length+((type==TYPE_CHAR)?1:0), 4);
  
  int32_t old_attributes_bsize = block->attributes_bsize;
  int32_t single_attribute_bsize = sizeof(struct Attribute) + name_length + value_length;
  
  block->attributes_bsize += 
              single_attribute_bsize + 
              sizeof(int32_t); // attributes_offsets
  block->num_attributes ++;
  block = realloc_block(block);
  
  // move content after attributes, making room for the additional attribute
  if (block->num_columns > 0 || block->num_rows > 0)
  {
    memmove((char*)block + sizeof(struct Block) + old_attributes_bsize + single_attribute_bsize + sizeof(int32_t),
            (char*)block + sizeof(struct Block) + old_attributes_bsize,
            block->columns_bsize + block->data_bsize);
  }
  
  // shift attributes over so there is room for the attribute_offset
  if (old_attributes_bsize > 0)
  {
    memmove((char*)block + sizeof(struct Block) + sizeof(int32_t),
            (char*)block + sizeof(struct Block),
            old_attributes_bsize);
  }
  
  int32_t * attribute_offsets = get_attribute_offsets(block);
  
  int i;
  struct Attribute * attribute = get_first_attribute(block);
  for (i = 0 ; i < block->num_attributes - 1 ; i++)
  {
    attribute_offsets[i] = (int32_t)((char*)attribute - (char*)block - sizeof(struct Block));
    attribute = get_next_attribute(block, attribute);
  }
  
  attribute->type = type;
  attribute->name_length = name_length;
  attribute->value_length = value_length;
  attribute_set_name(attribute, name);
  attribute_set_value(attribute, value);
  attribute_offsets[block->num_attributes - 1] = (int32_t)((char*)attribute - (char*)block - sizeof(struct Block));
  
  return block;
}

struct Block * add_int32_attribute(struct Block * block, char * name, int32_t value) { return _add_attribute(block, TYPE_INT, sizeof(int32_t), name, &value); }
struct Block * add_int64_attribute(struct Block * block, char * name, int64_t value) { return _add_attribute(block, TYPE_INT, sizeof(int64_t), name, &value); }
struct Block * add_float_attribute(struct Block * block, char * name, float value)   { return _add_attribute(block, TYPE_FLOAT, sizeof(float), name, &value); }
struct Block * add_double_attribute(struct Block * block, char * name, double value) { return _add_attribute(block, TYPE_FLOAT, sizeof(double), name, &value); }
struct Block * add_string_attribute(struct Block * block, char * name, char * value) { return _add_attribute(block, TYPE_CHAR, memory_pad(strlen(value)+1, 4), name, value); }

struct Block * copy_all_attributes(struct Block * block, struct Block * src)
{
  int i;
  for (i = 0 ; i < src->num_attributes ; i++)
  {
    struct Attribute * attr = get_attribute(src, i);
    block = _add_attribute(block, attr->type, attr->value_length, attribute_get_name(attr), attribute_get_value(attr));
  }
  return block;
}

void fprintf_attribute_value(FILE * fp, struct Block * block, uint32_t attribute_id)
{
  struct Attribute * attr = get_attribute(block, attribute_id);
  switch (attr->type) {
    case TYPE_INT:
      if      (attr->value_length == 4) { fprintf(fp, "%d", *(int32_t*)attribute_get_value(attr)); break; }
      else if (attr->value_length == 8) { fprintf(fp, "%lld", *(int64_t*)attribute_get_value(attr)); break; }
      else { fprintf(stderr, "bad %s %s:(%d)\n", __func__, __FILE__, __LINE__); break; }
    case TYPE_FLOAT:
      if      (attr->value_length == 4) { fprintf(fp, "%f", *(float*)attribute_get_value(attr)); break; }
      else if (attr->value_length == 8) { fprintf(fp, "%lf", *(double*)attribute_get_value(attr)); break; }
      else { fprintf(stderr, "bad %s %s:(%d)\n", __func__, __FILE__, __LINE__); break; }
    case TYPE_CHAR:
      fprintf(fp, "%s", (char*)attribute_get_value(attr)); break;
    default:
      fprintf(stderr, "bad %s %s:(%d)\n", __func__, __FILE__, __LINE__); break;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

char * column_get_name(struct Column * column)  { return (char*)column + sizeof(struct Column); }
void column_set_name(struct Column * column, char * name) { strncpy(column_get_name(column), name, column->name_length); }

////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Block * _add_column(struct Block * block, enum TYPE type, uint32_t bsize, char * name)
{
  if (block == NULL) { fprintf(stderr, "add_field called on a NULL block\n"); return NULL; }
  
  int32_t name_length = memory_pad(strlen(name)+1, 4);
  int32_t old_columns_bsize = block->columns_bsize;
  int32_t old_data_bsize = block->data_bsize;
  int32_t old_row_bsize = block->row_bsize;
  
  if (bsize != memory_pad(bsize, 4)) fprintf(stderr, "warning: _add_column called with bsize which isn't memory_pad'ed\n");
  
  //if (type == TYPE_CHAR) bsize = memory_pad(bsize+1, 4);
  block->row_bsize += bsize;
  
  int32_t single_column_bsize = sizeof(struct Column) + name_length;
  
  block->columns_bsize += 
              single_column_bsize + 
              sizeof(int32_t) + // column_offsets
              sizeof(int32_t); // cell_offsets
  block->num_columns++;
  block = set_num_rows(block, block->num_rows);
  block = realloc_block(block);
  
  // kbfu - interpolation of all rows of data, not easy
  if (block->num_rows > 0)
  {
    int row_id;
    for (row_id = block->num_rows - 1 ; row_id >= 0 ; row_id--)
    {
      memmove(
        (char*)block + sizeof(struct Block) + block->attributes_bsize + block->columns_bsize + block->row_bsize*row_id,
        (char*)block + sizeof(struct Block) + block->attributes_bsize + old_columns_bsize + old_row_bsize*row_id,
        old_row_bsize);
    }
  }
  
  // shift attributes over so there is room for the attribute_offsets and cell_offsets
  if (old_columns_bsize > 0)
  {
    memmove((char*)block + sizeof(struct Block) + block->attributes_bsize + sizeof(int32_t) + sizeof(int32_t),
            (char*)block + sizeof(struct Block) + block->attributes_bsize,
            old_columns_bsize);
  }
  
  int32_t * column_offsets = get_column_offsets(block);
  int32_t * cell_offsets = get_cell_offsets(block);
  
  int o = 0;
  int i;
  struct Column * column = get_first_column(block);
  for (i = 0 ; i < block->num_columns - 1 ; i++)
  {
    //fprintf(stderr, "%d: %d (add %d)\n", i, o, get_type_size(column->type));
    cell_offsets[i] = o;
    o += column->bsize;//get_type_size(column->type);
    column_offsets[i] = (int32_t)((char*)column - (char*)block - sizeof(struct Block) - block->attributes_bsize);
    column = get_next_column(block, column);
  }
  
  column->type = type;
  column->bsize = bsize;
  column->name_length = name_length;
  column_set_name(column, name);
  column_offsets[block->num_columns-1] = (int32_t)((char*)column - (char*)block - sizeof(struct Block) - block->attributes_bsize);
  //if (block->num_columns != 1)
  cell_offsets[block->num_columns-1] = o;
  
  return block;
}

struct Block * add_int32_column(struct Block * block, char * name)  { return _add_column(block, TYPE_INT, sizeof(int32_t), name); }
struct Block * add_int64_column(struct Block * block, char * name)  { return _add_column(block, TYPE_INT, sizeof(int64_t), name); }
struct Block * add_float_column(struct Block * block, char * name)  { return _add_column(block, TYPE_FLOAT, sizeof(float), name); }
struct Block * add_double_column(struct Block * block, char * name) { return _add_column(block, TYPE_FLOAT, sizeof(double), name); }
struct Block * add_string_column_with_length(struct Block * block, char * name, uint32_t length) { return _add_column(block, TYPE_CHAR, memory_pad(length+1,4), name); }

struct Block * add_int32_column_and_blank(struct Block * block, char * name)  { block = add_int32_column(block, name); blank_column_values(block, name); return block; }
struct Block * add_int64_column_and_blank(struct Block * block, char * name)  { block = add_int64_column(block, name); blank_column_values(block, name); return block; }
struct Block * add_float_column_and_blank(struct Block * block, char * name)  { block = add_float_column(block, name); blank_column_values(block, name); return block; }
struct Block * add_double_column_and_blank(struct Block * block, char * name) { block = add_double_column(block, name); blank_column_values(block, name); return block; }
struct Block * add_string_column_with_length_and_blank(struct Block * block, char * name, uint32_t length) { block = add_string_column_with_length(block, name, length); blank_column_values(block, name); return block; }

struct Block * add_xy_columns(struct Block * block)
{
  if (get_column_id_by_name(block, "x") == -1) block = add_float_column(block, "x");
  if (get_column_id_by_name(block, "y") == -1) block = add_float_column(block, "y");
  return block;
}

struct Block * add_xyz_columns(struct Block * block)
{
  block = add_xy_columns(block);
  if (get_column_id_by_name(block, "z") == -1) block = add_float_column(block, "z");
  return block;
}

struct Block * add_rgb_columns(struct Block * block)
{
  if (get_column_id_by_name(block, "red") == -1)   block = add_float_column(block, "red");
  if (get_column_id_by_name(block, "green") == -1) block = add_float_column(block, "green");
  if (get_column_id_by_name(block, "blue") == -1)  block = add_float_column(block, "blue");
  return block;
}

struct Block * add_rgba_columns(struct Block * block)
{
  block = add_rgb_columns(block);
  if (get_column_id_by_name(block, "alpha") == -1) block = add_float_column(block, "alpha");
  return block;
}

void blank_column_values(struct Block * block, char * column_name)
{
  if (block == NULL) { fprintf(stderr, "blank_column_values called on NULL block\n"); return; }
  if (column_name == NULL) { fprintf(stderr, "blank_column_values called with NULL column_name\n"); return; }
  uint32_t column_id = get_column_id_by_name(block, column_name);
  if (column_id == -1) { fprintf(stderr, "blank_column_values called on column '%s', column not found.\n", column_name); return; }
  struct Column * column = get_column(block, column_id);
  
  int row_id;
  for (row_id = 0 ; row_id < block->num_rows ; row_id++)
  {
    memset(get_cell(block, row_id, column_id), 0, column->bsize);//get_type_size(column->type));
  }
}

/*struct Block * column_string_set_length(struct Block * block, uint32_t column_id, int32_t length)
{
  fprintf(stderr, "column_string_set_length() not implemented\n");
  exit(0);
}*/

struct Block * copy_all_columns(struct Block * block, struct Block * src)
{
  int i;
  for (i = 0 ; i < src->num_columns ; i++)
  {
    struct Column * column = get_column(src, i);
    block = _add_column(block, column->type, column->bsize, column_get_name(column));
  }
  if (block->row_bsize != src->row_bsize)
  {
    fprintf(stderr, "copy_all_columns(%d, %d)\n", block->row_bsize, src->row_bsize);
    //exit(1);
  }
  return block;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void * get_cell(struct Block * block, uint32_t row_id, uint32_t column_id)
{
  int32_t * cell_offsets = get_cell_offsets(block);
  return (char*)get_row(block, row_id) + cell_offsets[column_id];
}

int32_t get_cell_as_int32(struct Block * block, uint32_t row_id, uint32_t column_id)
{
  void * cell = get_cell(block, row_id, column_id);
  struct Column * column = get_column(block, column_id);
  
  int32_t temp = -1;
  
  if      (column->type == TYPE_INT   && column->bsize == 4) temp = *(int32_t*)cell;
  else if (column->type == TYPE_INT   && column->bsize == 8) temp = *(int64_t*)cell;
  else if (column->type == TYPE_FLOAT && column->bsize == 4) temp = *(float*)cell;
  else if (column->type == TYPE_FLOAT && column->bsize == 8) temp = *(double*)cell;
  else if (column->type == TYPE_CHAR) temp = atoi((char*)cell);
  else { fprintf(stderr, "bad %s\n", __func__); return 0; }
  
  return temp;
}

int64_t get_cell_as_int64(struct Block * block, uint32_t row_id, uint32_t column_id)
{
  void * cell = get_cell(block, row_id, column_id);
  struct Column * column = get_column(block, column_id);
  
  int64_t temp = 0;
  
  if      (column->type == TYPE_INT   && column->bsize == 4) temp = *(int32_t*)cell;
  else if (column->type == TYPE_INT   && column->bsize == 8) temp = *(int64_t*)cell;
  else if (column->type == TYPE_FLOAT && column->bsize == 4) temp = *(float*)cell;
  else if (column->type == TYPE_FLOAT && column->bsize == 8) temp = *(double*)cell;
  else if (column->type == TYPE_CHAR) temp = atol((char*)cell);
  else { fprintf(stderr, "bad %s\n", __func__); return 0; }
  
  return temp;
}

float get_cell_as_float(struct Block * block, uint32_t row_id, uint32_t column_id)
{
  void * cell = get_cell(block, row_id, column_id);
  struct Column * column = get_column(block, column_id);
  
  float temp = 0;
  
  if      (column->type == TYPE_INT   && column->bsize == 4) temp = *(int32_t*)cell;
  else if (column->type == TYPE_INT   && column->bsize == 8) temp = *(int64_t*)cell;
  else if (column->type == TYPE_FLOAT && column->bsize == 4) temp = *(float*)cell;
  else if (column->type == TYPE_FLOAT && column->bsize == 8) temp = *(double*)cell;
  else if (column->type == TYPE_CHAR) temp = atof((char*)cell);
  else { fprintf(stderr, "bad %s\n", __func__); return 0; }
  
  return temp;
}

double get_cell_as_double(struct Block * block, uint32_t row_id, uint32_t column_id)
{
  void * cell = get_cell(block, row_id, column_id);
  struct Column * column = get_column(block, column_id);
  
  double temp = 0;
  
  if      (column->type == TYPE_INT   && column->bsize == 4) temp = *(int32_t*)cell;
  else if (column->type == TYPE_INT   && column->bsize == 8) temp = *(int64_t*)cell;
  else if (column->type == TYPE_FLOAT && column->bsize == 4) temp = *(float*)cell;
  else if (column->type == TYPE_FLOAT && column->bsize == 8) temp = *(double*)cell;
  else if (column->type == TYPE_CHAR) temp = atof((char*)cell);
  else { fprintf(stderr, "bad %s\n", __func__); return 0; }
  
  return temp;
}

char * get_cell_as_char_temp = NULL;
char * get_cell_as_char(struct Block * block, uint32_t row_id, uint32_t column_id)
{
  void * cell = get_cell(block, row_id, column_id);
  struct Column * column = get_column(block, column_id);
  
  if (get_cell_as_char_temp == NULL) get_cell_as_char_temp = malloc(500);
  
  if      (column->type == TYPE_INT   && column->bsize == 4) snprintf(get_cell_as_char_temp, 500, "%d",  *(int32_t*)cell);
  else if (column->type == TYPE_INT   && column->bsize == 8) snprintf(get_cell_as_char_temp, 500, "%ld", *(long*)cell);
  else if (column->type == TYPE_FLOAT && column->bsize == 4) snprintf(get_cell_as_char_temp, 500, "%f",  *(float*)cell);
  else if (column->type == TYPE_FLOAT && column->bsize == 8) snprintf(get_cell_as_char_temp, 500, "%lf", *(double*)cell);
  else if (column->type == TYPE_CHAR) strncpy(get_cell_as_char_temp, (char*)cell, 500);
  else { fprintf(stderr, "bad %s\n", __func__); return 0; }
}

void set_cell(struct Block * block, uint32_t row_id, uint32_t column_id, void * value)
{
  if (block == NULL) { fprintf(stderr, "set_cell called with null block\n"); exit(0); }
  if (row_id >= block->num_rows) { fprintf(stderr, "set_cell called with row_id >= block->num_rows\n"); exit(0); }
  if (column_id >= block->num_columns) { fprintf(stderr, "set_cell called with column_id(%d) >= block->num_columns(%d)\n", column_id, block->num_columns); exit(0); }
  
  struct Column * column = get_column(block, column_id);
  void * cell = get_cell(block, row_id, column_id);
  if (value == NULL)
    memset(cell, 0, column->bsize);//get_type_size(column->type));
  else
    memcpy(cell, value, column->bsize);//get_type_size(column->type));
}

void set_cell_from_int32(struct Block * block, uint32_t row_id, uint32_t column_id, int32_t data)
{
  if (block == NULL) { fprintf(stderr, "set_cell_from_int called with null block\n"); exit(0); }
  if (row_id >= block->num_rows) { fprintf(stderr, "set_cell_from_int called with row_id(%d) >= block->num_rows(%d)\n", row_id, block->num_rows); exit(0); }
  if (column_id >= block->num_columns) { fprintf(stderr, "set_cell_from_int called with column_id(%d) >= block->num_columns(%d)\n", column_id, block->num_columns); exit(0); }
  
  struct Column * column = get_column(block, column_id);
  void * cell = get_cell(block, row_id, column_id);
  if      (column->type == TYPE_INT && column->bsize == 4) *(int32_t*)cell = data;
  else if (column->type == TYPE_INT && column->bsize == 8) *(int64_t*)cell = data;
  else if (column->type == TYPE_FLOAT && column->bsize == 4) *(float*)cell = data;
  else if (column->type == TYPE_FLOAT && column->bsize == 8) *(double*)cell = data;
  else if (column->type == TYPE_CHAR) snprintf((char*)cell, column->bsize, "%d", data);
  else fprintf(stderr, "bad %s\n", __func__);
}

void set_cell_from_double(struct Block * block, uint32_t row_id, uint32_t column_id, double data)
{
  if (block == NULL) { fprintf(stderr, "set_cell_from_double called with null block\n"); exit(0); }
  if (row_id >= block->num_rows) { fprintf(stderr, "set_cell_from_double called with row_id >= block->num_rows\n"); exit(0); }
  if (column_id >= block->num_columns) { fprintf(stderr, "set_cell_from_double called with column_id(%d) >= block->num_columns(%d)\n", column_id, block->num_columns); exit(0); }
  
  struct Column * column = get_column(block, column_id);
  void * cell = get_cell(block, row_id, column_id);
  if      (column->type == TYPE_INT && column->bsize == 4) *(int32_t*)cell = data;
  else if (column->type == TYPE_INT && column->bsize == 8) *(int64_t*)cell = data;
  else if (column->type == TYPE_FLOAT && column->bsize == 4) *(float*)cell = data;
  else if (column->type == TYPE_FLOAT && column->bsize == 8) *(double*)cell = data;
  else if (column->type == TYPE_CHAR) snprintf((char*)cell, column->bsize, "%f", data);
  else fprintf(stderr, "bad %s\n", __func__);
}

void set_cell_from_string(struct Block * block, uint32_t row_id, uint32_t column_id, char * data)
{
  if (block == NULL) { fprintf(stderr, "set_cell_from_string called with null block\n"); exit(0); }
  if (row_id >= block->num_rows) { fprintf(stderr, "set_cell_from_string called with row_id >= block->num_rows\n"); exit(0); }
  if (column_id >= block->num_columns) { fprintf(stderr, "set_cell_from_string called with column_id(%d) >= block->num_columns(%d)\n", column_id, block->num_columns); exit(0); }
  
  struct Column * column = get_column(block, column_id);
  void * cell = get_cell(block, row_id, column_id);
  if      (column->type == TYPE_INT && column->bsize == 4) *(int32_t*)cell = atoi(data);
  else if (column->type == TYPE_INT && column->bsize == 8) *(int64_t*)cell = atol(data);
  else if (column->type == TYPE_FLOAT && column->bsize == 4) *(float*)cell = atof(data);
  else if (column->type == TYPE_FLOAT && column->bsize == 8) *(double*)cell = atof(data);
  else if (column->type == TYPE_CHAR) strncpy((char*)cell, data, column->bsize);
  else fprintf(stderr, "bad %s\n", __func__);
}

void fprintf_cell(FILE * fp, struct Block * block, uint32_t row_id, uint32_t column_id)
{
  struct Column * column = get_column(block, column_id);
  switch (column->type) {
    case TYPE_INT:
      if      (column->bsize == 4) fprintf(fp, "%d", *(int32_t*)get_cell(block, row_id, column_id));
      else if (column->bsize == 8) fprintf(fp, "%ld", *(long*)get_cell(block, row_id, column_id));
      else                         fprintf(stderr, "bad %s %s:(%d)\n", __func__, __FILE__, __LINE__);
      break;
    case TYPE_FLOAT:
      if      (column->bsize == 4) fprintf(fp, "%f", *(float*)get_cell(block, row_id, column_id));
      else if (column->bsize == 8) fprintf(fp, "%lf", *(double*)get_cell(block, row_id, column_id));
      else                         fprintf(stderr, "bad %s %s:(%d)\n", __func__, __FILE__, __LINE__);
      break;
    case TYPE_CHAR:
      fprintf(fp, "%s", (char*)get_cell(block, row_id, column_id));
      break;
    default:
      fprintf(stderr, "%s can't fprint_cell column '%s' of type %d\n", __func__, column_get_name(column), column->type);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Block * cached_xyz_block = NULL;
struct Block * cached_rgba_block = NULL;
int32_t cached_xyz_column_ids[3] = {0,0,0};
int32_t cached_rgba_column_ids[4] = {0,0,0,0};

void set_xy(struct Block * block, uint32_t row_id, float x, float y)
{
  if (row_id > block->num_rows) { fprintf(stderr, "set_xy invalid row_id\n"); exit(0); }
  if (cached_xyz_block != block)
  {
    cached_xyz_column_ids[0] = get_column_id_by_name(block, "x");
    cached_xyz_column_ids[1] = get_column_id_by_name(block, "y");
    cached_xyz_column_ids[2] = get_column_id_by_name(block, "z");
    cached_xyz_block = block;
  }
  if (cached_xyz_column_ids[0] != -1) set_cell(block, row_id, cached_xyz_column_ids[0], &x);
  if (cached_xyz_column_ids[1] != -1) set_cell(block, row_id, cached_xyz_column_ids[1], &y);
}

void set_xyz(struct Block * block, uint32_t row_id, float x, float y, float z)
{
  set_xy(block, row_id, x, y);
  if (cached_xyz_column_ids[2] != -1) set_cell(block, row_id, cached_xyz_column_ids[2], &z);
}

void set_rgb(struct Block * block, uint32_t row_id, float r, float g, float b)
{
  if (row_id > block->num_rows) { fprintf(stderr, "set_rgb invalid row_id\n"); exit(0); }
  if (cached_rgba_block != block)
  {
    cached_rgba_column_ids[0] = get_column_id_by_name(block, "red");
    cached_rgba_column_ids[1] = get_column_id_by_name(block, "green");
    cached_rgba_column_ids[2] = get_column_id_by_name(block, "blue");
    cached_rgba_column_ids[3] = get_column_id_by_name(block, "alpha");
    cached_rgba_block = block;
  }
  if (cached_rgba_column_ids[0] != -1) set_cell(block, row_id, cached_rgba_column_ids[0], &r);
  if (cached_rgba_column_ids[1] != -1) set_cell(block, row_id, cached_rgba_column_ids[1], &g);
  if (cached_rgba_column_ids[2] != -1) set_cell(block, row_id, cached_rgba_column_ids[2], &b);
}

void set_rgba(struct Block * block, uint32_t row_id, float r, float g, float b, float a)
{
  set_rgb(block, row_id, r, g, b);
  if (cached_rgba_column_ids[3] != -1) set_cell(block, row_id, cached_rgba_column_ids[3], &a);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Block * add_row(struct Block * block)
{
  if (block == NULL) { fprintf(stderr, "add_row called on a NULL block\n"); return NULL; }
  
  block = set_num_rows(block, block->num_rows + 1);
  
  return block;
}

struct Block * add_row_with_data(struct Block * block, int num_columns, ...)
{
  if (block == NULL) { fprintf(stderr, "add_row_with_data called on a NULL block\n"); return NULL; }
  if (block->num_columns != num_columns) { fprintf(stderr, "block num_columns not the same as provided num_columns\n"); return block; }
  
  block = add_row(block);
  
  int row_id = block->num_rows - 1;
  int column_id;
  va_list v1;
  va_start(v1, num_columns);
  for (column_id = 0 ; column_id < block->num_columns ; column_id++)
  {
    struct Column * column = get_column(block, column_id);
    if      (column->type == TYPE_INT && column->bsize == 4)   { int32_t value = va_arg(v1, int);          set_cell(block, row_id, column_id, &value); }
    else if (column->type == TYPE_INT && column->bsize == 8)   { long value = va_arg(v1, long);            set_cell(block, row_id, column_id, &value); }
    else if (column->type == TYPE_FLOAT && column->bsize == 4) { float value = (float)va_arg(v1, double);  set_cell(block, row_id, column_id, &value); }
    else if (column->type == TYPE_FLOAT && column->bsize == 8) { double value = va_arg(v1, double);        set_cell(block, row_id, column_id, &value); }
    else if (column->type == TYPE_CHAR)    { char * value = va_arg(v1, char *);        set_cell(block, row_id, column_id, value); }
    else fprintf(stderr, "bad %s\n", __func__);
  }
  va_end(v1);
  
  return block;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct Block * sort_block_using_int32_column(struct Block * block, int32_t column_id, char order)
{
  if (block == NULL) { fprintf(stderr, "add_row_with_data called on a NULL block\n"); return; }
  if (block->num_rows <= 1) return;
  
  int32_t * indexes = malloc(sizeof(int32_t)*block->num_rows);
  
  struct Block * ret = new_block();
  ret = copy_all_attributes(ret, block);
  ret = copy_all_columns(ret, block);
  ret = set_num_rows(ret, block->num_rows);
  
  int i,j;
  for (i = 0 ; i < block->num_rows ; i++)
    indexes[i] = i;
  
  int loop_count = 0;
  
  int sorted = 0;
  while (sorted == 0)
  {
    sorted = 1;
    for (i = 0 ; i < block->num_rows - 1 ; i++)
    {
      if ((order && get_cell_as_int32(block, indexes[i], column_id) < get_cell_as_int32(block, indexes[i+1], column_id)) || 
          (!order && get_cell_as_int32(block, indexes[i], column_id) > get_cell_as_int32(block, indexes[i+1], column_id)))
      {
        int32_t temp = indexes[i];
        indexes[i] = indexes[i+1];
        indexes[i+1] = temp;
        sorted = 0;
        i--;
      }
    }
    loop_count++;
  }
  
  for (i = 0 ; i < block->num_rows ; i++)
    memcpy(get_row(ret, i), get_row(block, indexes[i]), block->row_bsize);
  
  fprintf(stderr, "really slow sort algorithm, took %d loops for %d rows\n", loop_count, block->row_bsize);
  
  free(indexes);
  free_block(block);
  
  return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

void inspect_block(struct Block * block)
{
  if (block == NULL) { fprintf(stderr, "inspect_block called on a NULL block\n"); return; }
  fprintf(stderr, "\nblock (%d+%d+%d=%d total size in bytes - with %ld byte header)\n", block->attributes_bsize, block->columns_bsize, block->data_bsize, block->attributes_bsize + block->columns_bsize + block->data_bsize, SIZEOF_STRUCT_BLOCK);
  
  if (block->num_attributes == 0)
    fprintf(stderr, "     ->no_attributes\n");
  else
  {
    fprintf(stderr, "     ->attributes(count:%d, %d bytes) = [\n", block->num_attributes, block->attributes_bsize);
    fprintf(stderr, "       ->attributes_offsets(");
    int attribute_id;
    int32_t * attribute_offset = get_attribute_offsets(block);
    for (attribute_id = 0 ; attribute_id < block->num_attributes ;  attribute_id++)
      fprintf(stderr, "%s%d", (attribute_id==0)?"":", ", attribute_offset[attribute_id]);
    fprintf(stderr, ")\n");
    
    for (attribute_id = 0 ; attribute_id < block->num_attributes ;  attribute_id++)
    {
      struct Attribute * attribute = get_attribute(block, attribute_id);
      char * name = attribute_get_name(attribute);
      void * value = attribute_get_value(attribute);
      fprintf(stderr, "       [%2d][%7s] \"%s\" = ", attribute_id, get_type_name(attribute->type, attribute->value_length), name);
      fprintf_attribute_value(stderr, block, attribute_id);
      fprintf(stderr, "\n");
    }
    fprintf(stderr, "     ]\n");
  }
  
  if (block->num_columns == 0)
    fprintf(stderr, "     ->no_fields\n");
  else
  {
    fprintf(stderr, "     ->columns(count:%d, %d bytes) = [\n", block->num_columns, block->columns_bsize);
    fprintf(stderr, "       ->column_offsets(");
    int column_id;
    int32_t * column_offset = get_column_offsets(block);
    int32_t * cell_offset = get_cell_offsets(block);
    for (column_id = 0 ; column_id < block->num_columns ;  column_id++)
      fprintf(stderr, "%s%d", (column_id==0)?"":", ", column_offset[column_id]);
    fprintf(stderr, ")\n");
    fprintf(stderr, "       ->cell_offsets(");
    for (column_id = 0 ; column_id < block->num_columns ;  column_id++)
      fprintf(stderr, "%s%d", (column_id==0)?"":", ", cell_offset[column_id]);
    fprintf(stderr, ")\n");
    
    for (column_id = 0 ; column_id < block->num_columns ; column_id++)
    {
      struct Column * column = get_column(block, column_id);
      fprintf(stderr, "       [%2d][%7s][%3d][%3d] = \"%s\"\n", 
        column_id, get_type_name(column->type, column->bsize), 
        column_offset[column_id], cell_offset[column_id], 
        column_get_name(column));
    }
    fprintf(stderr, "     ]\n");
  }
  
  if (block->num_rows == 0)
    fprintf(stderr, "     ->no_data(row_bsize %d bytes)\n", block->row_bsize);
  else
  {
    fprintf(stderr, "     ->data(count:%d, %d bytes (%d each row)) = [\n", block->num_rows, block->data_bsize, block->row_bsize);
    int row_id;
    for (row_id = 0 ; row_id < block->num_rows ; row_id++)
    {
      fprintf(stderr, "       [%2d] = { ", row_id);
      
      int column_id;
      for (column_id = 0 ; column_id < block->num_columns ; column_id++)
      {
        struct Column * column = get_column(block, column_id);
        if (column == NULL) continue;
        void * cell = get_cell(block, row_id, column_id);
        if (column_id != 0) fprintf(stderr, ", ");
        
        if      (cell == NULL) fprintf(stderr, "NULL");
        else if (column->type == TYPE_INT && column->bsize == 4)   fprintf(stderr, "%d", *(int32_t*)cell);
        else if (column->type == TYPE_INT && column->bsize == 8)   fprintf(stderr, "%ld", *(long*)cell);
        else if (column->type == TYPE_FLOAT && column->bsize == 4) fprintf(stderr, "%f", *(float*)cell);
        else if (column->type == TYPE_FLOAT && column->bsize == 8) fprintf(stderr, "%lf", *(double*)cell);
        else if (column->type == TYPE_CHAR)    fprintf(stderr, "\"%s\"", (char*)cell);
      }
      
      if (row_id >= 25 && row_id < block->num_rows - 5) { fprintf(stderr, " }\n       ....\n"); row_id = block->num_rows-5; continue; }
      fprintf(stderr, " }\n");
    }
    fprintf(stderr, "     ]\n");
  }
  fprintf(stderr, "\n");
}

char block_type_names[12][20] = {
  "unknown",
  "int8", "int16", "int32", "int64",
  "uint8", "uint16", "uint32", "uint64",
  "float", "double", "char"
};

char * get_type_name(enum TYPE type, uint32_t bsize)
{
  if      (type == 0)                        return block_type_names[0];
  else if (type == TYPE_INT && bsize == 1)   return block_type_names[1];
  else if (type == TYPE_INT && bsize == 2)   return block_type_names[2];
  else if (type == TYPE_INT && bsize == 4)   return block_type_names[3];
  else if (type == TYPE_INT && bsize == 8)   return block_type_names[4];
  else if (type == TYPE_UINT && bsize == 1)  return block_type_names[5];
  else if (type == TYPE_UINT && bsize == 2)  return block_type_names[6];
  else if (type == TYPE_UINT && bsize == 4)  return block_type_names[7];
  else if (type == TYPE_UINT && bsize == 8)  return block_type_names[8];
  else if (type == TYPE_FLOAT && bsize == 4) return block_type_names[9];
  else if (type == TYPE_FLOAT && bsize == 8) return block_type_names[10];
  else if (type == TYPE_CHAR)
  {
    static char get_type_name_temp[100] = "";
    sprintf(get_type_name_temp, "char%3d", bsize);
    return get_type_name_temp;
  }
}

