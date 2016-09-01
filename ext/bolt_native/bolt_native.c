#include "bolt_native.h"
#include <arpa/inet.h>
#include "ruby/encoding.h"
VALUE rb_mBolt;
VALUE rb_mBolt_packStream;
VALUE rb_mBolt_structure;
VALUE rb_mBolt_basic_structure;
VALUE rb_mBolt_ByteBuffer;
ID id_pack_internal;
ID id_fields;
ID id_signature;
ID id_from_pack_stream;

static rb_encoding * utf8;
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

void write_byte(WriteBuffer *b, const uint8_t byte){
  ensure_capacity(b, 1);
  *(b->position++)= byte;
  b->consumed++;
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
  id_from_pack_stream = rb_intern("from_pack_stream");
  rb_mBolt_structure = rb_const_get(rb_mBolt_packStream, rb_intern("Structure"));
  rb_mBolt_basic_structure = rb_const_get(rb_mBolt_packStream, rb_intern("BasicStruct"));

  rb_define_singleton_method(rb_mBolt_packStream, "pack_internal", RUBY_METHOD_FUNC(rb_bolt_pack_internal),2);

  rb_mBolt_ByteBuffer = rb_const_get(rb_mBolt, rb_intern("ByteBuffer"));

  rb_define_alloc_func(rb_mBolt_ByteBuffer, rb_byte_buffer_allocate);
  rb_define_method(rb_mBolt_ByteBuffer, "initialize", RUBY_METHOD_FUNC(rb_byte_buffer_initialize),1);

  rb_define_method(rb_mBolt_ByteBuffer, "read_uint8", RUBY_METHOD_FUNC(rb_bolt_read_uint8),0);
  rb_define_method(rb_mBolt_ByteBuffer, "read_uint16", RUBY_METHOD_FUNC(rb_bolt_read_uint16),0);
  rb_define_method(rb_mBolt_ByteBuffer, "read_uint32", RUBY_METHOD_FUNC(rb_bolt_read_uint32),0);
  rb_define_method(rb_mBolt_ByteBuffer, "read_uint64", RUBY_METHOD_FUNC(rb_bolt_read_uint64),0);

  rb_define_method(rb_mBolt_ByteBuffer, "read_int8", RUBY_METHOD_FUNC(rb_bolt_read_int8),0);
  rb_define_method(rb_mBolt_ByteBuffer, "read_int16", RUBY_METHOD_FUNC(rb_bolt_read_int16),0);
  rb_define_method(rb_mBolt_ByteBuffer, "read_int32", RUBY_METHOD_FUNC(rb_bolt_read_int32),0);
  rb_define_method(rb_mBolt_ByteBuffer, "read_int64", RUBY_METHOD_FUNC(rb_bolt_read_int64),0);

  rb_define_method(rb_mBolt_ByteBuffer, "read_double", RUBY_METHOD_FUNC(rb_bolt_read_double),0);

  rb_define_method(rb_mBolt_ByteBuffer, "read_string", RUBY_METHOD_FUNC(rb_bolt_read_string),1);

  rb_define_method(rb_mBolt_ByteBuffer, "at_end?", RUBY_METHOD_FUNC(rb_bolt_at_end_p),0);
  rb_define_method(rb_mBolt_ByteBuffer, "fetch_next_field", RUBY_METHOD_FUNC(rb_bolt_fetch_next_field),0);
  utf8 =rb_utf8_encoding();
}


VALUE rb_bolt_pack_internal(VALUE self, VALUE rb_buffer, VALUE item){
  WriteBuffer buffer;
  allocate(&buffer, 128);
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
      write_byte(buffer, (uint8_t)'\xC0');
      break;
    case T_TRUE:
      write_byte(buffer, (uint8_t)'\xC3');
      break;
    case T_FALSE:
      write_byte(buffer, (uint8_t)'\xC2');
      break;
    case T_SYMBOL:
      bolt_encode_string(rb_sym_to_s(item), buffer);
      break;
    case T_FLOAT:
      bolt_encode_double(item, buffer);      
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
  uint64_t bytes;
  double f;
} FloatSwapper;
#pragma pack()


#pragma pack(1)
typedef union {
  struct {
    uint8_t marker;
    FloatSwapper d;
  } swapper;
  uint8_t raw[9];
} FloatHeader;
#pragma pack()

#define swap(a, b) ((a)^=(b),(b)^=(a),(a)^=(b))

void bolt_encode_double(VALUE rbfloat, WriteBuffer *buffer){
  ensure_capacity(buffer,sizeof(FloatHeader));
  FloatHeader *f = (FloatHeader*)buffer->position;
  f->swapper.marker = 0xC1;
  f->swapper.d.f = RFLOAT_VALUE(rbfloat);
  f->swapper.d.bytes = htonll(f->swapper.d.bytes);
 
  buffer->position += sizeof(FloatHeader);
  buffer->consumed += sizeof(FloatHeader);
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
  VALUE encoded = rb_enc_get(string) == utf8 ?
                  string : rb_str_encode(string, rb_enc_from_encoding(utf8), 0,Qnil);
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


#pragma pack(1)
typedef union {
  uint8_t marker;
  struct {
    uint8_t marker;
    uint8_t value;
  } one_byte;
  struct {
    uint8_t marker;
    uint16_t value;
  } two_byte;
  struct {
    uint8_t marker;
    uint32_t value;
  } four_byte;
  struct {
    uint8_t marker;
    uint64_t value;
  } eight_byte;
  uint8_t raw[9];
} IntHeader;
#pragma pack()


void rb_bolt_encode_integer(VALUE self, VALUE integer, WriteBuffer *buffer){
  size_t length=0;
  ensure_capacity(buffer,sizeof(IntHeader));
  long long value = NUM2LL(integer);
  unsigned long long unsigned_value = (unsigned long long) value;
  IntHeader *header = (IntHeader*)buffer->position;
  if(value >= -0x10 && value < 0x80){
    header->marker = (uint8_t)(unsigned_value & 0xFF);
    length = 1;
  }else if( -0x80 <= value && value < 0x80){
    header->one_byte.marker = '\xC8';
    header->one_byte.value = (uint8_t)(unsigned_value & 0xFF);
    length = 2;
  }
  else if (-0x8000 <= value && value < 0x8000){
    header->two_byte.marker = '\xC9';
    header->two_byte.value = htons((uint16_t)unsigned_value);
    length = 3;
  }
  else if (-0x80000000L <= value && value < 0x80000000L){
    header->four_byte.marker = '\xCA';
    header->four_byte.value = htonl((uint32_t)unsigned_value);
    length = 5;
  }else {
    header->eight_byte.marker = '\xCB';
    header->eight_byte.value = htonll(unsigned_value);
    length = 9;
  }
  buffer->position += length;
  buffer->consumed += length;
}

VALUE rb_byte_buffer_allocate(VALUE klass){
  ByteBuffer *buffer;
  VALUE wrapped = Data_Make_Struct(rb_mBolt_ByteBuffer, ByteBuffer, rb_byte_buffer_mark, RUBY_DEFAULT_FREE, buffer);
  buffer->rb_buffer = Qnil;
  buffer->rb_registry = Qnil;
  buffer->position = NULL;
  return wrapped;
}
void rb_byte_buffer_mark(void *object){
  ByteBuffer *buffer = (ByteBuffer*) object;
  if(buffer->rb_buffer != Qnil){
    rb_gc_mark(buffer->rb_buffer);
  }
  if(buffer->rb_registry != Qnil){
    rb_gc_mark(buffer->rb_registry);
  }
}


void bolt_check_buffer(ByteBuffer *object, size_t count){
  if(object->position + count > object->end){
    rb_raise(rb_eArgError, "Tried to read %lu bytes from buffer sized %lu", count, RSTRING_LEN(object->rb_buffer));
  }
}

uint8_t bolt_read_uint8(ByteBuffer *object){
  bolt_check_buffer(object, 1);
  uint8_t result = *(uint8_t*) (object->position++);
  return result;
}

VALUE rb_bolt_read_uint8(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  return INT2FIX(bolt_read_uint8(buffer));
}

uint16_t bolt_read_uint16(ByteBuffer *object){
  bolt_check_buffer(object, 2);
  uint16_t result = *(uint16_t*) (object->position);
  object->position +=2;
  return ntohs(result);
}

VALUE rb_bolt_read_uint16(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  return INT2FIX(bolt_read_uint16(buffer));
}

uint32_t bolt_read_uint32(ByteBuffer *object){
  bolt_check_buffer(object, 4);
  uint32_t result = *(uint32_t*) (object->position);
  object->position +=4;
  return ntohl(result);
}

VALUE rb_bolt_read_uint32(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  return INT2FIX(bolt_read_uint32(buffer));
}

uint64_t bolt_read_uint64(ByteBuffer *object){
  bolt_check_buffer(object, 8);
  uint64_t result = *(uint64_t*) (object->position);
  object->position += 8;
  return ntohll(result);
}

VALUE rb_bolt_read_uint64(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  return ULL2NUM(bolt_read_uint64(buffer));
}


int8_t bolt_read_int8(ByteBuffer *object){
  bolt_check_buffer(object, 1);
  int8_t result = *(int8_t*) (object->position++);
  return result;
}

VALUE rb_bolt_read_int8(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  return INT2FIX(bolt_read_int8(buffer));
}

int16_t bolt_read_int16(ByteBuffer *object){
  bolt_check_buffer(object, 2);
  uint16_t result = *(uint16_t*) (object->position);
  object->position +=2;
  return (int16_t)ntohs(result);
}

VALUE rb_bolt_read_int16(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  return INT2FIX(bolt_read_int16(buffer));
}

int32_t bolt_read_int32(ByteBuffer *object){
  bolt_check_buffer(object, 4);
  uint32_t result = *(uint32_t*) (object->position);
  object->position +=4;
  return (int32_t)ntohl(result);
}

VALUE rb_bolt_read_int32(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  return INT2FIX(bolt_read_int32(buffer));
}

int64_t bolt_read_int64(ByteBuffer *object){
  bolt_check_buffer(object, 8);
  uint64_t result = *(uint64_t*) (object->position);
  object->position += 8;
  return (int64_t)ntohll(result);
}

VALUE rb_bolt_read_int64(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  return LL2NUM(bolt_read_int64(buffer));
}


double bolt_read_double(ByteBuffer *object){
  bolt_check_buffer(object, 8);
  FloatSwapper result = *(FloatSwapper*) (object->position);
  result.bytes = ntohll(result.bytes);
  object->position += 8;
  return result.f;
}

VALUE rb_bolt_read_double(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  return DBL2NUM(bolt_read_double(buffer));
}

VALUE rb_bolt_read_string(VALUE self, VALUE rb_length){
  long length = FIX2INT(rb_length);
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  return bolt_read_string(buffer, length);
}

VALUE bolt_read_string(ByteBuffer *buffer, long length)
{
  bolt_check_buffer(buffer, length);
  VALUE string = rb_utf8_str_new((const char*)buffer->position, length);

  buffer->position += length;
  return string;

}

VALUE rb_bolt_at_end_p(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  if(buffer->position == buffer->end){
    return Qtrue;
  }else{
    return Qnil;
  }
}


VALUE rb_byte_buffer_initialize(VALUE self, VALUE string){
  ByteBuffer *buffer;
  Check_Type(string, T_STRING);
  Data_Get_Struct(self, ByteBuffer, buffer);

  RB_OBJ_FREEZE(string);
  buffer->rb_buffer = string;
  buffer->position = (uint8_t*) RSTRING_PTR(string);
  buffer->end = (uint8_t*)RSTRING_PTR(string) + RSTRING_LEN(string);
  return self;
}

VALUE rb_bolt_fetch_next_field(VALUE self){
  ByteBuffer *buffer;
  Data_Get_Struct(self, ByteBuffer, buffer);
  buffer->rb_registry = rb_ivar_get(self, rb_intern("@registry"));
  return bolt_fetch_next_field(buffer);
}

VALUE bolt_fetch_next_field(ByteBuffer *buffer){
  uint8_t marker = bolt_read_uint8(buffer);
  if(marker & 1<<7)
  {
    if(marker >= 0xF0){
      return INT2FIX(marker - 0x100);
    }
    else {
      if(marker & 1<< 6) {
        switch(marker){
          case 0xC0: return Qnil;
          case 0xC1: return rb_float_new(bolt_read_double(buffer));
          case 0xC2: return Qfalse;
          case 0xC3: return Qtrue;
          case 0xC8: return INT2FIX(bolt_read_int8(buffer));
          case 0xC9: return INT2FIX(bolt_read_int16(buffer));
          case 0xCA: return INT2FIX(bolt_read_int32(buffer));
          case 0xCB: return LL2NUM(bolt_read_int64(buffer));
      
          case 0xD0: return bolt_read_string(buffer, bolt_read_uint8(buffer));
          case 0xD1: return bolt_read_string(buffer, bolt_read_uint16(buffer));
          case 0xD2: return bolt_read_string(buffer, bolt_read_uint32(buffer));

          case 0xD4: return bolt_read_list(buffer, bolt_read_uint8(buffer));
          case 0xD5: return bolt_read_list(buffer, bolt_read_uint16(buffer));
          case 0xD6: return bolt_read_list(buffer, bolt_read_uint32(buffer));

          case 0xD8: return bolt_read_map(buffer, bolt_read_uint8(buffer));
          case 0xD9: return bolt_read_map(buffer, bolt_read_uint16(buffer));
          case 0xDA: return bolt_read_map(buffer, bolt_read_uint32(buffer));

          case 0xDC: return bolt_read_structure(buffer, bolt_read_uint8(buffer));
          case 0xDD: return bolt_read_structure(buffer, bolt_read_uint16(buffer));

          default:
            rb_raise(rb_eArgError, "Unknown marker %x", marker);
        }
      }
      else{
        switch(marker & 0xF0) {
          case 0x80: return bolt_read_string(buffer, marker & 0x0F);
          case 0x90: return bolt_read_list(buffer, marker & 0x0F);
          case 0xA0: return bolt_read_map(buffer, marker & 0x0F);
          case 0xB0: return bolt_read_structure(buffer, marker & 0x0F);
          default: /*small negative integer*/
            rb_raise(rb_eArgError, "Unknown marker %x", marker);
            break;
        }
      }
    }
  }
  else { /*marker is <0x80 : this is an int*/
    return INT2FIX(marker);
  }
}

VALUE bolt_read_list(ByteBuffer * buffer, long length){
  VALUE result = rb_ary_new_capa(length);
  for(long i=0; i< length;i++){
    rb_ary_push(result, bolt_fetch_next_field(buffer));
  }
  return result;
}

VALUE bolt_read_map(ByteBuffer * buffer, long length){
  VALUE result = rb_hash_new();
  for(long i=0; i< length;i++){
    VALUE key = bolt_fetch_next_field(buffer);
    VALUE value = bolt_fetch_next_field(buffer);
    rb_hash_aset(result, key, value);
  }
  return result;
}

VALUE bolt_read_structure(ByteBuffer * buffer, long length){
  int8_t signature = bolt_read_int8(buffer);
  VALUE fields = bolt_read_list(buffer, length);
  VALUE klass = rb_mBolt_basic_structure;
  if(buffer->rb_registry != Qnil){
    VALUE present = rb_hash_aref(buffer->rb_registry, INT2FIX(signature));
    if(RTEST(present)){
      klass = present;
    }
  }
  return rb_funcall(klass, id_from_pack_stream, 2, INT2FIX(signature), fields);
}
