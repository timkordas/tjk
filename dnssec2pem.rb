#!/usr/bin/env ruby

# Assumes RSA, expects file to look like the output of:
#   .../sbin/dnssec-keygen -T KEY -a rsasha1 -b 1024 -n USER tjktest

require 'openssl'
require 'base64'

file = ARGV[0]

mod = 0
e = 0
d = 0
p = 0
q = 0
dmodp1 = 0
dmodq1 = 0
iqmodp = 0

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
  if line =~ /^Prime1:/ then
    p_str = line.chomp.split(" ")[1]
    p = Base64.decode64(p_str).unpack("H*")
    p = OpenSSL::BN.new(p[0], 16)
  end
  if line =~ /^Prime2:/ then
    q_str = line.chomp.split(" ")[1]
    q = Base64.decode64(q_str).unpack("H*")
    q = OpenSSL::BN.new(q[0], 16)
  end
  if line =~ /^Exponent1:/ then
    dmodp1_str = line.chomp.split(" ")[1]
    dmodp1 = Base64.decode64(dmodp1_str).unpack("H*")
    dmodp1 = OpenSSL::BN.new(dmodp1[0], 16)
  end
  if line =~ /^Exponent2:/ then
    dmodq1_str = line.chomp.split(" ")[1]
    dmodq1 = Base64.decode64(dmodq1_str).unpack("H*")
    dmodq1 = OpenSSL::BN.new(dmodq1[0], 16)
  end
  if line =~ /^Coefficient:/ then
    iqmodp_str = line.chomp.split(" ")[1]
    iqmodp = Base64.decode64(iqmodp_str).unpack("H*")
    iqmodp = OpenSSL::BN.new(iqmodp[0], 16)
  end
end

size = mod.num_bits

k = OpenSSL::PKey::RSA.new(size)

k.n = mod
k.e = e
k.d = d
k.p = p
k.q = q
k.dmp1 = dmodp1
k.dmq1 = dmodq1
k.iqmp = iqmodp

puts k.to_text()

public = k.public_key()
puts k.to_pem()
puts public.to_pem()
