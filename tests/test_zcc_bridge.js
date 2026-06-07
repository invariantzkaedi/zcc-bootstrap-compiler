console.log("--- Testing ZCC Bridge and Floating-Point ---");
let initial_success = ZCC.generateSprites();
console.log("Initial generateSprites returned:", initial_success);

console.log("Setting phase for flashloan to 0.73");
let p1 = ZCC.setPhase("flashloan", 0.73);
console.log("setPhase returned:", p1);

console.log("Setting phase for all to 0.42");
let p2 = ZCC.setPhase("all", 0.42);
console.log("setPhase all returned:", p2);

console.log("Testing floating-point arithmetic:");
let f1 = 0.5;
let f2 = 0.25;
let sum_f = f1 + f2;
console.log("0.5 + 0.25 =", sum_f);
console.log("0.5 * 0.25 =", f1 * f2);
console.log("0.5 / 0.25 =", f1 / f2);
console.log("0.5 % 0.2 =", f1 % 0.2);
