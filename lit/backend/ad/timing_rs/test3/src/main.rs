#![allow(arithmetic_overflow)]
fn bool2bit(b: bool) -> i32 {
    if b {
        1
    } else {
        0
    }
}


fn f_1172768<T>(((_1172809@(_1172811, _1172833)) : (i32 , Box<dyn Fn(i32) -> T>))) -> T {
        let _1172831 = Box::new(move |(_1172834 : i32)| { 
                (_1172833 _1172834)
        });

        let pow_cont_1172826 = Box::new(move |(__1172859 : i32)| { 
                let _1172866 : i32 = ((42:i32) * __1172859);
                (_1172831 _1172866)
        });

        let pow_else_1172790 = Box::new(move |_| { 
                let _1172818 : i32 = ((-1:i32) + _1172811);
                (f_1172768 (_1172818, pow_cont_1172826))
        });

        let pow_then_1172877 = Box::new(move |_| { 
                (_1172831 (1:i32))
        });

        let _1172904 : i32 = (bool2bit ((0:i32) == _1172811));
        (([pow_else_1172790, pow_then_1172877][_1172904]) ())
}

// external 
fn thorin_main<T>(((__1172931@(mem_1172950, _, _, return_1172935)): (() , i32 , &(&(i32)) , Box<dyn Fn((() , i32)) -> T>))) -> T {
        let return_1172930 = Box::new(move |(_1172936 : (() , i32))| { 
                (return_1172935 _1172936)
        });

        let ret_cont_1172921 = Box::new(move |(r_1172954 : i32)| { 
                (return_1172930 (mem_1172950, r_1172954))
        });

        (f_1172768 ((2:i32), ret_cont_1172921))
}

// external 
fn pow<T>(((__1173017@(((__1173019@[a_1173020, b_1173024]): [i32;2]), ret_1173047)): ([i32;2] , Box<dyn Fn(i32) -> T>))) -> T {
        let ret_1173045 = Box::new(move |(_1173048 : i32)| { 
                (ret_1173047 _1173048)
        });

        let pow_cont_1173036 = Box::new(move |(__1173055 : i32)| { 
                let _1173060 : i32 = ((__1173019[(0:i32)]) * __1173055);
                (ret_1173045 _1173060)
        });

        let pow_else_1173002 = Box::new(move |_| { 
                let _1173031 : i32 = ((-1:i32) + (__1173019[(1:i32)]));
                (pow ([(__1173019[(0:i32)]), _1173031], pow_cont_1173036))
        });

        let pow_then_1173067 = Box::new(move |_| { 
                (ret_1173045 (1:i32))
        });

        let _1173081 : i32 = (bool2bit ((0:i32) == (__1173019[(1:i32)])));
        (([pow_else_1173002, pow_then_1173067][_1173081]) ())
}

fn main() {
    println!("Hello, world!");
}
