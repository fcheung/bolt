#ifndef BOLT_NATIVE_H
#define BOLT_NATIVE_H 1

#include "ruby.h"

typedef struct {
  uint8_t *buffer;
  uint8_t *position;
  size_t consumed;
  size_t allocated;
} WriteBuffer;


void rb_bolt_encode_integer(VALUE self, VALUE integer, WriteBuffer* buffer);

VALUE rb_bolt_pack_internal(VALUE self, VALUE buffer, VALUE item);

void pack_internal(WriteBuffer *buffer, VALUE item);
void bolt_encode_array(VALUE array, WriteBuffer* buffer);
void bolt_encode_hash(VALUE array, WriteBuffer* buffer);
void bolt_encode_string(VALUE array, WriteBuffer* buffer);
void bolt_encode_double(VALUE rbfloat, WriteBuffer* buffer);
void bolt_encode_structure(VALUE structure, WriteBuffer* buffer);

typedef struct {
  VALUE rb_buffer;
  size_t offset;
  size_t length;
} ByteBuffer;

uint8_t bolt_read_uint8(ByteBuffer *b);
uint16_t bolt_read_uint16(ByteBuffer *b);
uint32_t bolt_read_uint32(ByteBuffer *b);
uint64_t bolt_read_uint64(ByteBuffer *b);
int8_t bolt_read_int8(ByteBuffer *b);
int16_t bolt_read_int16(ByteBuffer *b);
int32_t bolt_read_int32(ByteBuffer *b);
int64_t bolt_read_int64(ByteBuffer *b);
double bolt_read_double(ByteBuffer *b);

VALUE rb_bolt_read_uint8(VALUE self);
VALUE rb_bolt_read_uint16(VALUE self);
VALUE rb_bolt_read_uint32(VALUE self);
VALUE rb_bolt_read_uint64(VALUE self);
VALUE rb_bolt_read_int8(VALUE self);
VALUE rb_bolt_read_int16(VALUE self);
VALUE rb_bolt_read_int32(VALUE self);
VALUE rb_bolt_read_int64(VALUE self);
VALUE rb_bolt_read_double(VALUE self);

VALUE rb_bolt_read_string(VALUE self, VALUE length);

VALUE rb_bolt_at_end_p(VALUE self);
void bolt_check_buffer(ByteBuffer *, size_t);


VALUE rb_byte_buffer_initialize(VALUE self, VALUE string);
void rb_byte_buffer_mark(void *);
VALUE rb_byte_buffer_allocate(VALUE);
#endif /* BOLT_NATIVE_H */
