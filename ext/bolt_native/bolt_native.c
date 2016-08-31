#include "bolt_native.h"
#include <arpa/inet.h>
#include "ruby/encoding.h"
VALUE rb_mBolt;
VALUE rb_mBolt_packStream;
VALUE rb_mBolt_structure;
ID id_pack_internal;
ID id_fields;
ID id_signature;
#pragma pack(1)
typedef union {
  struct  {
    uint8_t marker;
    union {
      uint8_t byte_length;
      uint16_t two_byte_length;
      uint32_t four_byte_length;
    } lengths;
  } structured;
  uint8_t raw[5];
} MarkerHeader;
#pragma pack()


void ensure_capacity(WriteBuffer *b, size_t bytes){
  if(b->consumed + bytes > b->allocated){
    size_t new_size = b->allocated * 2 + bytes;
    uint8_t *new_buffer = realloc(b->buffer,new_size);
    if(!new_buffer){
      free(b->buffer);
      rb_raise(rb_eNoMemError, "failed to resize buffer to %lu", bytes);
    }
    b->buffer = new_buffer;
    b->position = b->buffer + b->consumed;
    b->allocated = new_size;
  }
}

void allocate(WriteBuffer *b, size_t size){
  b->buffer = malloc(size);
  if(!b->buffer){
    rb_raise(rb_eNoMemError, "failed to resize buffer to %lu", size);    
  }
  b->allocated = size;
  b->consumed = 0;
  b->position = b->buffer;
}

void deallocate(WriteBuffer *b){
  free(b->buffer);
  memset(b, 0, sizeof(WriteBuffer));
}

inline void write_bytes(WriteBuffer *b, const uint8_t *bytes, size_t size){
  ensure_capacity(b, size);
  memcpy(b->position, bytes, size);
  b->position += size;
  b->consumed += size;
}
void
Init_bolt_native(void)
{
  rb_mBolt = rb_const_get(rb_cObject, rb_intern("Bolt"));
  rb_mBolt_packStream = rb_const_get(rb_mBolt, rb_intern("PackStream"));
  id_pack_internal = rb_intern("pack_internal");
  id_signature = rb_intern("signature");
  id_fields = rb_intern("fields");
  rb_mBolt_structure = rb_const_get(rb_mBolt_packStream, rb_intern("Structure"));

  rb_define_singleton_method(rb_mBolt_packStream, "pack_internal", RUBY_METHOD_FUNC(rb_bolt_pack_internal),2);
}

VALUE rb_bolt_pack_internal(VALUE self, VALUE rb_buffer, VALUE item){
  WriteBuffer buffer;
  allocate(&buffer, 16);
  pack_internal(&buffer, item);
  rb_str_buf_cat(rb_buffer, (const char*)buffer.buffer, buffer.consumed);
  deallocate(&buffer);
  return rb_buffer;
}

void pack_internal(WriteBuffer *buffer, VALUE item){
  switch(rb_type(item)){
    case T_BIGNUM:
    case T_FIXNUM:
      rb_bolt_encode_integer(rb_mBolt_packStream, item, buffer);
      break;
    case T_NIL:
      write_bytes(buffer, (uint8_t*)"\xC0", 1);
      break;
    case T_TRUE:
      write_bytes(buffer, (uint8_t*)"\xC3", 1);
      break;
    case T_FALSE:
      write_bytes(buffer, (uint8_t*)"\xC2", 1);
      break;
    case T_SYMBOL:
      bolt_encode_string(rb_sym_to_s(item), buffer);
      break;
    case T_FLOAT:
      bolt_encode_float(item, buffer);      
      break;
    case T_HASH:
      bolt_encode_hash(item, buffer);
      break;      
    case T_ARRAY:
      bolt_encode_array(item, buffer);
      break;
    case T_STRING:
      bolt_encode_string(item, buffer);
      break;
    default:
      if(RTEST(rb_obj_is_kind_of(item, rb_mBolt_structure))){
        bolt_encode_structure(item, buffer);
      }
      else{
        VALUE inspectOutput = rb_inspect(item);
        rb_raise(rb_eArgError, "value %s cannot be packstreamed", StringValueCStr(inspectOutput) );
      }
    
  }
}


static inline void append_marker_and_length(uint8_t base_marker, uint8_t base_length_marker, long length, WriteBuffer *buffer  ){
  size_t header_size = 0;
  ensure_capacity(buffer, 5); //biggest possible header
  MarkerHeader *header = (MarkerHeader*) buffer->position;
  if(length <= 15){
    header->structured.marker = base_marker + length;
    header_size = 1;
  }else{
    if(length <= 255){
      header->structured.marker = base_length_marker;
      header->structured.lengths.byte_length = (uint8_t)length;
      header_size = 2;
    }else if(length <= 65535){
      header->structured.marker = base_length_marker+1;
      header->structured.lengths.two_byte_length = htons((uint16_t)length);
      header_size = 3;
    }else if(length <= 0x100000000){
      header->structured.marker = base_length_marker+2;
      header->structured.lengths.four_byte_length = htonl((uint32_t)length);
      header_size = 5;
    }else {
      rb_raise(rb_eRangeError,"Data is too long (%ld items)", length);
    }
  }
  buffer->position += header_size;
  buffer->consumed += header_size;
}



#pragma pack(1)
typedef union {
  struct {
    uint8_t marker;
    double d;
  } s;
  uint8_t raw[9];
} FloatHeader;

#pragma pack()

#define swap(a, b) ((a)^=(b),(b)^=(a),(a)^=(b))

void bolt_encode_float(VALUE rbfloat, WriteBuffer *buffer){
  FloatHeader f;
  f.s.marker = 0xC1;
  f.s.d = RFLOAT_VALUE(rbfloat);
  swap(f.raw[1], f.raw[8]);
  swap(f.raw[2], f.raw[7]);
  swap(f.raw[3], f.raw[6]);
  swap(f.raw[4], f.raw[5]);
 

  write_bytes(buffer, f.raw, sizeof(FloatHeader));
}

static int encode_hash_iterator(VALUE key, VALUE val, VALUE _buffer){
  WriteBuffer *buffer = (WriteBuffer*)_buffer;
  pack_internal(buffer, key);
  pack_internal(buffer, val);
  return ST_CONTINUE;
}


void bolt_encode_hash(VALUE hash, WriteBuffer *buffer){
  long length = RHASH_SIZE(hash);
  long offset = 0;
  append_marker_and_length(0xA0,0xD8, length, buffer);
  rb_hash_foreach(hash, encode_hash_iterator, (VALUE)buffer);
}


void bolt_encode_array(VALUE array, WriteBuffer *buffer) {
  long length = RARRAY_LEN(array);
  long offset = 0;
  append_marker_and_length(0x90,0xD4, length, buffer);
  for(; offset < length ;offset++){
    pack_internal(buffer, RARRAY_AREF(array,offset));
  }  
}

void bolt_encode_string(VALUE string, WriteBuffer *buffer) {
  VALUE encoded = rb_str_encode(string, rb_enc_from_encoding(rb_utf8_encoding()),
              0,Qnil);
  long length = RSTRING_LEN(encoded);
  append_marker_and_length(0x80,0xD0, length, buffer);
  write_bytes(buffer, (uint8_t*)RSTRING_PTR(encoded), RSTRING_LEN(encoded));
}


void bolt_encode_structure(VALUE structure, WriteBuffer *buffer) {
  VALUE fields = rb_funcall(structure, id_fields, 0);


  Check_Type(fields, T_ARRAY);
  long length = RARRAY_LEN(fields);

  if(length >= 65536){
    rb_raise(rb_eRangeError, "Too many struct fields: %ld", length);
    return;
  }
  VALUE signature = rb_funcall(structure, id_signature, 0);
  uint8_t signature_byte = (uint8_t)FIX2INT(signature);
  append_marker_and_length(0xB0,0xDC, length, buffer);
  write_bytes(buffer,&signature_byte,1);
  for(long offset =0; offset < length ;offset++){
    pack_internal(buffer, RARRAY_AREF(fields,offset));
  }  
}


void rb_bolt_encode_integer(VALUE self, VALUE integer, WriteBuffer *buffer){
  size_t length=0;
  uint8_t cbuffer[9]; /*an int is at most 1 byte length, 8 bytes data*/
  long long value = NUM2LL(integer);
  unsigned long long unsigned_value = (unsigned long long) value;
  if(value >= -0x10 && value < 0x80){
    cbuffer[0] = (uint8_t)(unsigned_value & 0xFF);
    length = 1;
  }else if( -0x80 <= value && value < 0x80){
    cbuffer[0] = '\xC8';
    cbuffer[1] = (uint8_t)(unsigned_value & 0xFF);
    length = 2;
  }
  else if (-0x8000 <= value && value < 0x8000){
    cbuffer[0] = '\xC9';
    cbuffer[1] = (uint8_t)((unsigned_value & 0xFF00) >> 8);
    cbuffer[2] = (uint8_t)((unsigned_value & 0xFF));
    length = 3;
  }
  else if (-0x80000000L <= value && value < 0x80000000L){
    cbuffer[0] = '\xCA';
    cbuffer[1] = (uint8_t)((unsigned_value >> 24)  & 0xFF);
    cbuffer[2] = (uint8_t)((unsigned_value >> 16)  & 0xFF);
    cbuffer[3] = (uint8_t)((unsigned_value >> 8)  & 0xFF);
    cbuffer[4] = (uint8_t)((unsigned_value)  & 0xFF);
    length = 5;
  }else {
    cbuffer[0] = '\xCB';
    cbuffer[1] = (uint8_t)((unsigned_value >> 56)  & 0xFF);
    cbuffer[2] = (uint8_t)((unsigned_value >> 48)  & 0xFF);
    cbuffer[3] = (uint8_t)((unsigned_value >> 40)  & 0xFF);
    cbuffer[4] = (uint8_t)((unsigned_value >> 32)  & 0xFF);
    cbuffer[5] = (uint8_t)((unsigned_value >> 24)  & 0xFF);
    cbuffer[6] = (uint8_t)((unsigned_value >> 16)  & 0xFF);
    cbuffer[7] = (uint8_t)((unsigned_value >> 8)  & 0xFF);
    cbuffer[8] = (uint8_t)((unsigned_value)  & 0xFF);
    length = 9;
  }
  write_bytes(buffer, cbuffer, length);

}