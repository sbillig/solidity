{
    let a := 1
    let b := a
    if calldataload(0) { b := 2 }
    pop(a)
    pop(b)
    function f() -> r
    {
        pop(r)
    }
}
// ----
// step: literalRematerialiser
//
// {
//     let a := 1
//     let b := 1
//     if calldataload(0) { b := 2 }
//     pop(1)
//     pop(b)
//     function f() -> r
//     { pop(0) }
// }
