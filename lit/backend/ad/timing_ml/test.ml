
let time ?(name="") f x =
    let t = Sys.time() in
    let fx = f x in
    Printf.printf "Execution time %10s: %fs\n" name (Sys.time() -. t);
    fx


let auto_tests =
  List.init 100000 (fun i -> ((i*37+12) mod 10000, i mod 100))


let test tests s f =
  time ~name:s (fun _ -> List.iter (fun t -> ignore (f t)) tests) ()

let[@warning "-partial-match"] wrap f (a,b) = 
  let (r,[da;db]) = f ([a;b], Fun.id) in (r,(da,db))

let pow = wrap Opt.f_diffed
let pow_preopt = wrap Preopt.f_diffed

let () =
  test auto_tests "PreOpt" pow_preopt;
  test auto_tests "Opt" pow
