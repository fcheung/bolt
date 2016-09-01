require "bolt/version"
require 'bolt/pack_stream'
module Bolt
  #
  # Returns true if native extensions were loaded
  #
  def self.native_extensions_loaded?
    false
  end
end

require 'bolt/bolt_native' unless ENV['BOLT_DISABLE_NATIVE_EXTENSIONS']=='1'