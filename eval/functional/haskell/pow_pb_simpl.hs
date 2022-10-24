pow' (x, n) = 
    if n == 0 then
        (1, \s -> (0,0))
    else
        let (y,y_pb) = pow' (x, n-1) in
        (x*y,
            \s ->
                let (a1,a2) = (y_pb (x*s)) in
                    (a1+y*s,a2)
        )
        

main = 
    let (z, z_pb) = pow' (400,30000) in
    do print z
       print (z_pb 1)
