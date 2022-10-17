-- pow (x, 0) = 1
-- pow (x, n) = x * pow (x, n-1)


zero_pb s = (0,0)
x_pb s = (s,0)
n_pb s = (0,s)

tan_add (x,y) (x',y') = (x+x', y+y')
pb_add f g s = tan_add (f s) (g s)

-- sub' :: (Int, Int) -> (Int, Int -> (Int, Int))
sub' (x,y) = (x-y, \s -> (s, -s))
-- mul' :: (Int, Int) -> (Int, Int -> (Int, Int))
mul' (x,y) = (x*y, \s -> (s*y, x*s))

-- pow' :: (Eq a1, Num a1, Num a2, Num (a2, a2)) => (a2, a1) -> (a2, a2 -> (a2, a2))
-- pow' :: (Int, Int) -> (Int, Int -> (Int, Int))
pow' (x, n) = 
    if n == 0 then
        (1, \s -> zero_pb s)
    else
        let tn1 = (n,1) in
        let tn1_pb = \(s1,s2) -> tan_add (n_pb s1) (zero_pb s2) in
        let (n1, sub_pb) = sub' tn1 in
        let n1_pb = tn1_pb . sub_pb in

        let txn = (x,n1) in
        let txn_pb = \(s1,s2) -> tan_add (x_pb s1) (n1_pb s2) in
        let (y, pow_pb) = pow' txn in
        let y_pb = txn_pb . pow_pb in

        let txy = (x,y) in
        let txy_pb = \(s1,s2) -> tan_add (x_pb s1) (y_pb s2) in
        let (z, mul_pb) = mul' (x,y) in
        let z_pb = txy_pb . mul_pb in
        
        (z, z_pb)

main = 
    let (z, z_pb) = pow' (400,30000) in
    do print z
       print (z_pb 1)
