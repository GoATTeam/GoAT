from Crypto.Signature.pkcs1_15 import PKCS115_SigScheme
from Crypto.Hash import SHA1
from Crypto.PublicKey import RSA

import sys

if (len(sys.argv) != 4):
    print("Usage: ./verify pubkey msg sig")
    exit()

key = RSA.import_key(open(sys.argv[1]).read())
# print(key)

message = open(sys.argv[2], 'rb').read()
# print(message.hex())

signature = open(sys.argv[3], 'rb').read()
# print(signature.hex())

h = SHA1.new(message)

verifier = PKCS115_SigScheme(key)
try:
    verifier.verify(h, signature)
    print("Signature is valid.")
except:
    print("Signature is invalid.")