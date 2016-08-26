module Bolt
  module PackStream
    
    module Structure
      def signature
        raise UnimplementedError
      end

      def fields
        raise UnimplementedError
      end
    end

    class << self
      def pack(*values)
        values.map do |value|
          case value
          when Integer then encode_integer(value)
          when Float then ["\xC1", value].pack('AG')
          when String then encode_string(value)
          when Symbol then encode_string(value.to_s)
          when Array then encode_array(value)
          when Hash then encode_hash(value)
          when Structure then encode_structure(value)
          when nil then "\xC0"
          when true then "\xC3"
          when false then "\xC2"
          else
            raise ArgumentError, "value #{value} cannot be packstreamed"
          end
        end.join.force_encoding('BINARY')
      end

      def unpack(bytestring)
        Unpacker.new(bytestring).enumerator
      end

      private

      def encode_integer(value)
        if -0x10 <= value && value < 0x80
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
      end

      def encode_array(array)
        length = array.length
        leader = case length
        when 0..15 then [0x90 + length].pack('C')
        when 16..255 then [0xD4, length].pack('CC')
        when 256..65535 then [0xD5, length].pack('CS>')
        when 65536...0x100000000 then [0xD6, length].pack('CL>')
        else 
          raise ArgumentError, "Array is too long #{length}"
        end
        array.inject(leader) {|buffer, item| buffer << pack(item)}
      end

      def encode_hash(hash)
        size = hash.size
        leader = case size
        when 0..15 then [0xA0 + size].pack('C')
        when 16..255 then [0xD8, size].pack('CC')
        when 256..65535 then [0xD9, size].pack('CS>')
        when 65536...0x100000000 then [0xDA, size].pack('CL>')
        else 
          raise ArgumentError, "Hash is too big #{size}"
        end
        hash.inject(leader) do |buffer, (key, value)| 
          buffer << pack(key)
          buffer << pack(value)
        end
      end

      def encode_string(string)
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
        leader + encoded.force_encoding('BINARY')
      end

      def encode_structure(struct)
        fields = struct.fields
        size = fields.size
        leader = case size
        when 0..15 then [0xB0 + size, struct.signature].pack('CC')
        when 16..255 then [0xDC, size, struct.signature].pack('CCC')
        when 256..65535 then [0xDD, size, struct.signature].pack('CS>C')
        else
          raise ArgumentError, "structure has too many fields (#{size})"
        end
        fields.inject(leader) {|buffer, item| buffer << pack(item)}
      end
    end
  end

  class Unpacker
    def initialize(data)
      @data = data
      @offset = 0
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
      case marker
      when 0xC0 then nil
      when 0xC3 then true
      when 0xC2 then false
      when 0xC8 then get_scalar :int8
      when 0xC9 then get_scalar :int16
      when 0xCA then get_scalar :int32
      when 0xCB then get_scalar :int64
      when 0x80..0x8F then get_string(marker & 0x0F)
      when 0xD0 then get_string(get_scalar(:uint8))
      when 0xD1 then get_string(get_scalar(:uint16))
      when 0xD2 then get_string(get_scalar(:uint32))
      else
        return marker
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
    }

    def get_string(length)
      data = @data.byteslice(@offset, length).force_encoding('UTF-8')
      raise ArgumentError, "end of string data missing, wanted #{length} bytes, found #{data.length}" if data.length < length
      @offset+= length
      data
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