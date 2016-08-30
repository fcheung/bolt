# frozen_string_literal: true
module Bolt

  # A naive, pure ruby implementation of the packstream format. Other than structures, data is encoded/decoded to the obvious ruby primitives
  # 
  # For dumping, anything that includes the {Structure} module is consider a structure. It must respond to the signature and fields methods
  #
  #
  # For loading, structures are loaded as instances of {Bolt::PackStream::BasicStruct}. You can customize the classes loaded by passing a non nil registry to {Bolt::PackStream.unpack}
  #

  module PackStream
    
    # Empty Module. 
    # classes can include it to indicate that they implement the structure interface
    # This requires the class to have a signature method, that returns the signature byte to use
    # and a fields methods that returns an array of the fields the structure contains
    #
    module Structure
    end

    class BasicStruct < Struct.new(:signature, :fields)
      include Structure
      def self.from_pack_stream(signature, fields)
        new(signature, fields)
      end
    end

    class << self
      NULL = "\xC0".dup.force_encoding('BINARY')
      TRUE = "\xC3".dup.force_encoding('BINARY')
      FALSE = "\xC2".dup.force_encoding('BINARY')

      def pack(*values)
        values.inject("".dup.force_encoding('BINARY')) do |buffer, value|
          pack_internal(buffer, value)
        end
      end

      def unpack(bytestring, registry: nil)
        Unpacker.new(bytestring, registry: registry).enumerator
      end

      private

      def pack_internal(buffer, value)
        case value
        when Integer then encode_integer(value, buffer)
        when Float then buffer << ["\xC1", value].pack('AG')
        when String then encode_string(value, buffer)
        when Symbol then encode_string(value.to_s, buffer)
        when Array then encode_array(value, buffer)
        when Hash then encode_hash(value, buffer)
        when Structure then encode_structure(value, buffer)
        when nil then buffer << NULL
        when true then buffer << TRUE
        when false then buffer << FALSE
        else
          raise ArgumentError, "value #{value} cannot be packstreamed"
        end
        buffer
      end

      def encode_integer(value, buffer)
        data = if -0x10 <= value && value < 0x80
          [value].pack('c')
        elsif -0x80 <= value  && value < 0x80
          ["\xC8", value].pack('Ac')
        elsif  -0x8000 <= value && value < 0x8000
          ["\xC9", value].pack('As>')
        elsif  -0x8000_0000 <= value && value < 0x8000_0000
          ["\xCA", value].pack('Al>')
        elsif  -0x8000_0000_0000_0000 <= value && value < 0x8000_0000_0000_0000
          ["\xCB", value].pack('Aq>')
        else
          raise ArgumentError, "integer #{value} is out of range"
        end
        buffer << data
      end

      def encode_array(array, buffer)
        length = array.length
        leader = case length
        when 0..15 then [0x90 + length].pack('C')
        when 16..255 then [0xD4, length].pack('CC')
        when 256..65535 then [0xD5, length].pack('CS>')
        when 65536...0x100000000 then [0xD6, length].pack('CL>')
        else 
          raise ArgumentError, "Array is too long #{length}"
        end
        buffer << leader
        array.each { |item| pack_internal(buffer, item)}
      end

      def encode_hash(hash, buffer)
        size = hash.size
        leader = case size
        when 0..15 then [0xA0 + size].pack('C')
        when 16..255 then [0xD8, size].pack('CC')
        when 256..65535 then [0xD9, size].pack('CS>')
        when 65536...0x100000000 then [0xDA, size].pack('CL>')
        else 
          raise ArgumentError, "Hash is too big #{size}"
        end
        buffer << leader
        hash.each do |key, value| 
          pack_internal(buffer, key)
          pack_internal(buffer, value)
        end
      end

      def encode_string(string, buffer)
        encoded = string.encode('utf-8')
        bytesize = encoded.bytesize
        leader = case bytesize
        when 0..15 then [0x80 + bytesize].pack('C')
        when 16..255 then [0xD0, bytesize].pack('CC')
        when 256..65535 then [0xD1, bytesize].pack('CS>')
        when 65536...0x100000000 then [0xD2, bytesize].pack('CL>')
        else 
          raise ArgumentError, "String is too long (#{bytesize})"
        end
        buffer << leader
        buffer << encoded.force_encoding('BINARY')
      end

      def encode_structure(struct, buffer)
        fields = struct.fields
        size = fields.size
        leader = case size
        when 0..15 then [0xB0 + size, struct.signature].pack('CC')
        when 16..255 then [0xDC, size, struct.signature].pack('CCC')
        when 256..65535 then [0xDD, size, struct.signature].pack('CS>C')
        else
          raise ArgumentError, "structure has too many fields (#{size})"
        end
        buffer << leader
        fields.each {|item| pack_internal(buffer, item)}
      end
    end
  end

  class Unpacker
    def initialize(data, registry: nil)
      @data = data
      @offset = 0
      @registry = registry
    end

    def enumerator
      Enumerator.new do |y|
        loop do
          y << fetch_next_field
          break if at_end?
        end
      end
    end

    def fetch_next_field
      marker = get_scalar :uint8
      if marker < 0x80 then marker
      elsif marker >= 0xF0 then marker - 0x100 #the small negative ones - convert to signed byte
      elsif marker == 0xC0 then nil
      elsif marker == 0xC1 then get_scalar :float
      elsif marker == 0xC2 then false
      elsif marker == 0xC3 then true
      #ints
      elsif marker == 0xC8 then get_scalar :int8
      elsif marker == 0xC9 then get_scalar :int16
      elsif marker == 0xCA then get_scalar :int32
      elsif marker == 0xCB then get_scalar :int64
      #strings
      elsif marker >= 0x80 && marker <= 0x8F then get_string(marker & 0x0F)
      elsif marker == 0xD0 then get_string(get_scalar(:uint8))
      elsif marker == 0xD1 then get_string(get_scalar(:uint16))
      elsif marker == 0xD2 then get_string(get_scalar(:uint32))
      #lists
      elsif marker >= 0x90 && marker <= 0x9F then get_list(marker & 0x0F)
      elsif marker == 0xD4 then get_list(get_scalar(:uint8))
      elsif marker == 0xD5 then get_list(get_scalar(:uint16))
      elsif marker == 0xD6 then get_list(get_scalar(:uint32))
      #maps
      elsif marker >= 0xA0 && marker <= 0xAF then get_map(marker & 0x0F)
      elsif marker == 0xD8 then get_map(get_scalar(:uint8))
      elsif marker == 0xD9 then get_map(get_scalar(:uint16))
      elsif marker == 0xDA then get_map(get_scalar(:uint32))
      #structs
      elsif marker >= 0xB0 && marker <= 0xBF then get_struct(marker & 0x0F)
      elsif marker == 0xDC then get_struct(get_scalar(:uint8))
      elsif marker == 0xDD then get_struct(get_scalar(:uint16))
      else
        raise ArgumentError, "Unknown marker #{marker.to_s(16)}"
      end
    end      
    private

    TYPES = {
      :int8 => 'c',
      :int16 => 's>',
      :int32 => 'l>',
      :int64 => 'q>',
      :uint8 => 'C',
      :uint16 => 'S>',
      :uint32 => 'L>',
      :uint64 => 'Q>',
      :float => 'G'
    }
    SIZES = {
      :int8 => 1,
      :int16 => 2,
      :int32 => 4,
      :int64 => 8,
      :uint8 => 1,
      :uint16 => 2,
      :uint32 => 4,
      :uint64 => 8,
      :float => 8
    }

    def get_string(length)
      data = @data.byteslice(@offset, length).force_encoding('UTF-8')
      raise ArgumentError, "end of string data missing, wanted #{length} bytes, found #{data.length}" if data.length < length
      @offset+= length
      data
    end

    def get_list(length)
      length.times.collect { fetch_next_field }
    end

    def get_map(length)
      length.times.each_with_object({}) { |_, hash| hash[fetch_next_field]=fetch_next_field }
    end

    def get_struct(length)
      signature = get_scalar(:int8)
      klass = (@registry && @registry[signature]) || Bolt::PackStream::BasicStruct 
      klass.from_pack_stream(signature, get_list(length))
    end

    def get_scalar(type)
      length = SIZES.fetch(type)
      data = @data.byteslice(@offset, length)
      raise ArgumentError, "end of scalar data missing, wanted #{length} bytes, found #{data.length}" if data.length < length
      scalar = data.unpack(TYPES.fetch(type)).first
      @offset += length
      scalar
    end


    def at_end?
      @offset == @data.bytesize
    end
  end
end