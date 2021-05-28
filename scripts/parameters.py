from mpmath import *
mp.dps = 1000

num_challenges = mpf(170)
grinding_cap = mpf(2**40)
sec_par = mpf(80)

code_rate = mpf(0.33)

downloadable_ratio = mpf(0.001)

import math

stored_frac = (1 - (1 - 2 ** (-sec_par) ) ** (1 / grinding_cap) ) ** (1 / num_challenges)

stored_frac_2 = (2 ** (-sec_par) / grinding_cap) ** (1 / num_challenges)

# 2 ** ((- sec_par)/ (grinding_cap * num_challenges))

print("Stored fraction due to grinding is only", stored_frac)

print("Stored fraction due to grinding is only", stored_frac_2)

print(stored_frac > stored_frac_2)

unanswerable_frac = (code_rate + downloadable_ratio  + (1 - stored_frac)) ** num_challenges

print("Unanswerable fraction log base 2 is", math.log2(unanswerable_frac)) # This should be negligible in the security parameter

# print("Constraint: ", (kappa_file_frac <= (stored_frac / code_rate) - 1)) (This constraint holds automatically if above holds)