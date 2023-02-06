
let time ?(name="") f x =
    let t = Sys.time() in
    let fx = f x in
    Printf.printf "Execution time %10s: %fs\n" name (Sys.time() -. t);
    fx


let auto_tests =
  List.init 100000 (fun i -> ((i*37+12) mod 10000, (i mod 100) + 1))


let test tests s f =
  (* let r = time ~name:s (fun _ -> List.iter (fun t -> ignore (f t)) tests) () in *)
  let r = time ~name:s (fun _ -> List.map f tests) () in
  ignore r

let[@warning "-partial-match"] wrap f (a,b) = 
  let (r,[da;db]) = f ([a;b], Fun.id) in (r,(da,db))

let rec pow_manual (a,b) = 
  if b = 0 then 1 else a * pow_manual (a,b-1)

let pow_ml_full_pe (a,b) = (
  pow_manual (a,b),
  (b*pow_manual (a,b-1), 0)
)

let zero_pb = fun _ -> (0,0)
let pair_sum (da,db) (da',db') = (da+da', db+db')
let app_pb f (a, a_pb) = 
  let r, r_pb = f a in
  (r, fun s -> a_pb (r_pb s))
let tup_pb (a,a_pb) (b,b_pb) = (a,b), fun (s1,s2) -> pair_sum (a_pb s1) (b_pb s2)
let mul_diffed (a,b) = (a*b, fun s -> (s*b, s*a))
let sub_diffed (a,b) = (a-b, fun s -> (s, -s))
let const_diffed c = (c, zero_pb)
(* let mul_diffed (a',b') = 
  let t' = tup_pb a' b' in
  app_pb mul_diffed t' *)


let rec pow_ml_r (a,b) = 
  match b with
  | 0 -> const_diffed 1
  | _ -> 
    (*
      let r1 = b-1
      let r2 = pow (a,r1)
      let r3 = a * r2


      let bo = (b,1) 
      let r1 = sub b,1
      let t1 = (a,r1)
      let r2 = pow t1
      let t2 = (a,r2)
      let r3 = mul t2
    *)
    let a' = (a, fun s -> (s,0)) in
    let b' = (b, fun s -> (0,s)) in
    let bo' = tup_pb b' (const_diffed 1) in
    let r1' = app_pb sub_diffed bo' in
    let t1' = tup_pb a' r1' in
    let r2' = app_pb pow_ml_r t1' in
    let t2' = tup_pb a' r2' in
    app_pb mul_diffed t2'

    (* let (r,pb) = pow_ml (a,b-1) in *)
(* 
let rec pow_ml (a,b) = 
  match b with
  | 0 -> (1, fun _ -> (0,0))
  | _ -> 
    let a' = (a, fun _ -> (1,0)) in
    let b' = (b, fun _ -> (0,1)) in
    let bo' = sub_diff (b, 1) in
    let r' = app_pb pow_ml  *)
    (* let (r,pb) = pow_ml (a,b-1) in *)


let rec pow_ml_pe_r (a,b) = 
  match b with
  | 0 -> const_diffed 1
  | _ -> 
    let r,pb = pow_ml_pe_r (a,b-1) in
    (
      a*r,
      fun s -> 
        let (da,db) = pb s in
        (r+a*da,0)
    )

let pow_ml arg = 
  let (r,pb) = pow_ml_r arg in
  (r, pb 1)

let pow_ml_pe arg = 
  let (r,pb) = pow_ml_pe_r arg in
  (r, pb 1)

(* let () =
  let (r,(da,db)) = pow_ml (42, 3) in
  Printf.printf "r: %d, da: %d, db: %d\n" r da db;
  let (r,(da,db)) = pow_ml_pe (42, 3) in
  Printf.printf "r: %d, da: %d, db: %d\n" r da db *)
(* let () =
  let (r,(da,db)) = pow (42, 3) in
  Printf.printf "r: %d, da: %d, db: %d\n" r da db;
  let (r,(da,db)) = pow_ml (42, 3) in
  Printf.printf "r: %d, da: %d, db: %d\n" r da db;
  let (r,(da,db)) = pow_ml_pe (42, 3) in
  Printf.printf "r: %d, da: %d, db: %d\n" r da db *)



let pow = wrap Opt.f_diffed
let pow_preopt = wrap Preopt.f_diffed

let () =
  test auto_tests "PreOpt" pow_preopt;
  test auto_tests "Opt" pow;
  test auto_tests "Manual" pow_ml;
  test auto_tests "PE" pow_ml_pe;
  test auto_tests "FullPE" pow_ml_full_pe

