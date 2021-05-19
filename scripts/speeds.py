adv_speed = (2.0/3)*299.792458
honest_speed = adv_speed / 3
t_proc = 10
t_startup = 2
t_com = 5

distance = 305.84

print((2 * distance) / honest_speed)

t_ping = ((2 * distance) / honest_speed) + t_startup + t_proc

Delta = t_ping + t_com
print("Delta:", Delta)

granularity = Delta * adv_speed / 2
print("granularity:", granularity)
