#!/usr/bin/env ruby

# Assumes RSA, expects file to look like the output of:
#   .../sbin/dnssec-keygen -T KEY -a rsasha1 -b 1024 -n USER tjktest

require 'openssl'
require 'base64'

file = ARGV[0]

mod = 0
e = 0
d = 0

File.readlines(file).each do |line|
  if line =~ /^Modulus:/ then
    mod_str = line.chomp.split(" ")[1]
    mod = Base64.decode64(mod_str).unpack("H*")
    mod = OpenSSL::BN.new(mod[0], 16)
  end
  if line =~ /^PublicExponent:/ then
    e_str = line.chomp.split(" ")[1]
    e = Base64.decode64(e_str).unpack("H*")
    e = OpenSSL::BN.new(e[0], 16)
  end
  if line =~ /^PrivateExponent:/ then
    d_str = line.chomp.split(" ")[1]
    d = Base64.decode64(d_str).unpack("H*")
    d = OpenSSL::BN.new(d[0], 16)
  end
end

size = mod.num_bits

k = OpenSSL::PKey::RSA.new(size)
k.n = mod
k.e = e
k.d = d
public = k.public_key()
puts k.to_pem()
puts public.to_pem()
