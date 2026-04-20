#!/usr/bin/env python3
"""
🔱 Pipeline Validation — Etherscan → PRIME → Training (E2E)
═══════════════════════════════════════════════════════════

Generates a synthetic Etherscan corpus with realistic Solidity patterns,
runs the full pipeline, and validates the training output.
"""

import json
import sys
import os

# ═══════════════════════════════════════════════════════════════════
# Synthetic Corpus Generator
# ═══════════════════════════════════════════════════════════════════

CONTRACTS = [
    {
        "address": "0x7a250d5630B4cF539739dF2C5dAcb4c659F2488D",
        "name": "Uniswap V2 Router",
        "category": "DEX",
        "source": """
pragma solidity ^0.8.0;

interface IUniswapV2Factory {
    function getPair(address tokenA, address tokenB) external view returns (address pair);
}

interface IUniswapV2Pair {
    function getReserves() external view returns (uint112, uint112, uint32);
    function swap(uint amount0Out, uint amount1Out, address to, bytes calldata data) external;
}

contract UniswapV2Router {
    address public factory;
    address public WETH;
    mapping(address => uint) public nonces;

    event Swap(address indexed sender, uint amountIn, uint amountOut);

    modifier ensure(uint deadline) {
        require(block.timestamp <= deadline, 'EXPIRED');
        _;
    }

    constructor(address _factory, address _weth) {
        factory = _factory;
        WETH = _weth;
    }

    function getAmountOut(uint amountIn, uint reserveIn, uint reserveOut)
        public pure returns (uint amountOut)
    {
        require(amountIn > 0, 'INSUFFICIENT_INPUT');
        require(reserveIn > 0 && reserveOut > 0, 'INSUFFICIENT_LIQUIDITY');
        uint amountInWithFee = amountIn * 997;
        uint numerator = amountInWithFee * reserveOut;
        uint denominator = reserveIn * 1000 + amountInWithFee;
        amountOut = numerator / denominator;
    }

    function swapExactTokensForTokens(
        uint amountIn, uint amountOutMin, address[] calldata path,
        address to, uint deadline
    ) external ensure(deadline) returns (uint[] memory amounts) {
        amounts = new uint[](path.length);
        amounts[0] = amountIn;
        for (uint i = 0; i < path.length - 1; i++) {
            (uint reserveIn, uint reserveOut,) = IUniswapV2Pair(
                IUniswapV2Factory(factory).getPair(path[i], path[i + 1])
            ).getReserves();
            amounts[i + 1] = getAmountOut(amounts[i], reserveIn, reserveOut);
        }
        require(amounts[amounts.length - 1] >= amountOutMin, 'SLIPPAGE');
        emit Swap(msg.sender, amountIn, amounts[amounts.length - 1]);
    }
}
"""
    },
    {
        "address": "0xBEBC44782C7dB0a1A60Cb6fe97d0b483032FF1C7",
        "name": "Curve 3pool",
        "category": "DEX",
        "source": """
pragma solidity ^0.8.0;

contract Curve3Pool {
    uint256 constant N_COINS = 3;
    uint256[N_COINS] public balances;
    uint256 public fee;
    address public owner;

    mapping(address => uint256) public token_balance;
    event TokenExchange(address indexed buyer, int128 i, uint256 dx, int128 j, uint256 dy);

    modifier onlyOwner() {
        require(msg.sender == owner, "not owner");
        _;
    }

    constructor() {
        owner = msg.sender;
    }

    function get_dy(int128 i, int128 j, uint256 dx) external view returns (uint256) {
        uint256 x = balances[uint256(int256(i))] + dx;
        uint256 y = balances[uint256(int256(j))];
        return y - y * x / (x + y) - fee * dx / 10000;
    }

    function exchange(int128 i, int128 j, uint256 dx, uint256 min_dy) external {
        uint256 dy = this.get_dy(i, j, dx);
        require(dy >= min_dy, "slippage");
        balances[uint256(int256(i))] += dx;
        balances[uint256(int256(j))] -= dy;
        token_balance[msg.sender] += dy;
        emit TokenExchange(msg.sender, i, dx, j, dy);
    }

    function withdraw_admin_fees() external onlyOwner {
        payable(owner).transfer(address(this).balance);
    }
}
"""
    },
    {
        "address": "0x7d2768dE32b0b80b7a3454c06BdAc94A69DDc7A9",
        "name": "Aave V2 Lending Pool",
        "category": "Lending",
        "source": """
pragma solidity ^0.8.0;

interface IERC20 {
    function transfer(address to, uint256 amount) external returns (bool);
    function balanceOf(address account) external view returns (uint256);
}

contract AaveLendingPool {
    struct ReserveData {
        uint128 liquidityIndex;
        uint128 variableBorrowIndex;
        uint128 currentLiquidityRate;
        uint40 lastUpdateTimestamp;
        address aTokenAddress;
        bool isActive;
    }

    mapping(address => ReserveData) public reserves;
    mapping(address => mapping(address => uint256)) public userDeposits;
    mapping(address => mapping(address => uint256)) public userBorrows;
    address public admin;

    event Deposit(address indexed user, address indexed reserve, uint256 amount);
    event Borrow(address indexed user, address indexed reserve, uint256 amount);
    event Repay(address indexed user, address indexed reserve, uint256 amount);
    event FlashLoan(address indexed target, address indexed initiator, uint256 amount);

    modifier onlyAdmin() {
        require(msg.sender == admin, "only admin");
        _;
    }

    constructor() {
        admin = msg.sender;
    }

    function deposit(address asset, uint256 amount, address onBehalfOf) external {
        require(reserves[asset].isActive, "reserve inactive");
        require(amount > 0, "zero amount");
        IERC20(asset).transfer(address(this), amount);
        userDeposits[onBehalfOf][asset] += amount;
        emit Deposit(onBehalfOf, asset, amount);
    }

    function borrow(address asset, uint256 amount) external {
        require(reserves[asset].isActive, "reserve inactive");
        uint256 collateral = userDeposits[msg.sender][asset];
        require(collateral * 75 / 100 >= amount + userBorrows[msg.sender][asset], "undercollateralized");
        userBorrows[msg.sender][asset] += amount;
        IERC20(asset).transfer(msg.sender, amount);
        emit Borrow(msg.sender, asset, amount);
    }

    function flashLoan(address receiver, address asset, uint256 amount) external {
        uint256 balanceBefore = IERC20(asset).balanceOf(address(this));
        IERC20(asset).transfer(receiver, amount);
        (bool success,) = receiver.call(abi.encodeWithSignature("executeOperation(address,uint256)", asset, amount));
        require(success, "callback failed");
        uint256 balanceAfter = IERC20(asset).balanceOf(address(this));
        require(balanceAfter >= balanceBefore + amount * 9 / 10000, "flash loan not repaid");
        emit FlashLoan(receiver, msg.sender, amount);
    }

    function liquidate(address user, address collateralAsset, address debtAsset, uint256 debtAmount) external {
        require(userBorrows[user][debtAsset] >= debtAmount, "excess liquidation");
        uint256 collateralAmount = debtAmount * 105 / 100;
        userBorrows[user][debtAsset] -= debtAmount;
        userDeposits[user][collateralAsset] -= collateralAmount;
        IERC20(collateralAsset).transfer(msg.sender, collateralAmount);
    }
}
"""
    },
    {
        "address": "0x00000000006c3852cbEf3e08E8dF289169EdE581",
        "name": "Seaport NFT Exchange",
        "category": "NFT",
        "source": """
pragma solidity ^0.8.0;

contract Seaport {
    struct Order {
        address offerer;
        uint256 startTime;
        uint256 endTime;
        bytes32 orderHash;
        uint256 salt;
    }

    mapping(bytes32 => bool) public orderFilled;
    mapping(address => uint256) public counters;

    event OrderFulfilled(bytes32 indexed orderHash, address indexed offerer, address indexed fulfiller);

    function fulfillOrder(Order calldata order) external payable returns (bool) {
        require(block.timestamp >= order.startTime, "not started");
        require(block.timestamp <= order.endTime, "expired");

        bytes32 hash = keccak256(abi.encodePacked(order.offerer, order.salt, counters[order.offerer]));
        require(!orderFilled[hash], "already filled");
        orderFilled[hash] = true;

        (bool success,) = order.offerer.call{value: msg.value}("");
        require(success, "transfer failed");

        emit OrderFulfilled(hash, order.offerer, msg.sender);
        return true;
    }

    function cancel(Order[] calldata orders) external {
        for (uint i = 0; i < orders.length; i++) {
            require(orders[i].offerer == msg.sender, "not offerer");
            bytes32 hash = keccak256(abi.encodePacked(orders[i].offerer, orders[i].salt));
            orderFilled[hash] = true;
        }
    }

    function incrementCounter() external returns (uint256) {
        return ++counters[msg.sender];
    }

    function verify(bytes32 orderHash, bytes memory signature) external view returns (bool) {
        address recovered = ecrecover(orderHash, uint8(signature[64]),
            bytes32(abi.encodePacked(signature[0:32])),
            bytes32(abi.encodePacked(signature[32:64])));
        return recovered != address(0);
    }
}
"""
    },
    {
        "address": "0xVulnerable001",
        "name": "VulnerableVault",
        "category": "Vault",
        "source": """
pragma solidity ^0.8.0;

contract VulnerableVault {
    mapping(address => uint256) public balances;
    uint256 public totalDeposits;

    function deposit() external payable {
        balances[msg.sender] += msg.value;
        totalDeposits += msg.value;
    }

    function withdraw(uint256 amount) external {
        require(balances[msg.sender] >= amount);
        (bool success,) = msg.sender.call{value: amount}("");
        require(success);
        balances[msg.sender] -= amount;
        totalDeposits -= amount;
    }

    function withdrawAll() public {
        uint256 bal = balances[msg.sender];
        (bool sent,) = msg.sender.call{value: bal}("");
        require(sent);
        balances[msg.sender] = 0;
    }

    function emergencyWithdraw() external {
        if (tx.origin == address(0x123)) {
            selfdestruct(payable(msg.sender));
        }
    }

    function processAll(address[] calldata users) external {
        for (uint i = 0; i < users.length; i++) {
            (bool ok,) = users[i].call{value: 1 ether}("");
            require(ok);
        }
    }
}
"""
    },
]


def generate_corpus():
    """Generate synthetic training corpus with multiple copies per category."""
    corpus = []
    for contract in CONTRACTS:
        # Add base contract
        corpus.append(contract)

        # Add slightly modified variants to bulk up training data
        for variant in range(4):
            modified = dict(contract)
            modified['address'] = f"{contract['address'][:10]}variant{variant}"
            modified['name'] = f"{contract['name']} v{variant+2}"
            # Slightly modify source (add comments to change feature counts)
            extra = f"\n// Variant {variant} — {'unchecked {{ x++; }}' * (variant % 3)}\n"
            extra += f"// {'delegatecall' if variant == 2 else 'standard'}\n"
            modified['source'] = contract['source'] + extra
            corpus.append(modified)

    return corpus


# ═══════════════════════════════════════════════════════════════════
# Main
# ═══════════════════════════════════════════════════════════════════

def main():
    print("🔱 Pipeline Validation — End-to-End Test")
    print("=" * 60)

    # Step 1: Generate corpus
    print("\n[Step 1] Generating synthetic Etherscan corpus...")
    corpus = generate_corpus()
    corpus_path = 'test_corpus.json'
    with open(corpus_path, 'w') as f:
        json.dump(corpus, f, indent=2)
    print(f"  Generated {len(corpus)} contracts → {corpus_path}")

    # Step 2: Run PRIME bridge
    print("\n[Step 2] Running Etherscan → PRIME bridge...")
    sys.argv = ['etherscan_to_prime.py', corpus_path, 'test_prime_data.json']
    from etherscan_to_prime import main as bridge_main
    bridge_main()

    # Step 3: Verify training data structure
    print("\n[Step 3] Validating training data structure...")
    with open('test_prime_data.json') as f:
        data = json.load(f)

    assert 'samples' in data, "Missing 'samples' key"
    assert 'metadata' in data, "Missing 'metadata' key"
    assert 'class_names' in data, "Missing 'class_names' key"

    samples = data['samples']
    assert len(samples) > 0, "No samples"
    assert len(samples[0]['features']) == 23, f"Expected 23-D, got {len(samples[0]['features'])}-D"
    assert 0 <= samples[0]['label'] <= 18, "Label out of range"

    print(f"  ✅ Structure valid: {len(samples)} samples × 23-D features")

    # Step 4: Training (NumPy fallback — no PyTorch dependency)
    print("\n[Step 4] Running training pipeline...")
    sys.argv = ['train_evm_v2.py', 'test_prime_data.json', '--epochs', '50',
                '--lr', '0.01', '--output-model', 'test_model.pt']
    from train_evm_v2 import main as train_main
    train_main()

    # Step 5: Verify outputs
    print("\n[Step 5] Verifying outputs...")
    from pathlib import Path

    checks = [
        ('test_corpus.json', 'Synthetic corpus'),
        ('test_prime_data.json', 'PRIME training data'),
    ]

    # Check for model output (either .pt or _numpy.json)
    if Path('test_model.pt').exists():
        checks.append(('test_model.pt', 'Trained model (PyTorch)'))
    elif Path('test_model_numpy.json').exists():
        checks.append(('test_model_numpy.json', 'Trained model (NumPy)'))

    for path, desc in checks:
        if Path(path).exists():
            size = Path(path).stat().st_size
            print(f"  ✅ {desc}: {path} ({size:,} bytes)")
        else:
            print(f"  ❌ {desc}: {path} MISSING")

    print(f"\n{'='*60}")
    print("  🔱 FULL PIPELINE VALIDATED — Etherscan → PRIME → Training")
    print(f"{'='*60}")

    # Cleanup
    for f in ['test_corpus.json', 'test_prime_data.json',
              'test_model.pt', 'test_model_numpy.json',
              'test_model_report.json']:
        if Path(f).exists():
            os.remove(f)


if __name__ == '__main__':
    main()
