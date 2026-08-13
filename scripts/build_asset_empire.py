import json
import os
from pathlib import Path
from implementation.services.asset_service.asset_discovery_fabric import AssetDiscoveryFabric
from implementation.services.asset_service.asset_deduplicator import AssetDeduplicator
from implementation.services.asset_service.asset_quality_ranker import AssetQualityRanker

def main():
    print("👑 [Asset Empire Builder] Starting Autonomous Asset Empire Construction Loop...")
    
    # 1. Discover 100,000 candidates
    discovery = AssetDiscoveryFabric()
    raw_candidates = discovery.discover_assets(count=100000)
    
    # 2. Deduplicate
    dedup = AssetDeduplicator()
    deduped = dedup.deduplicate(raw_candidates)
    
    # 3. Quality rank & filter
    ranker = AssetQualityRanker(max_vertices=120000, max_file_size_mb=40.0)
    final_empire = ranker.rank_and_filter(deduped)
    
    # 4. Save to output/asset_empire.json
    output_dir = Path("output")
    output_dir.mkdir(parents=True, exist_ok=True)
    
    report_path = output_dir / "asset_empire.json"
    
    # We save a registry overview/index and top 1000 highest quality detailed entries
    # to avoid huge disk I/O bottleneck, while keeping the count statistic and active lists.
    empire_payload = {
        "total_empire_count": len(final_empire),
        "scanning_parameters": {
            "max_vertices": ranker.max_vertices,
            "max_size_mb": ranker.max_file_size_mb
        },
        "top_quality_candidates": final_empire[:1000]
    }
    
    with open(report_path, "w", encoding="utf-8") as f:
        json.dump(empire_payload, f, indent=2)
        
    print(f"✅ [Asset Empire Builder] Registry saved. Recorded total {len(final_empire)} verified assets.")
    print(f"📊 Empire file written to: {report_path}")

if __name__ == "__main__":
    main()
