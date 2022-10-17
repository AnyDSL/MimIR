let rec pow (x,n) = 
  match n with
  | 0 -> 1
  | _ -> x * pow(x,n-1)

let _ = 
  (* print_int (pow(2,10)); *)
  print_int (pow(4,3));
  print_newline()
