require "bolt/version"
require 'bolt/pack_stream'
module Bolt
end

require 'bolt/bolt_native' unless ENV['BOLT_DISABLE_NATIVE_EXTENSIONS']=='1'