(* let rec f_394645 (((_394692, _394716) as _394690): (int, int -> unit)) = _394692 *)
(* let rec f_394645 (((a, b) as c): (int, int -> unit)) = a *)
(* let rec f_394645 (((a, b) as c): (int, int)) = a
let rec g (((a,b) as p):(int*int)) = a+b *)

let rec f_394645 (((_394692, _394716) as _394690): (int * (int -> unit))) = 
    let rec _394714 (_394717 : int) = 
        (_394716 _394717)
    in
    let rec pow_cont_394709 (__394742 : int) = 
        (* let _394749: int = (((%core.wrap.mul 4294967296) 0) [42:(.Idx 4294967296), __394742]) in *)
        (* (_394718 (((%core.wrap.mul 4294967296) 0) [42:(.Idx 4294967296), __394742])) *)
        let _394749: int = (42 * __394742) in
        _394714 _394749
        (* (_394718 (((%core.wrap.mul 4294967296) 0) [42:(.Idx 4294967296), __394742])) *)
    in
    let rec pow_else_394671 _ = 
        (* let _394699: int = (((%core.wrap.add 4294967296) 0) [4294967295:(.Idx 4294967296), _394692]) in *)
        (* let _394699: int = 4294967295 + _394692 in *)
        let _394699: int = _394692 - 1 in
        (f_394645 (_394699, pow_cont_394709))
    in
    let rec pow_then_394761 _ = 
        (_394714 (1:(int)))
    in
    (* let _394787: int = ((%core.icmp.xyglE 4294967296) [0:(.Idx 4294967296), _394692]) in *)
    let _394787: int = if (0 = _394692) then 1 else 0 in
    (List.nth [pow_else_394671; pow_then_394761] _394787 ())

(* external *)
let rec main (((mem_394830, _, _, return_394815) as __394811): (unit * int * ((int ref) ref) * ((unit * int) -> unit))) = 
    let rec return_394810 (_394816 : (unit * int)) = 
        (return_394815 _394816)
    in
    let rec ret_cont_394801 (r_394834 : int) = 
        (return_394810 (mem_394830, r_394834))
    in
    (f_394645 (2, ret_cont_394801))

let test x = f_394645 (x, fun r -> Printf.printf "42 ^ %d = %d\n" x r)
let _ = test 2
let _ = test 3
