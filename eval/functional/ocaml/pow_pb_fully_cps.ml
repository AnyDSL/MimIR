let zero_pb (s,ret) = ret (0,0)
let x_pb (s,ret) = ret (s,0)
let n_pb (s,ret) = ret (0,s)

let tan_add (((a,b),(c,d)),ret) = ret (a+c,b+d)

let sub' ((x,y),ret) = ret (x-y, fun (s,ret) -> ret (s,-s))
let mul' ((x,y),ret) = ret (x*y, fun (s,ret) -> ret (s*y,s*x)) 

let tup_pb (pb1,pb2) ((s1,s2),ret) = 
  pb1 (s1, fun tan1 ->
    pb2 (s2, fun tan2 ->
       tan_add ((tan1,tan2),ret)))

let compose (f,g) (s,ret) = 
    g (s, fun t ->
        f (t,ret)
    )

let rec pow' ((x,n),ret) = 
  match n with
  | 0 -> ret (1, zero_pb)
  | _ -> 
      let tn1 = (n,1) in
      let tn1_pb = tup_pb (n_pb,zero_pb) in
      sub' (tn1, fun (n1,sub_pb) ->
      let n1_pb = compose (tn1_pb,sub_pb) in

      let txn = (x,n1) in
      let txn_pb = tup_pb (x_pb,n1_pb) in

      pow' (
        txn,
        fun (y, pow_pb) -> 
          let y_pb = compose (txn_pb,pow_pb) in

          let txy = (x,y) in
          let txy_pb = tup_pb (x_pb,y_pb) in
          mul' (txy, fun (z, mul_pb) ->
          let z_pb = compose (txy_pb,mul_pb) in
          ret (z, z_pb)
          )
      )

      )

let _ = 
  pow' ((4,3),
    fun (z, z_pb) -> 
      z_pb (1, fun (xt,nt) ->
        Printf.printf "z = %d, x' = %d, n' = %d\n" z xt nt
      )
  )
