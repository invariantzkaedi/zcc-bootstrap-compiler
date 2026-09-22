struct Point {
    x: i32,
    y: i32,
}

impl Point {
    fn sum(&self) -> i32 {
        return self.x + self.y;
    }
}

enum ResultVal {
    Ok(i32),
    Err(i32),
}

fn eval_result(r: ResultVal) -> i32 {
    match r {
        ResultVal::Ok(v) => {
            return v;
        },
        ResultVal::Err(e) => {
            return -e;
        },
    }
}

fn main() -> i32 {
    let p: Point = Point { x: 15, y: 27 };
    let s: i32 = p.sum();
    if s != 42 {
        return 1;
    }
    let r1: ResultVal = ResultVal::Ok(100);
    let r2: ResultVal = ResultVal::Err(58);
    let v1: i32 = eval_result(r1);
    let v2: i32 = eval_result(r2);
    if v1 + v2 != 42 {
        return 2;
    }
    return 0;
}
