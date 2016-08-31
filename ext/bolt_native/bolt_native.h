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
void bolt_encode_float(VALUE rbfloat, WriteBuffer* buffer);
void bolt_encode_structure(VALUE structure, WriteBuffer* buffer);

#endif /* BOLT_NATIVE_H */
