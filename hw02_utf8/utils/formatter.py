
res = []
with open('in.txt', 'r') as f:
    items = f.read().splitlines()
    for item in items:
        res.append(item)
a = ', '.join(res)
print(a)
print(len(res))