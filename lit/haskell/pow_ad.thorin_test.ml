#use "pow_ad.thorin.ml";;

let pow_ds (a,b) = 
  let (r,[da;db]) = f_diffed ([a;b], Fun.id) in
  (r,(da,db))

let test ?(verbose=false) a b =
  let (r,(da,db)) = pow_ds (a,b) in
  let expected = int_of_float (float_of_int a ** float_of_int b) in
  let expected_diff = int_of_float (float_of_int b *. (float_of_int a ** float_of_int (b-1))) in
  let test = (da = expected_diff && r = expected) in
  (if verbose then
    Printf.printf "%d ^ %d = %d, diff = %d (%s)\n" a b r da (if test then "OK" else "FAIL")
  else
    ())
  ;
  test

let tests = [
  (42,2);
  (42,3)
]

let _ = List.iter (fun (a,b) -> 
    assert (test ~verbose:true a b)
  ) tests
