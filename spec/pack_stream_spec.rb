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
        expect(Bolt::PackStream.pack(-9_223_372_036_854_775_808)).to match_hex('CA:80:00:00:00:00:00:00:00')
        expect(Bolt::PackStream.pack(9_223_372_036_854_775_807)).to match_hex('CA:7F:FF:FF:FF:FF:FF:FF:FF')
      end

      it 'raises on out of range integer' do
        expect {Bolt::PackStream.pack(2**65)}.to raise_error(ArgumentError)
      end
    end

  end
end