require 'spec_helper'

describe Bolt::PackStream do

  describe 'pack' do
    describe 'encoding' do
      it 'returns binary strings' do
        expect(Bolt::PackStream.pack(nil).encoding.names).to include('BINARY')
      end
    end

    describe 'booleans' do
      it 'serializes true as 0xC3' do
        expect(Bolt::PackStream.pack(true)).to match_hex('C3')
      end

      it 'serializes false as 0xC2' do
        expect(Bolt::PackStream.pack(false)).to match_hex('C2')
      end
    end

    describe 'null' do
      it 'serializes nil as 0xC0' do
        expect(Bolt::PackStream.pack(nil)).to match_hex('C0')
      end
    end

    describe 'floats' do
      it 'serializes values as C1 followed by 8 bytes of IEEE 754' do
        expect(Bolt::PackStream.pack(6.283185307179586)).to match_hex('C1:40:19:21:FB:54:44:2D:18')
      end
    end

    describe 'strings' do
      it 'serializes symbols as strings' do
        expect(Bolt::PackStream.pack(:foo)).to eq(Bolt::PackStream.pack('foo'))
      end
        
      it 'serializes <= 15 bytes to 80..8F followed by text' do
        expect(Bolt::PackStream.pack('é')).to match_hex('82:C3:A9')
        expect(Bolt::PackStream.pack('')).to match_hex('80')
        expect(Bolt::PackStream.pack('A')).to match_hex('81:41')
      end
      it 'serialises 16 to 255 bytes as D0 followed 1 unsigned byte length followed by text' do
        expect(Bolt::PackStream.pack('ABCDEFGHIJKLMNOPQRSTUVWXYZ')).to match_hex('D0:1A:41:42:43:44:45:46:47:48:49:4A:4B:4C:4D:4E:4F:50:51:52:53:54:55:56:57:58:59:5A')
      end

      it 'serialises 256 to 65535 bytes as D1 followed big endian 2 unsigned byte length followed by text' do
        expect(Bolt::PackStream.pack('A'*256)).to match_hex('D1:01:00' + ':41' * 256 )
      end

      it 'serialises 65536 to  4294967295 bytes as D2 followed big endian 4 unsigned byte length followed by text' do
        expect(Bolt::PackStream.pack('A'*65536)).to match_hex('D2:00:01:00:00' + ':41' * 65536 )
      end

      #when these methods are implemented in C we can't
      #stub the bytesize method
      unless Bolt.native_extensions_loaded?
        it "raises error if length >= 2^32 " do
          input = 'dummy'
          expect(input).to receive(:bytesize).and_return(2**32)
          allow(input).to receive(:encode).and_return(input)
          expect {Bolt::PackStream.pack(input)}.to raise_error(RangeError)
        end
      end

      it 'converts non utf 8 to utf 8' do
        expect(Bolt::PackStream.pack("\xE9".force_encoding("ISO-8859-1"))).to match_hex("82:C3:A9")
      end

      it 'passes examples' do
        aggregate_failures do 
          expect(Bolt::PackStream.pack('ABCDEFGHIJKLMNOPQRSTUVWXYZ')).to match_hex('D0:1A:41:42:43:44:45:46:47:48:49:4A:4B:4C:4D:4E:4F:50:51:52:53:54:55:56:57:58:59:5A')
          expect(Bolt::PackStream.pack('Größenmaßstäbe')).to match_hex('D0:12:47:72:C3:B6:C3:9F:65:6E:6D:61:C3:9F:73:74:C3:A4:62:65')
        end
      end
    end

    describe 'lists' do
      it 'serializes <= 15 items to 90..9F followed by items' do
        expect(Bolt::PackStream.pack([])).to match_hex('90')
        expect(Bolt::PackStream.pack([1,2,3])).to match_hex('93:01:02:03')
        expect(Bolt::PackStream.pack([1]*15)).to match_hex('9F'+ (':01'*15))
      end

      it 'serialises 16 to 255 items as D4 followed 1 unsigned byte length followed by items' do
        expect(Bolt::PackStream.pack((0..39).to_a)).to match_hex('D4:28:00:01:02:03:04:05:06:07:08:09:0A:0B:0C:0D:0E:0F:10:11:12:13:14:15:16:17:18:19:1A:1B:1C:1D:1E:1F:20:21:22:23:24:25:26:27')
      end

      it 'serialises 256 to 65535 items as D5 followed big endian 2 unsigned byte length followed by items' do
        expect(Bolt::PackStream.pack([1]*256)).to match_hex('D5:01:00' + ':01' * 256 )
      end

      it 'serialises 65536 to  4294967295 items as D6 followed big endian 4 unsigned byte length followed by items' do
        expect(Bolt::PackStream.pack([1]*65536)).to match_hex('D6:00:01:00:00' + ':01' * 65536 )
      end

      #when these methods are implemented in C we can't
      #stub the length method
      unless Bolt.native_extensions_loaded?
        it "raises error if length >= 2^32 " do
          input = []
          expect(input).to receive(:length).and_return(2**32)
          expect {Bolt::PackStream.pack(input)}.to raise_error(RangeError)
        end
      end

      it 'allows heterogenous lists' do
        expect(Bolt::PackStream.pack([1, true, 3.14, "fünf"])).to match_hex('94:01:C3:C1:40:09:1E:B8:51:EB:85:1F:85:66:C3:BC:6E:66')
      end
    end

    describe 'maps' do
      it 'serializes <= 15 items to A0..AF followed by key value pairs' do
        expect(Bolt::PackStream.pack({})).to match_hex('A0')
        expect(Bolt::PackStream.pack({one: 'eins'})).to match_hex('A1:83:6F:6E:65:84:65:69:6E:73')
      end

      it 'serialises 16 to 255 items as D8 followed 1 unsigned byte length followed by key value pairs in any order' do
        map = ('A'..'Z').zip(1..26).to_h
        #our spec actually asserts order too
        expect(Bolt::PackStream.pack(map)).to match_hex('D8:1A:81:41:01:81:42:02:81:43:03:81:44:04:81:45:05:81:46:06:81:47:07:81:48:08:81:49:09:81:4A:0A:81:4B:0B:81:4C:0C:81:4D:0D:81:4E:0E:81:4F:0F:81:50:10:81:51:11:81:52:12:81:53:13:81:54:14:81:55:15:81:56:16:81:57:17:81:58:18:81:59:19:81:5A:1A')
      end

      it 'serialises 256 to 65535 items as D9 followed big endian 2 unsigned byte length followed by key value pairs' do
        map = (1..256).zip(['1']*256).to_h
        expect(Bolt::PackStream.pack(map)[0,9]).to match_hex('D9:01:00:01:81:31:02:81:31')
      end

      it 'serialises 65536 to  4294967295 items as DA followed big endian 4 unsigned byte length followed by key value pairs' do
        map = (1..65536).zip(['1']*65536).to_h
        expect(Bolt::PackStream.pack(map)[0,11]).to match_hex('DA:00:01:00:00' + ':01:81:31:02:81:31' )
      end

      #when these methods are implemented in C we can't
      #stub the length method
      unless Bolt.native_extensions_loaded?
        it "raises error if size >= 2^32 " do
          input = {}
          expect(input).to receive(:size).and_return(2**32)
          expect {Bolt::PackStream.pack(input)}.to raise_error(RangeError)
        end
      end

    end

    describe 'integers' do
      it 'serializes value from -16 to 127 into 1 byte' do
        expect(Bolt::PackStream.pack(1)).to match_hex('01')
        expect(Bolt::PackStream.pack(-16)).to match_hex('F0')
        expect(Bolt::PackStream.pack(127)).to match_hex('7F')
      end

      it 'serializes value from -128 to -17 into C8 plus 1 signed byte' do
        expect(Bolt::PackStream.pack(-128)).to match_hex('C8:80')
        expect(Bolt::PackStream.pack(-17)).to match_hex('C8:EF')
      end

      it 'serializes value from -32768 to 32767 into C9 plus 2 signed big endian bytes' do
        expect(Bolt::PackStream.pack(-32768)).to match_hex('C9:80:00')
        expect(Bolt::PackStream.pack(1234)).to match_hex('C9:04:D2')
        expect(Bolt::PackStream.pack(32767)).to match_hex('C9:7F:FF')
      end


      it 'serializes value from -2147483648 to 2147483647 into CA plus 4 signed big endian bytes' do
        expect(Bolt::PackStream.pack(-2_147_483_648)).to match_hex('CA:80:00:00:00')
        expect(Bolt::PackStream.pack(2_147_483_647)).to match_hex('CA:7F:FF:FF:FF')
      end


      it 'serializes value from -9_223_372_036_854_775_808 to 9_223_372_036_854_775_807 into CB plus 8 signed big endian bytes' do
        expect(Bolt::PackStream.pack(-9_223_372_036_854_775_808)).to match_hex('CB:80:00:00:00:00:00:00:00')
        expect(Bolt::PackStream.pack(9_223_372_036_854_775_807)).to match_hex('CB:7F:FF:FF:FF:FF:FF:FF:FF')
      end

      it 'raises on out of range integer' do
        expect {Bolt::PackStream.pack(2**65)}.to raise_error(RangeError)
      end
    end

    describe 'structures' do

      it 'serializes a structure with <= 15 fields as B0..BF, signature byte, fields' do
        a = Struct.new(:a, :b, :c) do
          include Bolt::PackStream::Structure
          def signature
            100
          end

          def fields
            to_a
          end
        end

        expect(Bolt::PackStream.pack(a.new(1,2,"3"))).to match_hex('B3:64:01:02:81:33')
      end

      it 'serializes a structure with <= 255 fields as DC, unsigned byte length , signature byte, fields' do
        a = Struct.new(*('a'..'p').to_a.collect(&:to_sym)) do
          include Bolt::PackStream::Structure
          def signature
            100
          end

          def fields
            to_a
          end
        end
        expect(Bolt::PackStream.pack(a.new(*(1..16)))).to match_hex('DC:10:64:01:02:03:04:05:06:07:08:09:0A:0B:0C:0D:0E:0F:10')

      end

      it 'serializes a structure with <= 65535 fields as DD, 2 big endian unsigned byte length  , signature byte, fields' do
        a = Class.new do
          include Bolt::PackStream::Structure
          def signature
            100
          end

          def fields
            [1]*256
          end
        end
        expect(Bolt::PackStream.pack(a.new)).to match_hex('DD:01:00:64' + ':01'*256)

      end

      it 'raises on structure with > 65535 fields' do
        a = Class.new do
          include Bolt::PackStream::Structure
          def fields
            (1..65536).to_a
          end
        end
        expect {Bolt::PackStream.pack(a.new)}.to raise_error(RangeError)
      end
    end

    describe 'multiple items' do
      it 'concatenates their data' do
        expect(Bolt::PackStream.pack(1, 'abc', [1.0])).to eq(
          Bolt::PackStream.pack(1) +
          Bolt::PackStream.pack('abc') +
          Bolt::PackStream.pack([1.0])
          )
      end
    end
  end

  describe Bolt::ByteBuffer do
    describe 'to_a' do
      it 'returns an array of the unpacked values' do
        expect(Bolt::ByteBuffer.new("\x00\x7F\xF0\xC8\x80\xC9\x80\x00\xC9\x7F\xFF\xCA\x80\x00\x00\x00\xCB\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF").to_a).to eq([
          0, 127,
          -16,
          -128,
          -32768,
          32767,
          -2_147_483_648,
          9_223_372_036_854_775_807

        ])
      end
    end
  end

  describe 'unpack' do
    it 'rejects unknown marker bytes' do
      expect {Bolt::PackStream.unpack("\xCC").next}.to raise_error(ArgumentError)
    end

    describe 'scalars' do
      it 'unpacks true' do
        expect(Bolt::PackStream.unpack("\xC3").next).to eq(true)
      end

      it 'unpacks false' do
        expect(Bolt::PackStream.unpack("\xC2").next).to eq(false)
      end

      it 'unpacks nil' do
        expect(Bolt::PackStream.unpack("\xC0").next).to eq(nil)
      end

      it 'unpacks integers' do
        expect(Bolt::PackStream.unpack("\x00\x7F\xF0\xC8\x80\xC9\x80\x00\xC9\x7F\xFF\xCA\x80\x00\x00\x00\xCB\x7F\xFF\xFF\xFF\xFF\xFF\xFF\xFF").to_a).to eq([
          0, 127,
          -16,
          -128,
          -32768,
          32767,
          -2_147_483_648,
          9_223_372_036_854_775_807

        ])
      end

      it 'allows integers formatted in an oversized container' do
        expect(Bolt::PackStream.unpack("\xCB\x00\x00\x00\x00\x00\x00\x00\x20").next).to eq(32)
      end

      it 'unpacks floats' do
        expect(Bolt::PackStream.unpack("\xC1\x40\x19\x21\xFB\x54\x44\x2D\x18").next).to eq(6.283185307179586)
      end
    end


    describe 'lists' do
      it 'reads empty list' do
        expect(Bolt::PackStream.unpack("\x90").next).to eq([])
      end

      it 'reads lists with combined marker and byte length' do
        expect(Bolt::PackStream.unpack("\x91\x01").next).to eq [1]
      end

      it 'reads lists with 1 byte length' do
        expect(Bolt::PackStream.unpack("\xD4\x03\x01\x85\x48\x65\x6c\x6c\x6f\x02").next).to eq [1, "Hello", 2]
      end

      it 'reads lists with 2 byte length' do
       expect(Bolt::PackStream.unpack("\xD5\x00\x03\x01\x85\x48\x65\x6c\x6c\x6f\x02").next).to eq [1, "Hello", 2]
      end   

      it 'reads lists with 4 byte length' do
       expect(Bolt::PackStream.unpack("\xD6\x00\x00\x00\x03\x01\x85\x48\x65\x6c\x6c\x6f\x02").next).to eq [1, "Hello", 2]
      end   

      it 'handles nested lists' do
        expect(Bolt::PackStream.unpack("\x92\x91\x91\x91\x85\x48\x65\x6c\x6c\x6f\x01").next).to eq([[[['Hello']]],1])
      end

      it 'raises if length is longer than the buffer' do
        expect { Bolt::PackStream.unpack("\x9F").next}.to raise_error(ArgumentError)
      end
    end

    describe 'maps' do
      it 'reads empty map' do
        expect(Bolt::PackStream.unpack("\xA0").next).to eq({})
      end

      it 'reads maps with combined marker and byte length' do
        expect(Bolt::PackStream.unpack("\xA1\x81\x31\x01").next).to eq({"1" => 1})
      end


      it 'reads maps with 1 byte length' do
        expect(Bolt::PackStream.unpack("\xD8\x05\x81\x41\x01\x81\x42\x02\x81\x43\x03\x81\x44\x04\x81\x45\x05").next).to eq('A' => 1, 'B' => 2, 'C' => 3 , 'D' => 4, 'E' => 5)
      end

      it 'reads maps with 2 byte length' do
        expect(Bolt::PackStream.unpack("\xD9\x00\x05\x81\x41\x01\x81\x42\x02\x81\x43\x03\x81\x44\x04\x81\x45\x05").next).to eq('A' => 1, 'B' => 2, 'C' => 3 , 'D' => 4, 'E' => 5)
      end   

      it 'reads maps with 4 byte length' do
        expect(Bolt::PackStream.unpack("\xDA\x00\x00\x00\x05\x81\x41\x01\x81\x42\x02\x81\x43\x03\x81\x44\x04\x81\x45\x05").next).to eq('A' => 1, 'B' => 2, 'C' => 3 , 'D' => 4, 'E' => 5)
      end   

      it 'handles nested maps' do
        expect(Bolt::PackStream.unpack("\xA2\x1\x92\x2\x3\x2\x3").next).to eq({1 => [2,3], 2 => 3})
      end

      it 'raises if length is longer than the buffer' do
        expect { Bolt::PackStream.unpack("\xAF").next}.to raise_error(ArgumentError)
      end
    end

    describe 'strings' do
      it 'sets encoding of strings to utf8' do
        expect(Bolt::PackStream.unpack("\x85\x48\x65\x6c\x6c\x6f").next.encoding.name).to eq('UTF-8')
      end
      it 'reads strings with combined marker and byte length' do
        expect(Bolt::PackStream.unpack("\x85\x48\x65\x6c\x6c\x6f\xC0").next).to eq('Hello')
      end

      it 'reads empty string' do
        expect(Bolt::PackStream.unpack("\x80\xC0").next).to eq('')
      end

      it 'reads strings with 1 byte length' do
        expect(Bolt::PackStream.unpack("\xD0\x05\x48\x65\x6c\x6c\x6f\xC0").next).to eq('Hello')
      end

      it 'reads strings with 2 byte length' do
        expect(Bolt::PackStream.unpack("\xD1\x00\x05\x48\x65\x6c\x6c\x6f\xC0").next).to eq('Hello')
      end

      it 'reads strings with 4 byte length' do
        expect(Bolt::PackStream.unpack("\xD2\x00\x00\x00\x05\x48\x65\x6c\x6c\x6f\xC0").next).to eq('Hello')
      end

      it 'raises if length is longer than buffer' do
        expect { Bolt::PackStream.unpack("\x8F").next}.to raise_error(ArgumentError)
      end
    end

    describe 'structs' do
      it 'reads empty structs' do
        expect(Bolt::PackStream.unpack("\xB0\x01").next).to eq(Bolt::PackStream::BasicStruct.new(1, []))
      end

      it 'reads structs with combined marker and length' do
        expect(Bolt::PackStream.unpack("\xB2\x01\x85\x48\x65\x6c\x6c\x6f\xA0").next).to eq(Bolt::PackStream::BasicStruct.new(1, ["Hello", {}]))
      end

      it 'reads structs with 1 byte length' do
        expect(Bolt::PackStream.unpack("\xDC\x10\x7F\x01\x02\x03\x04\x05\x06\x07\x08\x09\x00\x01\x02\x03\x04\x05\x06").next).to eq(
          Bolt::PackStream::BasicStruct.new(0x7f,[1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6])
        )

      end

      it 'reads structs with 2 byte length'do
        expect(Bolt::PackStream.unpack("\xDD\x00\x10\x7F\x01\x02\x03\x04\x05\x06\x07\x08\x09\x00\x01\x02\x03\x04\x05\x06").next).to eq(
          Bolt::PackStream::BasicStruct.new(0x7f,[1,2,3,4,5,6,7,8,9,0,1,2,3,4,5,6])
        )
      end

      context 'with a registry of signatures to classes' do
        it 'reads structs as the specified class' do
          a = Class.new(Struct.new(:signature, :fields)) do
            def self.from_pack_stream(signature, fields)
              new(signature, fields)
            end
          end
          b = Class.new(Struct.new(:signature, :fields)) do
            def self.from_pack_stream(signature, fields)
              new(signature, fields)
            end
          end

          expect(Bolt::PackStream.unpack("\xB1\x01\x81\x41\xB1\x02\x81\x42", registry: {1 => a, 2 => b}).to_a).to eq([
            a.new(1, ["A"]), b.new(2, ["B"])
          ])

        end
      end
    end
  end

end