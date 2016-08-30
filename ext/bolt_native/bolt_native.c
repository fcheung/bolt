#include "bolt_native.h"
#include <arpa/inet.h>

VALUE rb_mBolt;
VALUE rb_mBolt_packStream;
ID id_pack_internal;

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

void
Init_bolt_native(void)
{
  rb_mBolt = rb_const_get(rb_cObject, rb_intern("Bolt"));
  rb_mBolt_packStream = rb_const_get(rb_mBolt, rb_intern("PackStream"));
  id_pack_internal = rb_intern("pack_internal");
  rb_define_singleton_method(rb_mBolt_packStream, "encode_integer", RUBY_METHOD_FUNC(rb_bolt_encode_integer),2);
  rb_define_singleton_method(rb_mBolt_packStream, "encode_array", RUBY_METHOD_FUNC(rb_bolt_encode_array),2);
}

VALUE pack_internal(VALUE buffer, VALUE item){
  if(IMMEDIATE_P(item)){
    if(FIXNUM_P(item)){
      rb_bolt_encode_integer(rb_mBolt_packStream, item, buffer);      
    }else{
      rb_funcall(rb_mBolt_packStream, id_pack_internal,2,buffer,item);
    }
  }else {
    switch(RB_BUILTIN_TYPE(item)){
      case T_BIGNUM:
        rb_bolt_encode_integer(rb_mBolt_packStream, item, buffer);
        break;
      default:
        rb_funcall(rb_mBolt_packStream, id_pack_internal,2,buffer,item);
    }
  }
  return buffer;
}


VALUE rb_bolt_encode_array(VALUE self, VALUE array, VALUE buffer){
  long length = RARRAY_LEN(array);
  long offset = 0;
  MarkerHeader header ={};

  if(length <= 15){
    header.structured.marker = 0x90 + length;
    rb_str_buf_cat(buffer, (const char*)header.raw,1);
  }else{
    if(length <= 255){
      header.structured.marker = 0xD4;
      header.structured.lengths.byte_length = (uint8_t)length;
      rb_str_buf_cat(buffer, (const char*)header.raw,2);
    }else if(length <= 65535){
      header.structured.marker = 0xD5;
      header.structured.lengths.two_byte_length = htons((uint16_t)length);
      rb_str_buf_cat(buffer, (const char*)header.raw,3);
    }else if(length <= 0x100000000){
      header.structured.marker = 0xD6;
      header.structured.lengths.four_byte_length = htonl((uint32_t)length);
      rb_str_buf_cat(buffer, (const char*)header.raw,5);
    }else {
      rb_raise(rb_eRangeError,"Array is too long (%ld items)", length);
    }
  }
  for(; offset < length ;offset++){
    pack_internal(buffer, RARRAY_AREF(array,offset));
  }
  return buffer;
}

VALUE rb_bolt_encode_integer(VALUE self, VALUE integer, VALUE buffer){
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
  return rb_str_buf_cat(buffer, (const char*)cbuffer, length);

}