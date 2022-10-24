zero_pb s = (0,0)
x_pb s = (s,0)
n_pb s = (0,s)

tan_add (x,y) (x',y') = (x+x', y+y')
pb_add f g s = tan_add (f s) (g s)

sub' (x,y) = (x-y, \s -> (s, -s))
mul' (x,y) = (x*y, \s -> (s*y, x*s))

pow' ((x, n),ret) = 
    if n == 0 then
        ret (1, zero_pb)
    else

        let tn1 = (n,1) in
        let tn1_pb = \(s1,s2) -> tan_add (n_pb s1) (zero_pb s2) in
        let (n1, sub_pb) = sub' tn1 in
        let n1_pb = tn1_pb . sub_pb in

        let txn = (x,n1) in
        let txn_pb = \(s1,s2) -> tan_add (x_pb s1) (n1_pb s2) in

        pow' (
            txn,
            \(y,pow_pb) ->
                let y_pb = txn_pb . pow_pb in

                let txy = (x,y) in
                let txy_pb = \(s1,s2) -> tan_add (x_pb s1) (y_pb s2) in
                let (z, mul_pb) = mul' (x,y) in
                let z_pb = txy_pb . mul_pb in

                ret (z, z_pb)
        )
        

main = 
    pow' ((400,30000), 
        \(z, z_pb) ->
            do print z
               print (z_pb 1)
    )
    -- let (z, z_pb) = pow' (4,3) in
    -- do print z
    --    print (z_pb 1)
