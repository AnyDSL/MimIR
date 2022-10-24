pow (x, 0) = 1
pow (x, n) = x * pow (x, n-1)

main = print (pow (400,30000))
