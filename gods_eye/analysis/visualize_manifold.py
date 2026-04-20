import json
import numpy as np
import plotly.graph_objects as go
from sentence_transformers import SentenceTransformer
from sklearn.decomposition import PCA
from sklearn.cluster import KMeans

def main():
    print("Initializing ZKAEDI PRIME Manifold Visualizer...")
    try:
        with open("zcc-compiler-bug-corpus.json", "r", encoding="utf-8") as f:
            bugs = json.load(f)
    except Exception as e:
        print(f"Failed to load corpus: {e}")
        return

    # Extract Text Data
    bug_ids = [b['id'] for b in bugs]
    bug_titles = [f"{b['cwe']}: {b['title']}" for b in bugs]
    hover_texts = [
        f"<b>{b['id']}</b><br>{b['cwe']}<br><i>Phase: {b.get('prime_phase', 'Unknown')}</i><br>Score: {b.get('prime_energy_score', 'N/A')}<br><br>{b['title'][:80]}..."
        for b in bugs
    ]

    print("Embedding bug topology... (all-MiniLM-L6-v2)")
    encoder = SentenceTransformer("all-MiniLM-L6-v2")
    raw_text_for_embed = [f"[{b['cwe']}] {b['title']}: {b['root_cause']}" for b in bugs]
    embeddings = encoder.encode(raw_text_for_embed, show_progress_bar=False)

    print("Executing Dimensionality Reduction...")
    # PCA to 3D Field
    pca = PCA(n_components=3)
    pos_3d = pca.fit_transform(embeddings)

    # Basic clustering to simulate Domains / Attractors
    kmeans = KMeans(n_clusters=3, random_state=42)
    clusters = kmeans.fit_predict(embeddings)

    print("Generating 3D Topological Matrix HTML...")
    fig = go.Figure(data=[go.Scatter3d(
        x=pos_3d[:, 0],
        y=pos_3d[:, 1],
        z=pos_3d[:, 2],
        mode='markers+text',
        text=bug_ids,
        hovertext=hover_texts,
        hoverinfo="text",
        marker=dict(
            size=[15]*len(bugs),
            color=clusters,
            colorscale='Magma',
            opacity=0.8,
            line=dict(width=2, color='#1f1f1f')
        ),
        textfont=dict(
            color='cyan',
            size=10
        )
    )])

    fig.update_layout(
        title="ZKAEDI PRIME Bug Topology (Hamiltonian Space)",
        scene=dict(
            xaxis_title="Vector PC1",
            yaxis_title="Vector PC2",
            zaxis_title="Vector PC3",
            bgcolor="#0a0a0f",
            xaxis=dict(showgrid=False, zeroline=False, visible=False),
            yaxis=dict(showgrid=False, zeroline=False, visible=False),
            zaxis=dict(showgrid=False, zeroline=False, visible=False),
        ),
        paper_bgcolor="#0a0a0f",
        font=dict(color='cyan')
    )

    out_file = "ZKAEDI_BUG_TOPOLOGY_3D.html"
    fig.write_html(out_file)
    print(f"Topology Map rendered to: {out_file}")

if __name__ == "__main__":
    main()
