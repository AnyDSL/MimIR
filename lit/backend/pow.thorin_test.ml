#use "pow.thorin.ml";;

let pow_ds (a,b) = pow ([a;b], Fun.id)

(* let test a b = pow ([a;b], fun r -> Printf.printf "%d ^ %d = %d (expected %d)\n" a b r (int_of_float (float_of_int a ** float_of_int b))) *)
let test ?(verbose=false) a b =
  let r = pow_ds (a,b) in
  let expected = int_of_float (float_of_int a ** float_of_int b) in
  (if verbose then
    Printf.printf "%d ^ %d = %d (%s)\n" a b r (if r = expected then "OK" else "ERROR")
  else
    ())
  ;
  r = expected

let tests = [
  (42,2);
  (42,3)
]

let _ = List.iter (fun (a,b) -> 
    assert (test ~verbose:true a b)
  ) tests
