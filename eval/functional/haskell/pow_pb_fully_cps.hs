zero_pb (s,ret) = ret (0,0)
x_pb (s,ret) = ret (s,0)
n_pb (s,ret) = ret (0,s)

-- tan_add :: (Num a, Num b) => (((a, b), (a, b)), (a, b) -> t) -> t
-- tan_add :: (Num a, Num b) => (((a, b), (a, b)), (a, b) -> ()) -> ()
-- tan_add :: (((Integer, Integer), (Integer, Integer)), (Integer, Integer) -> t) -> t
tan_add (((x,y),(x',y')),ret) = ret (x+x', y+y')

sub' ((x,y),ret) = ret (x-y, \(s,ret) -> ret (s, -s))
mul' ((x,y),ret) = ret (x*y, \(s,ret) -> ret (s*y, x*s))

-- tup_pb :: ((a, (Integer, Integer) -> t) -> t,
--           (b, (Integer, Integer) -> t) -> t)
--           -> ((a, b), (Integer, Integer) -> t) -> t
tup_pb (pb1,pb2) ((s1,s2),ret) = 
    pb1 (s1, \t1 ->
        pb2 (s2, \t2 ->
            tan_add ((t1,t2), ret)))


compose :: ((b, c -> t) -> t, (a, b -> t) -> t) -> (a, c->t) -> t
compose (f,g) (s,ret) = 
    g (s, \t -> f(t,ret))

pow' ((x, n),ret) = 
    if n == 0 then
        ret (1, zero_pb)
    else
        let tn1 = (n,1) in
        let tn1_pb = tup_pb (n_pb,zero_pb) in
        sub' (tn1, \(n1,sub_pb) ->
        let n1_pb = compose (tn1_pb, sub_pb) in
            -- ret (n1, n1_pb)
        let txn = (x,n1) in
        let txn_pb = tup_pb (x_pb,n1_pb) in

        pow' (txn, \(y,pow_pb) ->
                let y_pb = compose (txn_pb,pow_pb) in

                let txy = (x,y) in
                let txy_pb = tup_pb (x_pb, y_pb) in
                mul' (txy, \(z, mul_pb) ->
                    let z_pb = compose (txy_pb, mul_pb) in

                    ret (z, z_pb)
                )
            )
        )
        

main = 
   pow' ((400,30000), 
        \(z, z_pb) ->
            z_pb (1, \(xt,nt) -> 
    do print z
       print (xt,nt)
            ))
--    let (z,(xt,nt)) = pow' ((400,30000), 
--         \(z, z_pb) ->
--             z_pb (1, \(xt,nt) -> 
--                 (z,(xt,nt)))) in
--     do print z
--        print (xt,nt)
                -- do print z 
                --    print (xt,nt)))
    -- let (z, z_pb) = pow' (4,3) in
    -- do print z
    --    print (z_pb 1)
