#use "pow_ad.thorin.ml";;
(* #use "pow_ad_preopt_fix.ml";; *)

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


(* timing tests *)
let time f x =
    let t = Sys.time() in
    let fx = f x in
    Printf.printf "Execution time: %fs\n" (Sys.time() -. t);
    fx

let auto_tests =
  (* List.init 100000 (fun _ -> (Random.int 1000, Random.int 100)) *)
  List.init 100000 (fun i -> ((i*37+12) mod 10000, i mod 100))

let _ = time (fun _ -> List.iter (fun (a,b) ->
    test a b |> ignore
  ) auto_tests) ()
