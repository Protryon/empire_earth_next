# R6 WON leaves (D8)

The lane's state, moved from RUST_PLAN's Progress table. Each round rewrites this file to the
current state; the round's story goes in PORT_LEARNINGS.md.

Done (`ee_won`, branch `rust-r6-won`): the message buffers' format and WON's text conversions, CRC-16, MD5, the random generator, Blowfish, big integers, ElGamal, the auth blocks and the CD key logic, behind C++ forwarders (BigInteger, Random, IntegerExtractor deleted). The clang-built C++ MD5Digest and Karatsuba were wrong (PORT_LEARNINGS "R6")
