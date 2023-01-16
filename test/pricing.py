
import itertools
import collections

A = list("".join(sorted(t)) for t in itertools.product('01234', repeat=8))

B = sorted(set(A))



def getprice(s, pricing):
    c = collections.Counter(s)
    return sum(c.get(str(v), 0)*pricing[v] for v in range(5))


#p0 = [4, 3, 2, 1, 0]
#p1 = [0, 1, 2, 3, 4]
#
#for s in B:
#    price0 = getprice(s, p0)
#    price1 = getprice(s, p1)
#    print(f'{s} -> {price0} != {price1}')
#    assert price0 != price1


#p0 = [4, 3, 2, 1, 0]
#p1 = [3, 2, 1, 0, 4]
#p2 = [2, 1, 0, 4, 3]
#p3 = [1, 0, 4, 3, 2]
#p4 = [0, 4, 3, 2, 1]

#     " ! L * .
p0 = [4,3,1,0,2] # Grazing
p1 = [2,4,3,1,0] # Insects
p2 = [0,1,4,2,3] # Domestic
p3 = [3,2,0,4,1] # Game
p4 = [1,0,2,3,4] # Birds

#     " ! L * .
p0 = [52,42,10,18,1] # Grazing
p1 = [42,32,31,45,21] # Insects
p2 = [20,10,51,10, 9] # Domestic
p3 = [31,27,20,51,28] # Game
p4 = [32,19,21,41,50] # Birds

popweight = {
    '0': 2,
    '1': 4,
    '2': 1,
    '3': 0,
    '4': 3,
}
winnershow = collections.defaultdict(set)

for n in range(1, 9):
    print(f'=== {n} ===')
    A = list("".join(sorted(t)) for t in itertools.product('01234', repeat=n))
    B = sorted(set(A))

    winners = collections.defaultdict(lambda:0)
    coll = 0
    for s in B:
        price0 = getprice(s, p0)
        price1 = getprice(s, p1)
        price2 = getprice(s, p2)
        price3 = getprice(s, p3)
        price4 = getprice(s, p4)
        #print(f'{s} -> {price0} != {price1} != {price2} != {price3} != {price4}')
        P = [price0, price1, price2, price3, price4]
        M = max(P)
        K = ''.join(str(v) for v in range(5) if P[v] == M)
        nocoll = (len(K) == 1)
        if not nocoll:
            print(f'{s} -> {price0} != {price1} != {price2} != {price3} != {price4}')
            coll += 1
        winners[K] += 1
        winnershow[K].add(s)

    for k in sorted(winners, key=winners.get, reverse=True):
        print(f'{winners[k]} ({winners[k]/len(B)}) -> {k}')

    #print(coll/len(B))

# DEAD END
