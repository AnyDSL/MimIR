(* let rec f_394645 (((_394692, _394716) as _394690): (int, int -> unit)) = _394692 *)
(* let rec f_394645 (((a, b) as c): (int, int -> unit)) = a *)
(* let rec f_394645 (((a, b) as c): (int, int)) = a
let rec g (((a,b) as p):(int*int)) = a+b *)

let rec pow42 (((b, ret) as _394690): (int * (int -> unit))) = 
    let rec ret_wrap (_394717 : int) = 
        (ret _394717)
    in
    let rec pow_cont (__394742 : int) = 
        (* let _394749: int = (((%core.wrap.mul 4294967296) 0) [42:(.Idx 4294967296), __394742]) in *)
        (* (_394718 (((%core.wrap.mul 4294967296) 0) [42:(.Idx 4294967296), __394742])) *)
        let _394749: int = (42 * __394742) in
        ret_wrap _394749
        (* (_394718 (((%core.wrap.mul 4294967296) 0) [42:(.Idx 4294967296), __394742])) *)
    in
    let rec pow_else _ = 
        (* let _394699: int = (((%core.wrap.add 4294967296) 0) [4294967295:(.Idx 4294967296), _394692]) in *)
        (* let _394699: int = 4294967295 + a in *)
        let _394699: int = b - 1 in
        (pow42 (_394699, pow_cont))
    in
    let rec pow_then _ = 
        (ret_wrap (1:(int)))
    in
    (* let _394787: int = ((%core.icmp.xyglE 4294967296) [0:(.Idx 4294967296), _394692]) in *)
    let _394787: int = if (b=0) then 1 else 0 in
    (List.nth [pow_else; pow_then] _394787 ())

let test x = pow42 (x, fun r -> Printf.printf "42 ^ %d = %d (expected %d)\n" x r (int_of_float (42. ** float_of_int x)))
let _ = test 2
let _ = test 3
