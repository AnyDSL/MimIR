#![allow(arithmetic_overflow)]
#![feature(bench_black_box)]
#![feature(trait_alias)]

fn zero_pb(s: i32) -> (i32, i32) {
    (0, 0)
}
fn fst_pb(s: i32) -> (i32, i32) {
    (s, 0)
}
fn snd_pb(s: i32) -> (i32, i32) {
    (0, s)
}
fn pair_sum((a, b): (i32, i32), (c, d): (i32, i32)) -> (i32, i32) {
    (a + c, b + d)
}

// Pullbacks

fn app_pb(
    f: impl Fn((i32, i32)) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>),
    // f: Box<dyn Fn((i32, i32)) -> (i32, impl Fn(i32) -> (i32, i32))>,
    (a, a_pb): ((i32, i32), impl Fn((i32, i32)) -> (i32, i32) + 'static),
) -> (i32, impl Fn(i32) -> (i32, i32)) {
    let (r, r_pb) = f(a);
    (r, move |s| a_pb(r_pb(s)))
}
fn tup_pb(
    (a, a_pb): (i32, impl Fn(i32) -> (i32, i32)),
    (b, b_pb): (i32, impl Fn(i32) -> (i32, i32)),
) -> ((i32, i32), impl Fn((i32, i32)) -> (i32, i32)) {
    ((a, b), (move |(sa, sb)| pair_sum(a_pb(sa), b_pb(sb))))
}
fn mul_diff((a, b): (i32, i32)) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>) {
    (a * b, Box::new(move |s| (b * s, a * s)))
}
fn sub_diff((a, b): (i32, i32)) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>) {
    (a - b, Box::new(move |s| (s, -s)))
}
fn const_diff(c: i32) -> (i32, impl Fn(i32) -> (i32, i32)) {
    (c, (zero_pb))
}
fn pow_rs_r((a, b): (i32, i32)) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>) {
    if b == 0 {
        // Box::new(const_diff(1))
        (1, Box::new(zero_pb))
    } else {
        // Type annotation necessary
        let a_diff = (a, (fst_pb));
        // needed because of ownership of a_diff in tup_pb => TODO: handle correctly
        let a_diff2 = (a, (fst_pb));
        let b_diff = (b, (snd_pb));
        let bo_diff = tup_pb(b_diff, const_diff(1));
        let r1_diff = app_pb((sub_diff), bo_diff);
        let t1_diff = tup_pb(a_diff, r1_diff);
        let r2_diff = app_pb((pow_rs_r), t1_diff);
        let t2_diff = tup_pb(a_diff2, r2_diff);
        let (r, r_pb) = (app_pb((mul_diff), (t2_diff)));
        (r, Box::new(r_pb))
        // const_diff(1)
    }
}

fn pow_rs(a: i32, b: i32) -> (i32, (i32, i32)) {
    // (0, (0, 0))
    let (r, r_pb) = pow_rs_r((a, b));
    (r, r_pb(1))
}

// PE Version

// trait PB = Fn(i32) -> (i32, i32);

// fn pow_pe_r(a: i32, b: i32) -> (i32, impl PB) {
//     // (0, |s| (0, 0))
//     if b == 0 {
//         (1, |s| (0, 0))
//     } else {
//         let (r, pow_pb) = pow_pe_r(a, b - 1);
//         // (r, r_pb)
//         let r_pb = move |s| {
//             let (da, db) = pow_pb(s * a);
//             (s * r + da, db)
//         };
//         (a * r, r_pb)
//         // (a * r, move |s| {
//         //     let (da, db) = r_pb(s * a);
//         //     (s * r + da, db)
//         // })
//     }
// }

// trait PB {
//     fn apply(&self, s: i32) -> (i32, i32);
// }

// struct Const();

// impl PB for Const {
//     fn apply(&self, s: i32) -> (i32, i32) {
//         (0, 0)
//     }
// }

// struct Pow(i32, i32, i32, impl PB);

// impl PB for Pow {
//     fn apply(&self, s: i32) -> (i32, i32) {
//         let a = self.0;
//         let b = self.1;
//         let r = self.2;
//         let pow_pb = &self.3;
//         let (da, db) = pow_pb(s * a);
//         (s * r + da, db)
//     }
// }

// fn pow_pe_r(a: i32, b: i32) -> (i32, impl PB) {
//     // (0, |s| (0, 0))
//     if b == 0 {
//         (1, Const())
//     } else {
//         let (r, pow_pb) = pow_pe_r(a, b - 1);
//         (a * r, Pow(a, b, r, pow_pb))
//     }
// }

// trait PB {
//     fn apply(&self, s: i32) -> (i32, i32);
// }

// fn pow_pe(a: i32, b: i32) -> (i32, (i32, i32)) {
//     let (r, r_pb) = pow_pe_r(a, b);
//     (r, r_pb(1))
// }

// enum to store pow_pe_r return pb
enum PowPB {
    Const,
    Closure(i32, i32, i32, Box<PowPB>),
}

fn pow_pe_r(a: i32, b: i32) -> (i32, PowPB) {
    if b == 0 {
        // (1, Box::new(|s| (0, 0)))
        (1, PowPB::Const)
    } else {
        let (r, r_pb) = pow_pe_r(a, b - 1);
        (
            a * r,
            PowPB::Closure(a, b, r, Box::new(r_pb)), // Box::new(move |s| {
                                                     //     let (da, db) = r_pb(s * a);
                                                     //     (s * r + da, db)
                                                     // }),
        )
    }
}

fn eval_pow_pb(pb: PowPB, s: i32) -> (i32, i32) {
    match pb {
        PowPB::Const => (0, 0),
        PowPB::Closure(a, b, r, r_pb) => {
            let (da, db) = eval_pow_pb(*r_pb, s * a);
            (s * r + da, db)
        }
    }
}

fn pow_pe(a: i32, b: i32) -> (i32, (i32, i32)) {
    let (r, r_pb) = pow_pe_r(a, b);
    (r, eval_pow_pb(r_pb, 1))
}

// enum PowPB2<F: Fn(i32) -> (i32, i32)> {
//     Const,
//     Closure(F),
// }

fn main() {
    println!("Hello, world!");
    let tests = (0..100000)
        .map(|i| ((i * 37 + 12) % 10000, (i % 100) + 1))
        .collect::<Vec<_>>();

    let start = std::time::Instant::now();
    for (a, b) in tests.iter() {
        let (_r, (_da, _db)) = std::hint::black_box(pow_rs(*a, *b));
    }
    let time = start.elapsed().as_secs_f64();
    println!("{:10}: {}s", "Rust", time);

    let start = std::time::Instant::now();
    for (a, b) in tests.iter() {
        let (_r, (_da, _db)) = std::hint::black_box(pow_pe(*a, *b));
    }
    let time = start.elapsed().as_secs_f64();
    println!("{:10}: {}s", "PE", time);
}
