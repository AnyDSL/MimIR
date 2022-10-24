pow ((x, n),ret) = 
        if n == 0 then ret 1
        else 
            pow ((x, n-1), (\y -> ret (x*y)))

main = pow ((400,30000),print)
