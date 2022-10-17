let zero_pb s = (0,0)
let x_pb s = (s,0)
let n_pb s = (0,s)

let tan_add (a,b) (c,d) = (a+c,b+d)

let sub' (x,y) = (x-y, fun s -> (s,-s))
let mul' (x,y) = (x*y, fun s -> (s*y,s*x)) 

let tup_pb pb1 pb2 (s1,s2) = tan_add (pb1 s1) (pb2 s2)

let rec pow' (x,n) = 
  match n with
  | 0 -> (1, zero_pb)
  | _ -> 
      let tn1 = (n,1) in
      let tn1_pb = tup_pb n_pb zero_pb in
      let (n1,sub_pb) = sub' tn1 in
      let n1_pb s = tn1_pb (sub_pb s) in

      let txn = (x,n1) in
      let txn_pb = tup_pb x_pb n1_pb in
      let (y, pow_pb) = pow' txn in
      let y_pb s = txn_pb (pow_pb s) in

      let txy = (x,y) in
      let txy_pb = tup_pb x_pb y_pb in
      let (z, mul_pb) = mul' txy in
      let z_pb s = txy_pb (mul_pb s) in

      (z, z_pb)

let _ = 
  let (z, z_pb) = pow' (4,3) in
  let (xt, nt) = z_pb 1 in
  Printf.printf "z = %d, x' = %d, n' = %d\n" z xt nt
