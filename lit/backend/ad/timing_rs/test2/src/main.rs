fn add_tuple(n: i32, ab: (i32, i32)) -> i32 {
    ab.0 + ab.1
}
fn add_tuple2((ab @ (a, b)): (i32, i32)) -> i32 {
    a + ab.1
}
fn add_list([a, b]: [i32; 2]) -> i32 {
    a + b
}
fn sum_list(n: i32, xs: &[i32]) -> i32 {
    if n == 0 {
        0
    } else {
        add_list([xs[0], sum_list(n - 1, &xs[1..])])
    }
}
// fn sum_list2(n: i32, xs: [i32; n]) -> i32 {
//     42 << 2
// }

fn bool2bit(b: bool) -> i32 {
    if b {
        1
    } else {
        0
    }
}

fn fortyTwo() -> [i32; 5] {
    [42; 5]
}

fn cps<T>(a: i32, ret: Box<dyn Fn(i32) -> T>) -> T {
    ret(a)
}

fn add_tuple3((ab @ (a, b)): (i32, Box<dyn Fn(i32) -> i32>)) -> i32 {
    b(a)
}

// fn f_1172768<T>(((af@(a, ret)): (i32 , Box<dyn Fn(i32) -> T>))) -> T {
//     ret a
// }

fn main() {
    println!("Hello, world!");
}
