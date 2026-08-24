const fs = require('fs');
const path = require('path');

const wasmPath = path.join(__dirname, '..', 'test_wasm_native.wasm');
const bytes = fs.readFileSync(wasmPath);
const mod = new WebAssembly.Module(bytes);
const instance = new WebAssembly.Instance(mod, {});
const exp = instance.exports;

console.log("==================================================");
console.log("     ZCC NATIVE WEBASSEMBLY RUNTIME AUDIT         ");
console.log("==================================================");
console.log("Exported Symbols:", Object.keys(exp));

const r_add = exp.add(20, 22);
console.log(`[TEST 1] add(20, 22)       = ${r_add} (expected 42)`);

const r_sub = exp.sub(100, 42);
console.log(`[TEST 2] sub(100, 42)      = ${r_sub} (expected 58)`);

const r_mul = exp.mul(6, 7);
console.log(`[TEST 3] mul(6, 7)         = ${r_mul} (expected 42)`);

const r_fact = exp.factorial(5);
console.log(`[TEST 4] factorial(5)      = ${r_fact} (expected 120)`);

const r_fib = exp.fib(10);
console.log(`[TEST 5] fib(10)           = ${r_fib} (expected 55)`);

const r_bits = exp.bitwise_ops(5, 3);
console.log(`[TEST 6] bitwise_ops(5, 3) = ${r_bits} (expected 6)`);

const r_main = exp.main();
console.log(`[TEST 7] main()            = ${r_main} (expected 217)`);

if (r_add === 42 && r_sub === 58 && r_mul === 42 && r_fact === 120 && r_fib === 55 && r_bits === 6 && r_main === 217) {
    console.log("==================================================");
    console.log("★ ALL NATIVE WASM EXECUTION VERIFICATIONS PASSED ★");
    console.log("==================================================");
    process.exit(0);
} else {
    console.error("FAIL: Native WASM execution result mismatch!");
    process.exit(1);
}
