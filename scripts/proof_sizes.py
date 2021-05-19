import math

hash_len = 32
file_size = 1024*1024*1024 #1GB
print("File size", file_size / (1024*1024*1024), "GB")

print()
print("=======Merkle Tree======")

block_size = 64*1024 #64KB
print("Block size", block_size, "Bytes")

s_anc = 256 # anchor transcript size
amp_factor = 25

tree_height = math.log(file_size, 2)
por_size = block_size + tree_height * hash_len
print("PoRet size:", por_size, "Bytes")

def compute_for_MT(num_chals, num_anchors, num_intervals):
    size_per_proof = amp_factor * (num_chals *  por_size) + (amp_factor + 1) * (s_anc)
    return size_per_proof * num_anchors * num_intervals

num_chals = 100
tot_size = compute_for_MT(num_chals, 1, 1)
print("Size per proof (one anchor):", tot_size / (1024*1024), "MB")

# threshold = 5
without_sampling = compute_for_MT(num_chals, 11, 20)
print("Size per epoch (no sampling):", without_sampling / (1024*1024*1024), "GB")

with_sampling = compute_for_MT(num_chals / 10, 5, 5)
print("Size per epoch (with sampling):", with_sampling / (1024*1024), "MB")


print()
print("=======Shacham Waters======")

s = 1000
sec_param = 20

sw_por_size = (s + 1) * sec_param

def compute_for_SW(num_anchors, num_intervals):
    total_transcript_size = (amp_factor + 1) * s_anc * num_anchors * num_intervals
    return total_transcript_size + sw_por_size

without_sampling = compute_for_SW(11, 20)

print("Size per epoch (no sampling)", without_sampling / (1024*1024), "MB" )

with_sampling = compute_for_SW(5, 5)
print("Size per epoch (with sampling)", with_sampling / (1024*1024), "MB" )