#![allow(arithmetic_overflow)]
#![feature(bench_black_box)]

fn pow_manual(a: i32, b: i32) -> i32 {
    if b == 0 {
        1
    } else {
        a * pow_manual(a, b - 1)
    }
}

fn pow_full_pe(a: i32, b: i32) -> (i32, (i32, i32)) {
    (pow_manual(a, b), (a * pow_manual(a, b - 1), 0))
}

fn pow_pe_r(a: i32, b: i32) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>) {
    if b == 0 {
        (1, Box::new(|s| (0, 0)))
    } else {
        let (r, r_pb) = pow_pe_r(a, b - 1);
        (
            a * r,
            Box::new(move |s| {
                let (da, db) = r_pb(s * a);
                (s * r + da, db)
            }),
        )
    }
}

fn pow_pe(a: i32, b: i32) -> (i32, (i32, i32)) {
    let (r, r_pb) = pow_pe_r(a, b);
    (r, r_pb(1))
}

// start

fn zero_pb(s: i32) -> (i32, i32) {
    (0, 0)
}
fn fst_pb(s: i32) -> (i32, i32) {
    (s, 0)
}
fn snd_pb(s: i32) -> (i32, i32) {
    (0, s)
}
fn pair_sum((p @ (a, b)): (i32, i32), (c, d): (i32, i32)) -> (i32, i32) {
    (a + c, b + d)
}
fn app_pb(
    f: Box<dyn Fn((i32, i32)) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>)>,
    (a, a_pb): ((i32, i32), Box<dyn Fn((i32, i32)) -> (i32, i32)>),
) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>) {
    let (r, r_pb) = f(a);
    (r, Box::new(move |s| a_pb(r_pb(s))))
}
fn tup_pb(
    (a, a_pb): (i32, Box<dyn Fn(i32) -> (i32, i32)>),
    (b, b_pb): (i32, Box<dyn Fn(i32) -> (i32, i32)>),
) -> ((i32, i32), Box<dyn Fn((i32, i32)) -> (i32, i32)>) {
    (
        (a, b),
        Box::new(move |(sa, sb)| pair_sum(a_pb(sa), b_pb(sb))),
    )
}
fn mul_diff((a, b): (i32, i32)) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>) {
    (a * b, Box::new(move |s| (b * s, a * s)))
}
fn sub_diff((a, b): (i32, i32)) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>) {
    (a - b, Box::new(move |s| (s, -s)))
}
fn const_diff(c: i32) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>) {
    (c, Box::new(zero_pb))
}
fn pow_rs_r((a, b): (i32, i32)) -> (i32, Box<dyn Fn(i32) -> (i32, i32)>) {
    if b == 0 {
        const_diff(1)
    } else {
        // Type annotation necessary
        let a_diff: (i32, Box<dyn Fn(i32) -> (i32, i32)>) = (a, Box::new(fst_pb));
        // needed because of ownership of a_diff in tup_pb => TODO: handle correctly
        let a_diff2: (i32, Box<dyn Fn(i32) -> (i32, i32)>) = (a, Box::new(fst_pb));
        let b_diff: (i32, Box<dyn Fn(i32) -> (i32, i32)>) = (b, Box::new(snd_pb));
        let bo_diff = tup_pb(b_diff, const_diff(1));
        let r1_diff = app_pb(Box::new(sub_diff), bo_diff);
        let t1_diff = tup_pb(a_diff, r1_diff);
        let r2_diff = app_pb(Box::new(pow_rs_r), t1_diff);
        let t2_diff = tup_pb(a_diff2, r2_diff);
        app_pb(Box::new(mul_diff), t2_diff)
        // const_diff(1)
    }
}
// end

fn pow_rs(a: i32, b: i32) -> (i32, (i32, i32)) {
    // (0, (0, 0))
    let (r, r_pb) = pow_rs_r((a, b));
    (r, r_pb(1))
}

fn main() {
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

    let start = std::time::Instant::now();
    for (a, b) in tests.iter() {
        let (_r, (_da, _db)) = std::hint::black_box(pow_full_pe(*a, *b));
    }
    let time = start.elapsed().as_secs_f64();
    println!("{:10}: {}s", "Full PE", time);
}
