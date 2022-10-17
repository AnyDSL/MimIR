let rec pow' (x,n) = 
  match n with
  | 0 -> (1, fun s -> (0,0))
  | _ -> 
    let (y, pb) = pow' (x, n-1) in
    (x*y,
      fun s ->
        let (x', n') = pb (x*s) in
        (x'+y*s, n')
    )

let _ = 
  let (z, z_pb) = pow' (4,3) in
  let (xt, nt) = z_pb 1 in
  Printf.printf "z = %d, x' = %d, n' = %d\n" z xt nt
